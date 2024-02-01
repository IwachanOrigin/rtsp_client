
#include "videorenderer.h"
#include <cassert>
#include <iostream>

#define FF_REFRESH_EVENT (SDL_USEREVENT)
#define FF_QUIT_EVENT    (SDL_USEREVENT + 1)

using namespace client;

VideoRenderer::VideoRenderer()
{
}

VideoRenderer::~VideoRenderer()
{
  if (m_renderer)
  {
    SDL_DestroyRenderer(m_renderer);
    m_renderer = nullptr;
  }

  if (m_texture)
  {
    SDL_DestroyTexture(m_texture);
    m_texture = nullptr;
  }

  if (m_mutex)
  {
    SDL_DestroyMutex(m_mutex);
    m_mutex = nullptr;
  }

  if (m_window)
  {
    SDL_DestroyWindow(m_window);
    m_window = nullptr;
  }
}

bool VideoRenderer::init(const int& x, const int& y, const int& w, const int& h)
{
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER))
  {
    return false;
  }

  m_window = SDL_CreateWindow("RTSP Client", x, y, w, h, 0);
  assert(m_window);
  if (!m_window)
  {
    return false;
  }

  m_renderer = SDL_CreateRenderer(m_window, -1, 0);
  assert(m_renderer);
  if (!m_renderer)
  {
    return false;
  }

  m_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, 2560, 1440);
  assert(m_texture);
  if (!m_texture)
  {
    return false;
  }

  m_mutex = SDL_CreateMutex();
  assert(m_mutex);
  if (!m_mutex)
  {
    return false;
  }

  return true;
}

int VideoRenderer::start()
{
  // Start thread.
  return 0;
}

void VideoRenderer::stop()
{
}

int VideoRenderer::displayThread()
{
  SDL_Event event;
  int ret = -1;

  this->scheduleRefresh(100);

  for (;;)
  {
    double incr = 0, pos = 0;
    // wait indefinitely for the next available event
    ret = SDL_WaitEvent(&event);
    if (ret == 0)
    {
      std::cerr << "SDL_WaitEvent failed : " << SDL_GetError() << std::endl;
    }

    // switch on the retrieved event type
    switch (event.type)
    {
      case SDL_KEYDOWN:
      {
        switch (event.key.keysym.sym)
        {
          case SDLK_LEFT:
          {
            incr = -10.0;
            goto do_seek;
          }
          break;

          case SDLK_RIGHT:
          {
            incr = 10.0;
            goto do_seek;
          }
          break;

          case SDLK_DOWN:
          {
            incr = -60.0;
            goto do_seek;
          }
          break;

          case SDLK_UP:
          {
            incr = 60.0;
            goto do_seek;
          }
          break;

          do_seek:
          {
            // Seek
          }
          break;

          default:
          {
            // No action
          }
          break;
        }
      }
      break;

      case FF_QUIT_EVENT:
      case SDL_QUIT:
      {
        // Finish
      }
      break;

      case FF_REFRESH_EVENT:
      {
        this->videoRefreshTimer();
      }
      break;
    }
  }

  return 0;
}

void VideoRenderer::scheduleRefresh(int delay)
{
  // schedule an sdl timer
  int ret = SDL_AddTimer(delay, this->sdlRefreshTimerCb, nullptr);
  if (ret == 0)
  {
    std::cerr << "could not schedule refresh callback : " << SDL_GetError() << std::endl;
  }
}

void VideoRenderer::videoRefreshTimer()
{
}

Uint32 VideoRenderer::sdlRefreshTimerCb(Uint32 interval, void* param)
{
  // create an sdl event of type
  SDL_Event event;
  event.type = FF_REFRESH_EVENT;
  event.user.data1 = param;

  // push the event to the events queue
  SDL_PushEvent(&event);

  return 0;
}

