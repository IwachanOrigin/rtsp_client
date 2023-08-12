
#include <memory>
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <cstdlib>

#include "videostate.h"
#include "videoreader.h"
#include "stringhelper.h"

struct Options
{
  int audioIndex = 0;
  int syncType = 0;
  int scanAllPmts = 0;
  int rtspTransport = 0;
  int maxDelay = 0;
  int stimeout = 0;
  int bufferSize = 0;
};

static inline int getOutputAudioDeviceList(std::vector<std::wstring> &vec)
{
  int deviceNum = SDL_GetNumAudioDevices(0);
  for (int i = 0; i < deviceNum; i++)
  {
    const char* audioDeviceName = SDL_GetAudioDeviceName(i, 0);
    std::string mcAudioDeviceName = std::string(audioDeviceName);
    std::wstring wcAudioDeviceName = UTF8ToUnicode(mcAudioDeviceName);
    vec.push_back(wcAudioDeviceName);
  }
  return deviceNum;
}

static inline void usage()
{
  // Output command line parameter.
  std::wcout << __wargv[0]
             << " <file path / url>"
             << " <output audio device index>"
             << " <sync type>"
             << std::endl;
  std::wcout << "i.e.," << std::endl;
  std::wcout << __wargv[0] << " /path/to/movie.mp4 1 0" << std::endl << std::endl;

  std::wcout << "----- sync type -----" << std::endl;
  std::wcout << "0 : sync audio clock." << std::endl;
  std::wcout << "1 : sync video clock." << std::endl;
  std::wcout << "2 : sync external clock." << std::endl << std::endl;

  // Get audio output devices.
  std::vector<std::wstring> vecAudioOutDevNames;
  std::wcout << "----- Audio Output Devices -----" << std::endl;
  getOutputAudioDeviceList(vecAudioOutDevNames);
  if (vecAudioOutDevNames.empty())
  {
    std::wcerr << "Failed to get audio output devices." << std::endl;
    return;
  }
  for (uint32_t i = 0; i < vecAudioOutDevNames.size(); i++)
  {
    std::wcout << "No. " << i << " : " << vecAudioOutDevNames[i] << std::endl;
  }
}

void myLogCallback(void* ptr, int level, const char* fmt, va_list vargs)
{
  vprintf(fmt, vargs);
}

int wmain(int argc, wchar_t *argv[])
{
  // Set locale(use to the system default locale)
  std::wcout.imbue(std::locale(""));

  // init SDL
  int ret = -1;
  ret = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
  if (ret != 0)
  {
    std::cerr << "Could not initialize SDL" << SDL_GetError() << std::endl;
    return -1;
  }

  if (argc < 3)
  {
    usage();
    return -1;
  }

#if 0
  // Enable console logging for the ffmpeg api.
  av_log_set_level(AV_LOG_DEBUG);
  av_log_set_callback(myLogCallback);
#endif

  Options opt;

  // Output audio device.
  std::vector<std::wstring> vecAudioOutDevNames;
  int deviceNum = getOutputAudioDeviceList(vecAudioOutDevNames);
  opt.audioIndex = std::stoi(argv[2]);
  if (deviceNum < opt.audioIndex)
  {
    std::cerr << "Failed to input audio output device number." << std::endl;
    usage();
    return -1;
  }

  // Sync type
  if (argc >= 3)
  {
    opt.syncType = std::stoi(argv[3]);
    if (opt.syncType < 0 || opt.syncType > 3)
    {
      std::cerr << "Failed to set sync type." << std::endl;
      usage();
      return -1;
    }
  }

  // Create filename
  std::wstring wsFilename = std::wstring(argv[1]);
  std::string filename = wstringToString(wsFilename);

  // Create VideoState
  std::shared_ptr<VideoState> videoState = std::make_shared<VideoState>();
  videoState->filename = filename;
  videoState->av_sync_type = (SYNC_TYPE)opt.syncType;

  // Create VideoReader
  std::unique_ptr<VideoReader> videoReader = std::make_unique<VideoReader>();
  videoReader->start(videoState.get(), opt.audioIndex);
  while(1)
  {
    std::chrono::milliseconds duration(1000);
    std::this_thread::sleep_for(duration);
    if (videoReader->quitStatus())
    {
      break;
    }
  }

  std::wcout << "finished." << std::endl;
  return 0;
}
