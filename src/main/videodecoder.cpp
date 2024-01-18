
#include <iostream>
#include <thread>
#include "videodecoder.h"

using namespace player;

const int MAX_QUEUE_SIZE = 50;

VideoDecoder::~VideoDecoder()
{
  this->stop();
}

int VideoDecoder::start(std::shared_ptr<VideoState> vs)
{
  m_vs = vs;
  if (m_vs)
  {
    std::thread([&](VideoDecoder *decoder)
    {
      decoder->decodeThread(m_vs);
    }, this).detach();
    return 0;
  }

  return -1;
}

void VideoDecoder::stop()
{
  std::unique_lock<std::mutex> lock(m_mutex);
  m_finishedDecoder = true;
}

int VideoDecoder::decodeThread(std::shared_ptr<VideoState> vs)
{
  // retrieve global videostate
  auto videoState = vs;

  // allocate an AVPacket to be used to retrieve data from the videoq.
  AVPacket* packet = av_packet_alloc();
  if (packet == nullptr)
  {
    std::cerr << "Could not alloc packet" << std::endl;
    return -1;
  }

  // set this when we are done decoding an entire frame
  int frameFinished = 0;

  // allocate a new AVFrame, used to decode video packets
  AVFrame* pFrame = av_frame_alloc();
  if (!pFrame)
  {
    std::cerr << "Could not allocate AVFrame" << std::endl;
    av_packet_unref(packet);
    av_packet_free(&packet);
    return -1;
  }

  double pts = 0.0;
  auto& videoCodecCtx = videoState->videoCodecCtx();
  for (;;)
  {
    // Check decoder finish flg
    {
      std::unique_lock<std::mutex> lock(m_mutex);
      if (m_finishedDecoder)
      {
        break;
      }
    }

    // get a packet from videq
    int ret = videoState->popVideoPacketRead(packet);
    if (ret < 0)
    {
      if (videoState->isPlayerFinished())
      {
        // means we quit getting packets
        break;
      }
      continue;
    }

    auto flushPacket = videoState->flushPacket();
    if (flushPacket)
    {
      if (packet->data == flushPacket->data)
      {
        avcodec_flush_buffers(videoCodecCtx);
        continue;
      }
    }

    // init set pts to 0 for all frames
    pts = 0.0;

    // give the decoder raw compressed data in an AVPacket
    ret = avcodec_send_packet(videoCodecCtx, packet);
    if (ret < 0)
    {
      std::cerr << "Error sending packet for decoding" << std::endl;
      return -1;
    }

    while (ret >= 0)
    {
      // get decoded output data from decoder
      ret = avcodec_receive_frame(videoCodecCtx, pFrame);
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

      pts = (double)this->guessCorrectPts(videoCodecCtx, pFrame->pts, pFrame->pkt_dts);
      // in case we get an undefined timestamp value
      if (pts == AV_NOPTS_VALUE)
      {
        // set pts to the default value of 0
        pts = 0.0;
      }

      auto& videoStream = videoState->videoStream();
      pts *= av_q2d(videoStream->time_base);

      // did we get an entire video frame?
      if (frameFinished)
      {
        pts = this->syncVideo(videoState, pFrame, pts);
        if (videoState->queuePicture(pFrame, pts) < 0)
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


int64_t VideoDecoder::guessCorrectPts(AVCodecContext *ctx, const int64_t& reordered_pts, const int64_t& dts)
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

double VideoDecoder::syncVideo(std::shared_ptr<VideoState> videoState, AVFrame* srcFrame, double pts)
{
  double frame_delay = 0.0;

  if (pts!= 0)
  {
    // if we have pts, set video clock to it
    videoState->setVideoClock(pts);
  }
  else
  {
    // if we aren't given a pts, set it to the clock
    pts = videoState->videoClock();
  }

  // update the video clock
  auto& videoCodecCtx = videoState->videoCodecCtx();
  frame_delay = av_q2d(videoCodecCtx->time_base);

  // if we are repeating a frame, adjust clock accordingly
  frame_delay += srcFrame->repeat_pict * (frame_delay * 0.5);

  auto videoClock = videoState->videoClock();
  videoState->setVideoClock(videoClock + frame_delay);

  return pts;
}
