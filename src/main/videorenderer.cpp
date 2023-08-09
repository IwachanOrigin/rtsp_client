
#if (WIN32)
#include <windows.h>
#endif
#include <iostream>
#include <thread>
#include "videorenderer.h"

#define FF_REFRESH_EVENT (SDL_USEREVENT)
#define FF_QUIT_EVENT    (SDL_USEREVENT + 1)

// av sync correction is done if the clock difference is above the max av sync threshold
#define AV_SYNC_THRESHOLD 0.01

// no av sync correction is done if the clock difference is below the minimum av sync shreshold
#define AV_NOSYNC_THRESHOLD 1.0

VideoRenderer::VideoRenderer()
  : m_videoState(nullptr)
  , m_screen(nullptr)
{
}

VideoRenderer::~VideoRenderer()
{
}

int VideoRenderer::start(VideoState *videoState)
{
  m_videoState = videoState;
  if (m_videoState)
  {
    std::thread([&](VideoRenderer *vr)
      {
        vr->displayThread();

      }, this).detach();
  }
  else
  {
    return -1;
  }

  return 0;
}

int VideoRenderer::displayThread()
{
  SDL_Event event;
  int ret = -1;

  this->scheduleRefresh(100);

  for (;;)
  {
    double incr = 0, pos = 0;
    // Wait indefinitely for the next available event
    ret = SDL_WaitEvent(&event);
    if (ret == 0)
    {
      std::cerr << "SDL_WaitEvent failed : " << SDL_GetError() << std::endl;
    }

    // Switch on the retrieved event type
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
          if (m_videoState)
          {
            pos = m_videoState->get_master_clock();
            pos += incr;
            m_videoState->stream_seek((int64_t)(pos * AV_TIME_BASE), incr);
          }
        }
        break;

        default:
        {
          // nothing
        }
        break;
      }
    }
    break;

    case FF_QUIT_EVENT:
    case SDL_QUIT:
    {
      SDL_CondSignal(m_videoState->audioq.cond);
      SDL_CondSignal(m_videoState->videoq.cond);
      m_videoState->quit = 1;
    }
    break;

    case FF_REFRESH_EVENT:
    {
      this->videoRefreshTimer();
    }
    break;

    default:
    {
      // nothing
    }
    break;
    }

    // Check global quit flag
    if (m_videoState->quit)
    {
      // Exit for loop
      break;
    }
  }

  SDL_DestroyTexture(m_videoState->texture);
  m_videoState->texture = nullptr;
  SDL_DestroyRenderer(m_videoState->renderer);
  m_videoState->renderer = nullptr;
  SDL_DestroyWindow(m_screen);
  m_screen = nullptr;
  m_videoState->quit = 1;
  m_videoState = nullptr;

  return 0;
}

void VideoRenderer::scheduleRefresh(int delay)
{
  // Schedule an sdl timer
  int ret = SDL_AddTimer(delay, this->sdlRefreshTimerCB, m_videoState);
  if (ret == 0)
  {
    std::cerr << "could not schedule refresh callback : " << SDL_GetError() << std::endl;
  }
}

void VideoRenderer::videoRefreshTimer()
{
  // We will later see how to property use this
  VideoPicture *videoPicture;

  // Used for video frames display delay and audio video sync
  double pts_delay = 0;
  double ref_clock = 0;
  double sync_threshold = 0;
  double real_delay = 0;
  double audio_video_delay = 0;
  double diff = 0;

  // Check the video stream was correctly opened
  if (m_videoState->video_st)
  {
    // Check the videopicture queue contains decoded frames
    if (m_videoState->pictq_size == 0)
    {
      this->scheduleRefresh(1);
    }
    else
    {
      // Get videopicture reference using the queue read index
      videoPicture = &m_videoState->pictq[m_videoState->pictq_rindex];

      // Get last frame pts
      pts_delay = videoPicture->pts - m_videoState->frame_last_pts;

      // If the obtained delay is incorrect
      if (pts_delay <= 0 || pts_delay >= 1.0)
      {
        // Use the previously calculated delay
        pts_delay = m_videoState->frame_last_delay;
      }

      // Save delay information for the next time
      m_videoState->frame_last_delay = pts_delay;
      m_videoState->frame_last_pts = videoPicture->pts;

#if 1
      // Update delay to sync to audio if not master source
      if (m_videoState->av_sync_type != SYNC_TYPE::AV_SYNC_VIDEO_MASTER)
      {
        ref_clock = m_videoState->get_master_clock();
        diff = videoPicture->pts - ref_clock;

        // Skip or repeat the frame taking into account the delay
        sync_threshold = (pts_delay > AV_SYNC_THRESHOLD) ? pts_delay : AV_SYNC_THRESHOLD;
        //std::cout << "sync threshold : " << sync_threshold << std::endl;

        // Check audio video delay absolute value is below sync threshold
        if (fabs(audio_video_delay) < AV_NOSYNC_THRESHOLD)
        {
          if (audio_video_delay <= -sync_threshold)
          {
            pts_delay = 0;
          }
          else if (audio_video_delay >= sync_threshold)
          {
            pts_delay = 2 * pts_delay;
          }
        }
        //std::cout << "corrected pts delay : " << pts_delay << std::endl;
      }
#else
      sync_threshold = AV_SYNC_THRESHOLD;
      pts_delay = 0;
#endif

      m_videoState->frame_timer += pts_delay;
      // Compute the real delay
      real_delay = m_videoState->frame_timer - (av_gettime() / 1000000.0);
      //std::cout << "real delay : " << real_delay << std::endl;
      if (real_delay < 0.010)
      {
        real_delay = 0.010;
      }
      //std::cout << "corrected real delay : " << real_delay << std::endl;

      this->scheduleRefresh((int)(real_delay * 1000 + 0.5));
      //std::cout << "next schedule refresh : " << (int)(real_delay * 1000 + 0.5) << std::endl;

      // Show the frame on the sdl_surface
      this->videoDisplay();

      // Update read index for the next frame
      if (++m_videoState->pictq_rindex == VIDEO_PICTURE_QUEUE_SIZE)
      {
        m_videoState->pictq_rindex = 0;
      }

      // Lock videopicture queue mutex
      SDL_LockMutex(m_videoState->pictq_mutex);

      // Decrease videopicture queue size
      m_videoState->pictq_size--;

      // Notify other threads waiting for the videoPicture queue
      SDL_CondSignal(m_videoState->pictq_cond);

      // Unlock videoPicture queue mutex
      SDL_UnlockMutex(m_videoState->pictq_mutex);
    }
  }
  else
  {
    this->scheduleRefresh(100);
  }
}

Uint32 VideoRenderer::sdlRefreshTimerCB(Uint32 interval, void *param)
{
  // Create an sdl event of type
  SDL_Event event;
  event.type = FF_REFRESH_EVENT;
  event.user.data1 = param;

  // Push the event to the events queue
  SDL_PushEvent(&event);

  return 0;
}

void VideoRenderer::videoDisplay()
{
  // Create window, renderer and textures if not already created
  if (!m_screen)
  {
    //int flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS | SDL_WINDOW_TOOLTIP;
    int flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE;
    m_screen = SDL_CreateWindow(
      "display"
      , SDL_WINDOWPOS_UNDEFINED
      , SDL_WINDOWPOS_UNDEFINED
      , m_videoState->video_ctx->width / 2
      , m_videoState->video_ctx->height / 2
      , flags
      );
    SDL_GL_SetSwapInterval(1);
  }

  // Check window was correctly created
  if (!m_screen)
  {
    std::cerr << "SDL : could not create window - exiting" << std::endl;
    return;
  }

  if (!m_videoState->renderer)
  {
    // Create a 2d rendering context for the sdl_window
    m_videoState->renderer = SDL_CreateRenderer(
      m_screen
      , -1
      , SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE
      );
  }

  if (!m_videoState->texture)
  {
    // Create a texture for a rendering context
    m_videoState->texture = SDL_CreateTexture(
      m_videoState->renderer
      , SDL_PIXELFORMAT_YV12
      , SDL_TEXTUREACCESS_STREAMING
      , m_videoState->video_ctx->width
      , m_videoState->video_ctx->height
      );
  }
  // Reference for the next videopicture to be displayed
  VideoPicture *videoPicture = nullptr;
  float aspect_ratio = 0;
  int w = -1, h = -1, x = -1, y = -1;

  // Get next videoPicture to be displayed from the videopicture queue
  videoPicture = &m_videoState->pictq[m_videoState->pictq_rindex];
  if (videoPicture->frame)
  {
    if (m_videoState->video_ctx->sample_aspect_ratio.num == 0)
    {
      aspect_ratio = 0;
    }
    else
    {
      aspect_ratio = av_q2d(m_videoState->video_ctx->sample_aspect_ratio)
        * m_videoState->video_ctx->width / m_videoState->video_ctx->height;
    }

    if (aspect_ratio <= 0.0)
    {
      aspect_ratio = (float)m_videoState->video_ctx->width
        / (float)m_videoState->video_ctx->height;
    }

    // Get the size of a window's client area
    int screen_width = -1;
    int screen_height = -1;
    SDL_GetWindowSize(m_screen, &screen_width, &screen_height);

    // Global sdl_surface height
    h = screen_height;

    // Retrieve width using the calculated aspect ratio and the screen height
    w = ((int) rint(h * aspect_ratio)) & -3;

    // If the new width is bigger than the screen width
    if (w > screen_width)
    {
      // Set the width to the screen width
      w = screen_width;

      // Recalculate height using the calculated aspect ratio and screen width
      h = ((int) rint(w / aspect_ratio)) & -3;
    }

    x = (screen_width - w);
    y = (screen_height - h);

    // Check the number of frames to decode was not exceeded
    {
      // Set blit area x and y coordinates, width and height
      SDL_Rect rect{0};
      rect.x = x;
      rect.y = y;
      rect.w = 2 * w;
      rect.h = 2 * h;

      // Lock screen mutex
      SDL_LockMutex(m_videoState->screen_mutex);

      // Update the texture with the new pixel data
      SDL_UpdateYUVTexture(
        m_videoState->texture
        , &rect
        , videoPicture->frame->data[0]
        , videoPicture->frame->linesize[0]
        , videoPicture->frame->data[1]
        , videoPicture->frame->linesize[1]
        , videoPicture->frame->data[2]
        , videoPicture->frame->linesize[2]
        );

      // Clear the current rendering target with the drawing color
      SDL_RenderClear(m_videoState->renderer);

      // Copy a portion of the texture to the current rendering target
      SDL_RenderCopy(m_videoState->renderer, m_videoState->texture, &rect, nullptr);

      // Update the screen with any rendering performed since the previous call
      SDL_RenderPresent(m_videoState->renderer);

      // Unlock screen mutex
      SDL_UnlockMutex(m_videoState->screen_mutex);
    }
  }
}
