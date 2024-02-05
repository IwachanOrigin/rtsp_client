
#include "videorenderer.h"
#include <cassert>
#include <iostream>
#include <thread>

#define FF_REFRESH_EVENT (SDL_USEREVENT)
#define FF_QUIT_EVENT    (SDL_USEREVENT + 1)

using namespace client;

VideoRenderer::VideoRenderer()
{
  m_frameContainer = std::make_shared<FrameContainer>();
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

bool VideoRenderer::init(const int& x, const int& y, const int& w, const int& h, const std::vector<std::string>& vecURL)
{
  {
    //int flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS | SDL_WINDOW_TOOLTIP;
    int flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE;
    m_window = SDL_CreateWindow("RTSP Client", x, y, w, h, flags);
    assert(m_window);
    if (!m_window)
    {
      return false;
    }
    SDL_GL_SetSwapInterval(1);
  }

  m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
  assert(m_renderer);
  if (!m_renderer)
  {
    return false;
  }

  m_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, w, h);
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

  m_rtspController.setFrameContainer(m_frameContainer);

  for (auto& url : vecURL)
  {
    m_rtspController.openURL("rtspClient", url.data());
  }

  std::thread([](RtspController* controller)
    {
      controller->eventloop();
    }, &m_rtspController).detach();

  return true;
}

int VideoRenderer::render()
{
  SDL_Event sdlEvent{0};
  int ret = -1;

  double incr = 0, pos = 0;
  // Wait indefinitely for the next available event
  ret = SDL_PollEvent(&sdlEvent);

  // Switch on the retrieved event type
  switch (sdlEvent.type)
  {
    case SDL_KEYDOWN:
    {
      switch (sdlEvent.key.keysym.sym)
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

    case SDL_QUIT:
    {
      m_rtspController.setStopStreaming();
      return -1;
    }
    break;
  }

  this->videoDisplay();

  if (m_rtspController.isRtspClientFinished())
  {
    return -1;
  }

  return 0;
}

int VideoRenderer::videoDisplay()
{
  // Check the videopicture queue contains decoded frames
  if (m_frameContainer->sizeVideoFrameDecoded() == 0)
  {
    return -1;
  }

  // Get videopicture reference using the queue read index
  AVFrame* frame = av_frame_alloc();
  if (!frame)
  {
    std::cerr << "Could not allocate AVFrame" << std::endl;
    return -1;
  }
  auto isFrame = m_frameContainer->popVideoFrameDecoded(frame);
  if (isFrame < 0)
  {
    std::cerr << "Could not get AVFrame" << std::endl;
    // wipe the frame
    av_frame_free(&frame);
    av_free(frame);
    return -1;
  }

  // Lock screen mutex
  SDL_LockMutex(m_mutex);

  // Update the texture with the new pixel data
  SDL_UpdateYUVTexture(
    m_texture
    , nullptr
    , frame->data[0]
    , frame->linesize[0]
    , frame->data[1]
    , frame->linesize[1]
    , frame->data[2]
    , frame->linesize[2]
    );

  // Clear the current rendering target with the drawing color
  SDL_RenderClear(m_renderer);

  // Copy a portion of the texture to the current rendering target
  SDL_RenderCopy(m_renderer, m_texture, nullptr, nullptr);

  // Update the screen with any rendering performed since the previous call
  SDL_RenderPresent(m_renderer);

  // Unlock screen mutex
  SDL_UnlockMutex(m_mutex);

  // Release
  av_frame_unref(frame);
  av_frame_free(&frame);

  return 0;
}

