
extern "C"
{
#include <SDL.h>
}

#include <iostream>
#include "rtspcontroller.h"

#undef main

int main(int argc, char* argv[])
{
  if (argc < 2)
  {
    std::cerr << argv[0] << " <rtsp url 1> ... <rtsp url N>" << std::endl;
    return -1;
  }

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
  {
    return -1;
  }

  client::RtspController controller;
  for (int i = 1; i <= argc - 1; i++)
  {
    controller.openURL(argv[0], argv[i]);
  }

  // Start to Live555 event loop.
  controller.eventloop();

  // Quit
  SDL_VideoQuit();
  SDL_AudioQuit();
  SDL_Quit();

  return 0;
}
