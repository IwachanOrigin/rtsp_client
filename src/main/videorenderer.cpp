
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
        vr->display_thread();

      }, this).detach();
  }
  else
  {
    return -1;
  }

  return 0;
}

int VideoRenderer::display_thread()
{
  SDL_Event event;
  int ret = -1;

  this->schedule_refresh(100);

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
        if (m_videoState)
        {
          pos = m_videoState->get_master_clock();
          pos += incr;
          m_videoState->stream_seek((int64_t)(pos * AV_TIME_BASE), incr);
        }
        break;
      };

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
      this->video_refresh_timer();
    }
    break;

    default:
    {
      // nothing
    }
    break;
    }

    // check global quit flag
    if (m_videoState->quit)
    {
      // exit for loop
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

void VideoRenderer::schedule_refresh(int delay)
{
  // schedule an sdl timer
  int ret = SDL_AddTimer(delay, this->sdl_refresh_timer_cb, m_videoState);
  if (ret == 0)
  {
    std::cerr << "could not schedule refresh callback : " << SDL_GetError() << std::endl;
  }
}

void VideoRenderer::video_refresh_timer()
{
  // we will later see how to property use this
  VideoPicture *videoPicture;

  // used for video frames display delay and audio video sync
  double pts_delay = 0;
  double audio_ref_clock = 0;
  double sync_threshold = 0;
  double real_delay = 0;
  double audio_video_delay = 0;

  // check the video stream was correctly opened
  if (m_videoState->video_st)
  {
    // check the videopicture queue contains decoded frames
    if (m_videoState->pictq_size == 0)
    {
      this->schedule_refresh(1);
    }
    else
    {
      // get videopicture reference using the queue read index
      videoPicture = &m_videoState->pictq[m_videoState->pictq_rindex];

      // get last frame pts
      pts_delay = videoPicture->pts - m_videoState->frame_last_pts;

      // if the obtained delay is incorrect
      if (pts_delay <= 0 || pts_delay >= 1.0)
      {
        // use the previously calculated delay
        pts_delay = m_videoState->frame_last_delay;
      }

      // save delay information for the next time
      m_videoState->frame_last_delay = pts_delay;
      m_videoState->frame_last_pts = videoPicture->pts;

      // update delay to stay in sync with the audio
      audio_ref_clock = this->get_audio_clock();

      audio_video_delay = videoPicture->pts - audio_ref_clock;

      // skip or repeat the frame taking into account the delay
      sync_threshold = (pts_delay > AV_SYNC_THRESHOLD) ? pts_delay : AV_SYNC_THRESHOLD;
      //std::cout << "sync threshold : " << sync_threshold << std::endl;

      // check audio video delay absolute value is below sync threshold
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

      m_videoState->frame_timer += pts_delay;
      // compute the real delay
      real_delay = m_videoState->frame_timer - (av_gettime() / 1000000.0);
      //std::cout << "real delay : " << real_delay << std::endl;
      if (real_delay < 0.010)
      {
        real_delay = 0.010;
      }
      //std::cout << "corrected real delay : " << real_delay << std::endl;

      this->schedule_refresh((int)(real_delay * 1000 + 0.5));
      //std::cout << "next schedule refresh : " << (int)(real_delay * 1000 + 0.5) << std::endl;

      // show the frame on the sdl_surface
      this->video_display();

      // update read index for the next frame
      if (++m_videoState->pictq_rindex == VIDEO_PICTURE_QUEUE_SIZE)
      {
        m_videoState->pictq_rindex = 0;
      }

      // lock videopicture queue mutex
      SDL_LockMutex(m_videoState->pictq_mutex);

      // decrease videopicture queue size
      m_videoState->pictq_size--;

      // notify other threads waiting for the videoPicture queue
      SDL_CondSignal(m_videoState->pictq_cond);

      // unlock videoPicture queue mutex
      SDL_UnlockMutex(m_videoState->pictq_mutex);
    }
  }
  else
  {
    this->schedule_refresh(100);
  }
}

Uint32 VideoRenderer::sdl_refresh_timer_cb(Uint32 interval, void *param)
{
  // create an sdl event of type
  SDL_Event event;
  event.type = FF_REFRESH_EVENT;
  event.user.data1 = param;

  // push the event to the events queue
  SDL_PushEvent(&event);

  return 0;
}

void VideoRenderer::video_display()
{
  // create window, renderer and textures if not already created
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

  // check window was correctly created
  if (!m_screen)
  {
    std::cerr << "SDL : could not create window - exiting" << std::endl;
    return;
  }

  if (!m_videoState->renderer)
  {
    // create a 2d rendering context for the sdl_window
    m_videoState->renderer = SDL_CreateRenderer(
      m_screen
      , -1
      , SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE
      );
  }

  if (!m_videoState->texture)
  {
    // create a texture for a rendering context
    m_videoState->texture = SDL_CreateTexture(
      m_videoState->renderer
      , SDL_PIXELFORMAT_YV12
      , SDL_TEXTUREACCESS_STREAMING
      , m_videoState->video_ctx->width
      , m_videoState->video_ctx->height
      );
  }
  // reference for the next videopicture to be displayed
  VideoPicture *videoPicture;
  float aspect_ratio = 0;
  int w, h, x, y;

  // get next videoPicture to be displayed from the videopicture queue
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

    // get the size of a window's client area
    int screen_width;
    int screen_height;
    SDL_GetWindowSize(m_screen, &screen_width, &screen_height);

    // global sdl_surface height
    h = screen_height;

    // retrieve width using the calculated aspect ratio and the screen height
    w = ((int) rint(h * aspect_ratio)) & -3;

    // if the new width is bigger than the screen width
    if (w > screen_width)
    {
      // set the width to the screen width
      w = screen_width;

      // recalculate height using the calculated aspect ratio and screen width
      h = ((int) rint(w / aspect_ratio)) & -3;
    }

    x = (screen_width - w);
    y = (screen_height - h);

    // check the number of frames to decode was not exceeded
    {
      // dump information about the frame being rendered

      // set blit area x and y coordinates, width and height
      SDL_Rect rect;
      rect.x = x;
      rect.y = y;
      rect.w = 2 * w;
      rect.h = 2 * h;

      // lock screen mutex
      SDL_LockMutex(m_videoState->screen_mutex);

      // update the texture with the new pixel data
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

      // clear the current rendering target with the drawing color
      SDL_RenderClear(m_videoState->renderer);

      // copy a portion of the texture to the current rendering target
      SDL_RenderCopy(m_videoState->renderer, m_videoState->texture, &rect, nullptr);

      // update the screen with any rendering performed since the previous call
      SDL_RenderPresent(m_videoState->renderer);

      // unlock screen mutex
      SDL_UnlockMutex(m_videoState->screen_mutex);
    }
  }
}

double VideoRenderer::get_audio_clock()
{
  double pts = m_videoState->audio_clock;
  int hw_buf_size = m_videoState->audio_buf_size - m_videoState->audio_buf_index;
  int bytes_per_sec = 0;
  int n = 2 * m_videoState->audio_ctx->ch_layout.nb_channels;
  if (m_videoState->audio_st)
  {
    bytes_per_sec = m_videoState->audio_ctx->sample_rate * n;
  }

  if (bytes_per_sec)
  {
    pts -= (double)hw_buf_size / bytes_per_sec;
  }

  return pts;
}
