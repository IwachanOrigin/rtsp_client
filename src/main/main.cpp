
#include <memory>
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <cstdlib>

#include "videoreader.h"
#include "stringhelper.h"

#pragma comment(lib, "avcodec")
#pragma comment(lib, "avdevice")
#pragma comment(lib, "avfilter")
#pragma comment(lib, "avformat")
#pragma comment(lib, "avutil")
#pragma comment(lib, "swresample")
#pragma comment(lib, "swscale")
#pragma comment(lib, "SDL2")

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

static inline void usage()
{
  // Output command line parameter.
  std::wcout << __wargv[0]
             << " <file path / url>"
             << " <output audio device index>"
             << std::endl;
  std::wcout << "i.e.," << std::endl;
  std::wcout << __wargv[0] << " /path/to/movie.mp4 1" << std::endl << std::endl;

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

  if (argc != 3)
  {
    usage();
    return -1;
  }

  std::vector<std::wstring> vecAudioOutDevNames;
  int deviceNum = getOutputAudioDeviceList(vecAudioOutDevNames);
  int outputAudioDevIndex = std::stoi(argv[2]);
  if (deviceNum < outputAudioDevIndex)
  {
    std::cerr << "Failed to input audio output device number." << std::endl;
    usage();
    return -1;
  }

  std::unique_ptr<VideoReader> videoReader = std::make_unique<VideoReader>();
  std::wstring wsFilename = std::wstring(argv[1]);
  std::string filename = wstringToString(wsFilename);
  videoReader->start(filename, outputAudioDevIndex);
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
