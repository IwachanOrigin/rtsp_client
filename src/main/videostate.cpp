
#include <iostream>
#include "videostate.h"

VideoState::VideoState()
  : pFormatCtx(nullptr)
  , audioStream(-1)
  , audio_st(nullptr)
  , audio_ctx(nullptr)
  , audio_buf_size(0)
  , audio_buf_index(0)
  , audio_pkt_data(nullptr)
  , audio_pkt_size(0)
  , audio_clock(0)
  , audio_diff_cum(0)
  , audio_diff_avg_coef(0)
  , audio_diff_threshold(0)
  , audio_diff_avg_count(0)
  , videoStream(-1)
  , video_st(nullptr)
  , video_ctx(nullptr)
  , video_clock(0)
  , video_current_pts(0)
  , video_current_pts_time(0)
  , texture(nullptr)
  , renderer(nullptr)
  , sws_ctx(nullptr)
  , frame_timer(0)
  , frame_last_pts(0)
  , frame_last_delay(0)
  , quit(0)
  , pictq_size(0)
  , pictq_rindex(0)
  , pictq_windex(0)
  , pictq_cond(SDL_CreateCond())
  , pictq_mutex(SDL_CreateMutex())
  , screen_mutex(SDL_CreateMutex())
  , output_audio_device_index(0)
{
  flush_pkt = av_packet_alloc();
  flush_pkt->data = (uint8_t*)"FLUSH";
}

VideoState::~VideoState()
{
}

void VideoState::alloc_picture()
{
  VideoPicture *videoPicture;
  videoPicture = &pictq[pictq_windex];

  // check if the sdl_overlay is allocated
  if (videoPicture->frame)
  {
    // we already have an avframe allocated, free memory
    av_frame_free(&videoPicture->frame);
    av_free(videoPicture->frame);
  }

  // lock global screen mutex
  SDL_LockMutex(screen_mutex);

  // get the size in bytes required to store an image with the given parameters
  int numBytes = 0;
  numBytes = av_image_get_buffer_size(
    AV_PIX_FMT_YUV420P
    , video_ctx->width
    , video_ctx->height
    , 32
    );

  // allocate image data buffer
  uint8_t *buffer = nullptr;
  buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));

  // alloc the avframe later used to contain the scaled frame
  videoPicture->frame = av_frame_alloc();
  if (videoPicture->frame == nullptr)
  {

    return;
  }

  // the fields of the given image are filled in by using the buffer which points to the image data buffer
  av_image_fill_arrays(
    videoPicture->frame->data
    , videoPicture->frame->linesize
    , buffer
    , AV_PIX_FMT_YUV420P
    , video_ctx->width
    , video_ctx->height
    , 32
    );

  // unlock mutex
  SDL_UnlockMutex(screen_mutex);

  // update videoPicture struct fields
  videoPicture->width = video_ctx->width;
  videoPicture->height = video_ctx->height;
  videoPicture->allocated = 1;
}

int VideoState::queue_picture(AVFrame *pFrame, double pts)
{
  // lock videostate pictq mutex
  SDL_LockMutex(pictq_mutex);

  // wait until we have space for a new picture in pictq
  while (pictq_size >= VIDEO_PICTURE_QUEUE_SIZE && !quit)
  {
    SDL_CondWait(pictq_cond, pictq_mutex);
  }

  // unlock pictq mutex
  SDL_UnlockMutex(pictq_mutex);

  // check global quit flag
  if (quit)
  {
    return -1;
  }

  VideoPicture *videoPicture;
  videoPicture = &pictq[pictq_windex];

  // if the videopicture sdl_overlay is not allocated or has a different width, height
  if (!videoPicture->frame ||
      videoPicture->width != video_ctx->width ||
      videoPicture->height != video_ctx->height)
  {
    // set sdl_overlay not allocated
    videoPicture->allocated = 0;

    // allocate a new sdl_overlay for the videoPicture struct
    this->alloc_picture();

    // check global quit flag
    if (quit)
    {
      return -1;
    }
  }

  // check the new sdl_overlay was correctly allocated
  if (videoPicture->frame)
  {
    // so now we've got pictures lining up onto our picture queue with proper PTS values
    videoPicture->pts = pts;

    // set videopicture avframe info using the last decoded frame
    videoPicture->frame->pict_type = pFrame->pict_type;
    videoPicture->frame->pts = pFrame->pts;
    videoPicture->frame->pkt_dts = pFrame->pkt_dts;
    videoPicture->frame->key_frame = pFrame->key_frame;
    videoPicture->frame->best_effort_timestamp = pFrame->best_effort_timestamp;
    videoPicture->frame->width = pFrame->width;
    videoPicture->frame->height = pFrame->height;

    // scale the image in pFrame->data and put the resulting scaled image in pict->data
    sws_scale(
      sws_ctx
      , (uint8_t const* const*)pFrame->data
      , pFrame->linesize
      , 0
      , video_ctx->height
      , videoPicture->frame->data
      , videoPicture->frame->linesize
      );

    // update videopicture queue write index
    pictq_windex++;

    // if the write index has reached the videopicture queue size
    if (pictq_windex == VIDEO_PICTURE_QUEUE_SIZE)
    {
      pictq_windex = 0;
    }

    // lock videopicture queue
    SDL_LockMutex(pictq_mutex);

    // increase videopictq queue size
    pictq_size++;

    // unlock videopicture queue
    SDL_UnlockMutex(pictq_mutex);
  }

  return 0;
}

double VideoState::get_master_clock()
{
  if (av_sync_type == SYNC_TYPE::AV_SYNC_VIDEO_MASTER)
  {
    return get_video_clock();
  }
  else if (av_sync_type == SYNC_TYPE::AV_SYNC_AUDIO_MASTER)
  {
    return get_audio_clock();
  }
  else if (av_sync_type == SYNC_TYPE::AV_SYNC_EXTERNAL_MASTER)
  {
    return get_external_clock();
  }
  else
  {
    std::cerr << "Error : Undefined a/v sync type" << std::endl;
    return -1;
  }
}

double VideoState::get_video_clock()
{
  double delta = (av_gettime() - video_current_pts_time) / 1000000.0;
  return video_current_pts + delta;
}

double VideoState::get_audio_clock()
{
  double pts = audio_clock;
  int hw_buf_size = audio_buf_size - audio_buf_index;
  int bytes_per_sec = 0;
  int n = 2 * audio_ctx->ch_layout.nb_channels;

  if (audio_st)
  {
    bytes_per_sec = audio_ctx->sample_rate * n;
  }

  if (bytes_per_sec)
  {
    pts -= (double) hw_buf_size / bytes_per_sec;
  }

  return pts;
}

double VideoState::get_external_clock()
{
  external_clock_time = av_gettime();
  external_clock = external_clock_time / 1000000.0;

  return external_clock;
}


void VideoState::stream_seek(int64_t pos, int rel)
{
  if (!seek_req)
  {
    seek_pos = pos;
    seek_flags = rel < 0 ? AVSEEK_FLAG_BACKWARD : 0;
    seek_req = 1;
  }
}
