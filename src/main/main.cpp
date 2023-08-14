
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
#include "options.h"

#undef main

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

static inline void usage(const std::wstring& wsProgName)
{
  // Output command line parameter.
  std::wcout << wsProgName
             << " <rtsp url>"
             << " <output audio device index>"
             << " <sync type>"
             << " <scan all pmts>"
             << " <rtsp transport>"
             << " <max delay>"
             << " <stimeout>"
             << " <buffer size>"
             << std::endl;
  std::wcout << "i.e.," << std::endl;
  std::wcout << wsProgName << " rtsp://username:password@IP_Address:554/ch1 1 0 0 0 0 0 10000" << std::endl << std::endl;

  std::wcout << "----- sync type -----" << std::endl;
  std::wcout << "0 : sync audio clock. Default value." << std::endl;
  std::wcout << "1 : sync video clock." << std::endl;
  std::wcout << "2 : sync external clock." << std::endl << std::endl;

  std::wcout << "----- scan all pmts -----" << std::endl;
  std::wcout << "0 : OFF. Default value." << std::endl;
  std::wcout << "1 : ON." << std::endl << std::endl;

  std::wcout << "----- rtsp transport -----" << std::endl;
  std::wcout << "0 : Not set. Default value(udp)." << std::endl;
  std::wcout << "1 : TCP." << std::endl << std::endl;

  std::wcout << "----- max delay -----" << std::endl;
  std::wcout << "value : Integer. i.e, 3000 etc." << std::endl << std::endl;

  std::wcout << "----- stimeout -----" << std::endl;
  std::wcout << "value : Integer. i.e, 1000 etc." << std::endl << std::endl;

  std::wcout << "----- buffer size -----" << std::endl;
  std::wcout << "value : Integer. i.e, 20000 etc." << std::endl << std::endl;

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

int main(int argc, char *argv[])
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

  std::string progName = std::string(argv[0]);
  std::wstring wsProgName = UTF8ToUnicode(progName);
  if (argc < 3)
  {
    usage(wsProgName);
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
    usage(wsProgName);
    return -1;
  }

  // Sync type
  if (argc > 3)
  {
    opt.syncType = std::stoi(argv[3]);
    if (opt.syncType < 0 || opt.syncType > 3)
    {
      std::cerr << "Failed to set sync type." << std::endl;
      usage(wsProgName);
      return -1;
    }
  }

  // Scan all pmts
  if (argc > 4)
  {
    opt.scanAllPmts = std::stoi(argv[4]);
    if (opt.scanAllPmts < 0 || opt.scanAllPmts > 1)
    {
      std::cerr << "Failed to set scan all pmts." << std::endl;
      usage(wsProgName);
      return -1;
    }
  }

  // rtsp transport
  if (argc > 5)
  {
    opt.rtspTransport = std::stoi(argv[5]);
    if (opt.rtspTransport < 0 || opt.rtspTransport > 1)
    {
      std::cerr << "Failed to set rtsp transports." << std::endl;
      usage(wsProgName);
      return -1;
    }
  }

  // max delay
  if (argc > 6)
  {
    opt.maxDelay = std::stoi(argv[7]);
    if (opt.maxDelay < 0)
    {
      std::cerr << "Failed to set max delay." << std::endl;
      usage(wsProgName);
      return -1;
    }
  }

  // stimeout
  if (argc > 7)
  {
    opt.stimeout = std::stoi(argv[8]);
    if (opt.stimeout < 0)
    {
      std::cerr << "Failed to set stimeout." << std::endl;
      usage(wsProgName);
      return -1;
    }
  }

  // buffer size
  if (argc > 8)
  {
    opt.bufferSize = std::stoi(argv[9]);
    if (opt.bufferSize < 0)
    {
      std::cerr << "Failed to set buffer size." << std::endl;
      usage(wsProgName);
      return -1;
    }
  }

  // Create filename
  std::string filename = std::string(argv[1]);

  // Create VideoState
  std::shared_ptr<VideoState> videoState = std::make_shared<VideoState>();
  videoState->filename = filename;
  videoState->av_sync_type = (SYNC_TYPE)opt.syncType;

  // Create VideoReader
  std::unique_ptr<VideoReader> videoReader = std::make_unique<VideoReader>();
  videoReader->start(videoState.get(), opt);
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
