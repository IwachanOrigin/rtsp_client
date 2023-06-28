
#include "ffmpegdecoder.h"

#include <iostream>
#include <chrono>
#include <thread>

FFMPEGDecoder::FFMPEGDecoder()
{
  // Create AVFormatContext
  m_fmtCtx = avformat_alloc_context();

  // Allocate the AVPacket structure
  m_pkt = av_packet_alloc();

  m_yuvFrame = av_frame_alloc();
  m_rgbFrame = av_frame_alloc();
}

FFMPEGDecoder::~FFMPEGDecoder()
{
  if (m_pkt)
  {
    av_packet_free(&m_pkt);
  }
  if (m_yuvFrame)
  {
    av_frame_free(&m_yuvFrame);
  }
  if (m_rgbFrame)
  {
    av_frame_free(&m_rgbFrame);
  }
  if (m_videoCodecCtx)
  {
    avcodec_free_context(&m_videoCodecCtx);
    avcodec_close(m_videoCodecCtx);
  }
  if (m_fmtCtx)
  {
    avformat_close_input(&m_fmtCtx);
    avformat_free_context(m_fmtCtx);
  }
}

int FFMPEGDecoder::openInputFile(const std::wstring url)
{
  // Open video file or url
  int ret = avformat_open_input(&m_fmtCtx, url.data(), nullptr, nullptr);
  if (ret < 0)
  {
    std::cerr << "Failed to open video file." << std::endl;
    return -1;
  }

  // Find the stream info(Audio & Video)
  ret = avformat_find_stream_info(m_fmtCtx, nullptr);
  if (ret < 0)
  {
    std::cerr << "Failed to find stream info." << std::endl;
    return -1;
  }

  // Find the index number of the stream whose codec type is video.
  for (unsigned int i = 0; i < m_fmtCtx->nb_streams; i++)
  {
    if (m_fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      m_videoStreamIndex = i;
      break;
    }
  }
  if (m_videoStreamIndex < 0)
  {
    std::cerr << "Failed to find video stream index." << std::endl;
    return -1;
  }

  // Copy to m_fmtCtx that length, bit rate, stream format etc...
  av_dump_format(m_fmtCtx, 0, m_url.toStdString().c_str(), 0);

  // ---------------------------------------------------------------------
  //                              DECODER
  // ---------------------------------------------------------------------

  // Find a decoder
  AVCodecParameters* av_codec_param = nullptr;
  av_codec_param = m_fmtCtx->streams[m_videoStreamIndex]->codecpar;
  m_videoCodec = avcodec_find_decoder(av_codec_param->codec_id);
  if (m_videoCodec == nullptr)
  {
    std::cerr << "Failed to find decoder." << std::endl;
    return -1;
  }

  // Create decoder content based on decoder parameters
  m_videoCodecCtx = avcodec_alloc_context3(m_videoCodec);
  avcodec_parameters_to_context(m_videoCodecCtx, av_codec_param);
  if (m_videoCodecCtx == nullptr)
  {
    std::cerr << "Failed to alloc context." << std::endl;
    return -1;
  }

  // Open decoder
  ret = avcodec_open2(m_videoCodecCtx, m_videoCodec, nullptr);
  if (ret < 0)
  {
    std::cerr << "Failed to open decoder." << std::endl;
    return -1;
  }

  // Set data conversion parameters
  m_imgCtx = sws_getContext(
    m_videoCodecCtx->width         // src
    , m_videoCodecCtx->height      // src
    , m_videoCodecCtx->pix_fmt // src
    , m_videoCodecCtx->width       // dst
    , m_videoCodecCtx->height      // dst
    , AV_PIX_FMT_RGB32   // dst
    , SWS_BICUBIC
    , nullptr
    , nullptr
    , nullptr
    );

  // One frame image data size
  m_numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB32, m_videoCodecCtx->width, m_videoCodecCtx->height, 1);
  m_outBuffer = (unsigned char*)av_malloc(m_numBytes * sizeof(unsigned char));

  // Associate m_rgbFrame with out_buffer.
  // When a change is made to the data in out_buffer, the m_rgbFrame is also changed.
  // If this is not done, ffmepg will throw the error "bad dst image pointers".
  ret = av_image_fill_arrays(
    m_rgbFrame->data
    , m_rgbFrame->linesize
    , m_outBuffer
    , AV_PIX_FMT_RGB32
    , m_videoCodecCtx->width
    , m_videoCodecCtx->height
    , 1);
  if (ret < 0)
  {
    std::cerr << "Failed to fill arrays." << std::endl;
    return -1;
  }
  m_decoderStatus = DecoderStatus::READY;

  return 0;
}

void FFMPEGDecoder::run()
{
  if (m_decoderStatus != DecoderStatus::READY)
  {
    std::cerr << "Decoder status is not READY." << std::endl;
    return;
  }

  m_decoderStatus = DecoderStatus::START;
  std::chrono::milliseconds duration(15);

  // Read video information
  while (av_read_frame(m_fmtCtx, m_pkt) >= 0) // Get a frame data set in AVPacket structure from m_fmtCtx
  {
    // mutex
    if (m_decoderStatus == DecoderStatus::STOP)
    {
      break;
    }

    if (m_pkt->stream_index == m_videoStreamIndex)
    {
      // Send to decoder
      if (avcodec_send_packet(m_videoCodecCtx, m_pkt) == 0)
      {
        // receive from decoder. decode to yuv420p
        int ret = 0;
        while ((ret = avcodec_receive_frame(m_videoCodecCtx, m_yuvFrame)) >= 0)
        {
          if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
          {
            return;
          }
          else if (ret < 0)
          {
            std::cerr << "Error during decoding" << std::endl;
            // exit
          }
          // convert to yuv420p to rgb32
          sws_scale(
            img_ctx
            , (const uint8_t* const*)m_yuvFrame->data
            , m_yuvFrame->linesize
            , 0
            , m_videoCodecCtx->height
            , m_rgbFrame->data
            , m_rgbFrame->linesize
            );

          // sleep : 15ms
          std::this_thread::sleep_for(duration);
        }
      }
      av_packet_unref(m_pkt);
    }
  }
  std::cout << "All video play done." << std::endl;

  // Release
  if (m_pkt)
  {
    av_packet_free(&m_pkt);
  }
  if (m_yuvFrame)
  {
    av_frame_free(&m_yuvFrame);
  }
  if (m_rgbFrame)
  {
    av_frame_free(&m_rgbFrame);
  }
  if (m_codecCtx)
  {
    avcodec_free_context(&m_codecCtx);
    avcodec_close(m_codecCtx);
  }
  if (m_fmtCtx)
  {
    avformat_close_input(&m_fmtCtx);
    avformat_free_context(m_fmtCtx);
  }
}

void FFMPEGDecoder::stop()
{
  // mutex
  m_decoderStatus = DecoderStatus::STOP;
}
