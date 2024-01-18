
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

using namespace player;

VideoRenderer::~VideoRenderer()
{
  this->stop();
}

int VideoRenderer::start(std::shared_ptr<VideoState> vs)
{
  m_vs = vs;
  if (m_vs)
  {
    std::thread([&](VideoRenderer *vr)
    {
      vr->displayThread();
    }, this).detach();
    return 0;
  }

  return -1;
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
    if (m_vs->isPlayerFinished())
    {
      break;
    }

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
            if (m_vs)
            {
              pos = m_vs->masterClock();
              pos += incr;
              m_vs->streamSeek((int64_t)(pos * AV_TIME_BASE), incr);
            }
            break;
          }

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
        m_vs->setPlayerFinished();
      }
      break;

      case FF_REFRESH_EVENT:
      {
        this->videoRefreshTimer();
      }
      break;
    }
  }

  if (m_texture)
  {
    SDL_DestroyTexture(m_texture);
    m_texture = nullptr;
  }
  
  if (m_renderer)
  {
    SDL_DestroyRenderer(m_renderer);
    m_renderer = nullptr;
  }
  
  if (m_screen)
  {
    SDL_DestroyWindow(m_screen);
    m_screen = nullptr;
  }

  return 0;
}

void VideoRenderer::scheduleRefresh(int delay)
{
  // schedule an sdl timer
  int ret = SDL_AddTimer(delay, this->sdlRefreshTimerCb, m_vs.get());
  if (ret == 0)
  {
    std::cerr << "could not schedule refresh callback : " << SDL_GetError() << std::endl;
  }
}

void VideoRenderer::videoRefreshTimer()
{
  // used for video frames display delay and audio video sync
  double pts_delay = 0;
  double sync_threshold = 0;
  double real_delay = 0;
  double audio_video_delay = 0;

  // check the video stream was correctly opened
  auto& videoStream = m_vs->videoStream();
  if (videoStream)
  {
    // check the videopicture queue contains decoded frames
    auto pictureQueueSize = m_vs->videoPictureQueueSize();
    if (pictureQueueSize == 0)
    {
      this->scheduleRefresh(1);
    }
    else
    {
      // Get videopicture reference using the queue read index
      auto& videoPicture = m_vs->videoPicture();

      // get last frame pts
      auto frameDecodeLastPts = m_vs->frameDecodeLastPts();
      pts_delay = videoPicture.pts - frameDecodeLastPts;

      // if the obtained delay is incorrect
      if (pts_delay <= 0 || pts_delay >= 1.0)
      {
        // use the previously calculated delay
        pts_delay = frameDecodeLastPts;
      }

      // save delay information for the next time
      m_vs->setFrameDecodeLastDelay(pts_delay);
      m_vs->setFrameDecodeLastPts(videoPicture.pts);

      // update delay to stay in sync with the audio
      if (m_vs->syncType() != player::SYNC_TYPE::AV_SYNC_VIDEO_MASTER)
      {
        auto refClock = m_vs->masterClock();
        auto diff = videoPicture.pts - refClock;
        // skip or repeat the frame taking into account the delay
        sync_threshold = (pts_delay > AV_SYNC_THRESHOLD) ? pts_delay : AV_SYNC_THRESHOLD;

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
      }

      auto frameDecodeTimer = m_vs->frameDecodeTimer() + pts_delay;
      m_vs->setFrameDecodeTimer(frameDecodeTimer);
      // compute the real delay
      real_delay = frameDecodeTimer - (av_gettime() / 1000000.0);
      if (real_delay < 0.010)
      {
        real_delay = 0.010;
      }

      this->scheduleRefresh((int)(real_delay * 1000 + 0.5));

      // show the frame on the sdl_surface
      this->videoDisplay();

      // update read index for the next frame
      auto& pictureQueueRIndex = m_vs->videoPictureQueueRIndex();
      if (++pictureQueueRIndex == VIDEO_PICTURE_QUEUE_SIZE)
      {
        pictureQueueRIndex = 0;
      }

      // lock videopicture queue mutex
      auto& pictureQueueMutex = m_vs->pictureQueueMutex();
      SDL_LockMutex(pictureQueueMutex);

      // decrease videopicture queue size
      auto& pictureQueueSize = m_vs->videoPictureQueueSize();
      pictureQueueSize--;

      // notify other threads waiting for the videoPicture queue
      auto& pictureQueueCond = m_vs->pictureQueueCond();
      SDL_CondSignal(pictureQueueCond);

      // unlock videoPicture queue mutex
      SDL_UnlockMutex(pictureQueueMutex);
    }
  }
  else
  {
    this->scheduleRefresh(100);
  }
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

void VideoRenderer::videoDisplay()
{
  auto& videoCodecCtx = m_vs->videoCodecCtx();
  // create window, renderer and textures if not already created
  if (!m_screen)
  {
    //int flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS | SDL_WINDOW_TOOLTIP;
    int flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE;
    m_screen = SDL_CreateWindow(
      "display"
      , SDL_WINDOWPOS_UNDEFINED
      , SDL_WINDOWPOS_UNDEFINED
      , videoCodecCtx->width / 2
      , videoCodecCtx->height / 2
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

  if (!m_renderer)
  {
    // create a 2d rendering context for the sdl_window
    m_renderer = SDL_CreateRenderer(
      m_screen
      , -1
      , SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE
      );
  }

  if (!m_texture)
  {
    // create a texture for a rendering context
    m_texture = SDL_CreateTexture(
      m_renderer
      , SDL_PIXELFORMAT_YV12
      , SDL_TEXTUREACCESS_STREAMING
      , videoCodecCtx->width
      , videoCodecCtx->height
      );
  }
  // reference for the next videopicture to be displayed
  float aspect_ratio = 0;
  int w = 0, h = 0, x = 0, y = 0;

  // get next videoPicture to be displayed from the videopicture queue
        // Get videopicture reference using the queue read index
  auto& videoPicture = m_vs->videoPicture();
  if (videoPicture.frame)
  {
    if (videoCodecCtx->sample_aspect_ratio.num == 0)
    {
      aspect_ratio = 0;
    }
    else
    {
      aspect_ratio = av_q2d(videoCodecCtx->sample_aspect_ratio) * videoCodecCtx->width / videoCodecCtx->height;
    }

    if (aspect_ratio <= 0.0)
    {
      aspect_ratio = (float)videoCodecCtx->width / (float)videoCodecCtx->height;
    }

    // get the size of a window's client area
    int screen_width = 0;
    int screen_height = 0;
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
      SDL_Rect rect{};
      rect.x = x;
      rect.y = y;
      rect.w = 2 * w;
      rect.h = 2 * h;

      // lock screen mutex
      auto& screenMutex = m_vs->screenMutex();
      SDL_LockMutex(screenMutex);

      // update the texture with the new pixel data
      SDL_UpdateYUVTexture(
        m_texture
        , &rect
        , videoPicture.frame->data[0]
        , videoPicture.frame->linesize[0]
        , videoPicture.frame->data[1]
        , videoPicture.frame->linesize[1]
        , videoPicture.frame->data[2]
        , videoPicture.frame->linesize[2]
        );

      // clear the current rendering target with the drawing color
      SDL_RenderClear(m_renderer);

      // copy a portion of the texture to the current rendering target
      SDL_RenderCopy(m_renderer, m_texture, &rect, nullptr);

      // update the screen with any rendering performed since the previous call
      SDL_RenderPresent(m_renderer);

      // unlock screen mutex
      SDL_UnlockMutex(screenMutex);
    }
  }
}

double VideoRenderer::getAudioClock()
{
  auto& audioCodecCtx = m_vs->audioCodecCtx();
  auto& audioStream = m_vs->audioStream();
  double pts = m_vs->audioClock();
  int hw_buf_size = m_vs->audioBufSize() - m_vs->audioBufIndex();
  int bytes_per_sec = 0;
  int n = 2 * audioCodecCtx->ch_layout.nb_channels;
  if (audioStream)
  {
    bytes_per_sec = audioCodecCtx->sample_rate * n;
  }

  if (bytes_per_sec)
  {
    pts -= (double)hw_buf_size / bytes_per_sec;
  }

  return pts;
}
