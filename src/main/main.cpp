
extern "C"
{
#include <SDL.h>
}

#include <iostream>
#include <chrono>
#include <thread>
#include "videorenderer.h"

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

  std::vector<std::string> vecURL;
  for (int i = 1; i < argc; i++)
  {
    vecURL.push_back(argv[i]);
  }

  std::chrono::milliseconds ms(1);
  client::VideoRenderer renderer;
  renderer.init(50, 50, 1920, 1080, vecURL);
  while(1)
  {
    auto ret = renderer.render();
    if (ret < 0)
    {
      break;
    }
    std::this_thread::sleep_for(ms);
  }

  // Quit
  SDL_VideoQuit();
  SDL_AudioQuit();
  SDL_Quit();

  return 0;
}
