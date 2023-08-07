
#include <iostream>
#include <thread>
#include "videodecoder.h"
#include "timer.h"

VideoDecoder::VideoDecoder()
  : m_videoState(nullptr)
{
}

VideoDecoder::~VideoDecoder()
{
  m_videoState = nullptr;
}

int VideoDecoder::start(VideoState *videoState)
{
  m_videoState = videoState;
  if (m_videoState)
  {
    std::thread([&](VideoDecoder *decoder)
      {
        decoder->video_thread(m_videoState);
      }, this).detach();
  }
  else
  {
    return -1;
  }
  return 0;
}

int VideoDecoder::video_thread(void *arg)
{
  // retrieve global videostate
  VideoState *videoState = (VideoState *)arg;

  // allocate an AVPacket to be used to retrieve data from the videoq.
  AVPacket *packet = av_packet_alloc();
  if (packet == nullptr)
  {
    std::cerr << "Could not alloc packet" << std::endl;
    return -1;
  }

  // set this when we are done decoding an entire frame
  int frameFinished = 0;

  // allocate a new AVFrame, used to decode video packets
  static AVFrame *pFrame = nullptr;
  pFrame = av_frame_alloc();
  if (!pFrame)
  {
    std::cerr << "Could not allocate AVFrame" << std::endl;
    return -1;
  }

  double pts = 0.0;

  for (;;)
  {
    // get a packet from videq
    int ret = videoState->videoq.get(packet, 1);
    if (ret < 0)
    {
      // means we quit getting packets
      break;
    }

    if (packet->data == videoState->flush_pkt->data)
    {
      avcodec_flush_buffers(videoState->video_ctx);
      continue;
    }

    // init set pts to 0 for all frames
    pts = 0.0;

    // give the decoder raw compressed data in an AVPacket
    ret = avcodec_send_packet(videoState->video_ctx, packet);
    if (ret < 0)
    {
      std::cerr << "Error sending packet for decoding" << std::endl;
      return -1;
    }

    while (ret >= 0)
    {
      // get decoded output data from decoder
      ret = avcodec_receive_frame(videoState->video_ctx, pFrame);
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
      {
        break;
      }
      else if (ret < 0)
      {
        std::cerr << "Error while decoding" << std::endl;
        return -1;
      }
      else
      {
        frameFinished = 1;
      }

      pts = this->guess_correct_pts(videoState->video_ctx, pFrame->pts, pFrame->pkt_dts);
      // in case we get an undefined timestamp value
      if (pts == AV_NOPTS_VALUE)
      {
        // set pts to the default value of 0
        pts = 0.0;
      }

      pts *= av_q2d(videoState->video_st->time_base);

      // did we get an entire video frame?
      if (frameFinished)
      {
        pts = this->sync_video(videoState, pFrame, pts);
        if (m_videoState->queue_picture(pFrame, pts) < 0)
        {
          break;
        }
      }
    }
    // wipe the packet
    av_packet_unref(packet);
  }

  // wipe the frame
  av_frame_free(&pFrame);
  av_free(pFrame);

  return 0;
}


int64_t VideoDecoder::guess_correct_pts(AVCodecContext *ctx, int64_t reordered_pts, int64_t dts)
{
  int64_t pts = AV_NOPTS_VALUE;

  if (dts != AV_NOPTS_VALUE)
  {
    ctx->pts_correction_num_faulty_dts += dts <= ctx->pts_correction_last_dts;
    ctx->pts_correction_last_dts = dts;
  }
  else if (reordered_pts != AV_NOPTS_VALUE)
  {
    ctx->pts_correction_last_dts = reordered_pts;
  }

  if (reordered_pts != AV_NOPTS_VALUE)
  {
    ctx->pts_correction_num_faulty_pts += reordered_pts <= ctx->pts_correction_last_pts;
    ctx->pts_correction_last_pts = reordered_pts;
  }
  else if (dts != AV_NOPTS_VALUE)
  {
    ctx->pts_correction_last_pts = dts;
  }

  if ((ctx->pts_correction_num_faulty_pts <= ctx->pts_correction_num_faulty_dts || dts == AV_NOPTS_VALUE) && reordered_pts != AV_NOPTS_VALUE)
  {
    pts = reordered_pts;
  }
  else
  {
    pts = dts;
  }

  return pts;
}

double VideoDecoder::sync_video(VideoState *videoState, AVFrame *src_frame, double pts)
{

  double frame_delay = 0.0;

  if (pts!= 0)
  {
    // if we have pts, set video clock to it
    videoState->video_clock = pts;
  }
  else
  {
    // if we aren't given a pts, set it to the clock
    pts = videoState->video_clock;
  }

  // update the video clock
  frame_delay = av_q2d(videoState->video_ctx->time_base);

  // if we are repeating a frame, adjust clock accordingly
  frame_delay += src_frame->repeat_pict * (frame_delay * 0.5);

  videoState->video_clock += frame_delay;

  return pts;
}
