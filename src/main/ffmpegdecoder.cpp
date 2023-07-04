
#include "ffmpegdecoder.h"
#include "dx11manager.h"
#include "stringhelper.h"

#include <iostream>
#include <chrono>
#include <thread>

using namespace manager;

FFMPEGDecoder::FFMPEGDecoder()
{
  // Create AVFormatContext
  m_fmtCtx = avformat_alloc_context();

  // Allocate the AVPacket structure
  m_pkt = av_packet_alloc();

  m_yuvFrame = av_frame_alloc();
  m_rgbFrame = av_frame_alloc();
  m_pcmFrame = av_frame_alloc();
}

FFMPEGDecoder::~FFMPEGDecoder()
{
}

int FFMPEGDecoder::openInputFile(const std::wstring url, uint32_t& outFrameWidth, uint32_t& outFrameHeight)
{
  // convert wstring to string
  std::string strURL = wstringToString(url);

  // Set rtsp transport protocols
  av_dict_set(&m_rtspOptions, "rtsp_transport", "tcp", 0);

  // Open video file or url
  int ret = avformat_open_input(&m_fmtCtx, strURL.c_str(), nullptr, &m_rtspOptions);
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

  // Find the index number of the stream whose codec type is audio.
  for (unsigned int i = 0; i < m_fmtCtx->nb_streams; i++)
  {
    if (m_fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
    {
      m_audioStreamIndex = i;
      break;
    }
  }
  if (m_audioStreamIndex < 0)
  {
    std::cerr << "Failed to find audio stream index." << std::endl;
    return -1;
  }

  // Copy to m_fmtCtx that length, bit rate, stream format etc...
  av_dump_format(m_fmtCtx, 0, strURL.c_str(), 0);

  // ---------------------------------------------------------------------
  //                              DECODER
  // ---------------------------------------------------------------------

  // for Video
  ret = this->setupVideoCodec();
  if (ret < 0)
  {
    std::cerr << "" << std::endl;
    return -1;
  }

  // for Audio
  ret = this->setupAudioCodec();
  if (ret < 0)
  {
    std::cerr << "" << std::endl;
    return -1;
  }

  m_decoderStatus = DecoderStatus::READY;
  outFrameWidth = (uint32_t)m_videoCodecCtx->width;
  outFrameHeight = (uint32_t)m_videoCodecCtx->height;

  return 0;
}

int FFMPEGDecoder::setupVideoCodec()
{
  int ret = 0;

  // Find a decoder to video
  AVCodecParameters* videoCodecParam = nullptr;
  videoCodecParam = m_fmtCtx->streams[m_videoStreamIndex]->codecpar;
  m_videoCodec = avcodec_find_decoder(videoCodecParam->codec_id);
  if (m_videoCodec == nullptr)
  {
    std::cerr << "Failed to find video decoder." << std::endl;
    return -1;
  }

  // Create decoder content based on decoder parameters
  m_videoCodecCtx = avcodec_alloc_context3(m_videoCodec);
  avcodec_parameters_to_context(m_videoCodecCtx, videoCodecParam);
  if (m_videoCodecCtx == nullptr)
  {
    std::cerr << "Failed to alloc context to video." << std::endl;
    return -1;
  }

  // Open decoder
  ret = avcodec_open2(m_videoCodecCtx, m_videoCodec, nullptr);
  if (ret < 0)
  {
    std::cerr << "Failed to open video decoder." << std::endl;
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

  return 0;
}

int FFMPEGDecoder::setupAudioCodec()
{
  int ret = 0;

  // Find a decoder to audio
  AVCodecParameters* audioCodecParam = nullptr;
  audioCodecParam = m_fmtCtx->streams[m_audioStreamIndex]->codecpar;
  m_audioCodec = avcodec_find_decoder(audioCodecParam->codec_id);
  if (m_audioCodec == nullptr)
  {
    std::cerr << "Failed to find audio decoder." << std::endl;
    return -1;
  }

  // Create decoder content based on decoder parameters
  m_audioCodecCtx = avcodec_alloc_context3(m_audioCodec);
  avcodec_parameters_to_context(m_audioCodecCtx, audioCodecParam);
  if (m_audioCodecCtx == nullptr)
  {
    std::cerr << "Failed to alloc context to audio." << std::endl;
    return -1;
  }

  m_audioCodecCtx->pkt_timebase = m_fmtCtx->streams[m_audioStreamIndex]->time_base;

  // Open audio decoder
  ret = avcodec_open2(m_audioCodecCtx, m_audioCodec, nullptr);
  if (ret < 0)
  {
    std::cerr << "Failed to open decoder." << std::endl;
    return -1;
  }

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
  std::chrono::milliseconds duration(1);

  // Read video information
  while (av_read_frame(m_fmtCtx, m_pkt) >= 0) // Get a frame data set in AVPacket structure from m_fmtCtx
  {
    // Check decoder status.
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      if (m_decoderStatus == DecoderStatus::STOP)
      {
        break;
      }
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
            break;
          }
          else if (ret < 0)
          {
            std::cerr << "Error during decoding" << std::endl;
            break;
          }
          // convert to yuv420p to rgb32
          sws_scale(
            m_imgCtx
            , (const uint8_t* const*)m_yuvFrame->data
            , m_yuvFrame->linesize
            , 0
            , m_videoCodecCtx->height
            , m_rgbFrame->data
            , m_rgbFrame->linesize
            );

          // Copy to gpu from cpu
          if(DX11Manager::getInstance().updateTexture(m_outBuffer, m_numBytes))
          {
            // Render
            DX11Manager::getInstance().render();
          }
          // sleep : 1ms
          std::this_thread::sleep_for(duration);
        }
      }
      av_packet_unref(m_pkt);
    }
#if 0
    else if (m_pkt->stream_index == m_audioStreamIndex)
    {
      // Send to decoder
      if (avcodec_send_packet(m_audioCodecCtx, m_pkt) == 0)
      {
        // receive from decoder. decode to PCM
        while (avcodec_receive_frame(m_audioCodecCtx, m_pcmFrame) == 0)
        {
          // Planar (plane), whose data format is arranged as (remember, in particular, that it is interleaved by point nb_samples sampling points, not bytes).
          // LLLLLLRRRRRRLLLLLLRRRRRRLLLLLLRRRRRRRL... (Each LLLLLRRRRRR is an audio frame)
          // And the data format without P (i.e. interleaved) is arranged as
          // LRLRLRLRLRLRLRLRLRLRLRLRLRLRLRLRLRL... (Each LR is an audio sample)
          if (av_sample_fmt_is_planar(m_audioCodecCtx->sample_fmt))
          {
            int numBytes = av_get_bytes_per_sample(m_audioCodecCtx->sample_fmt);
            // pcm is played in LRLRLR format, so the data is saved interleaved.
            for (int i = 0; i < m_pcmFrame->nb_samples; i++)
            {
              for (int ch = 0; ch < m_audioCodecCtx->ch_layout.nb_channels; ch++)
              {
                //fwrite((char*)pcm_frame->data[ch] + numBytes * i, 1, numBytes, fp);
                std::cout << "decode audio data." << std::endl;
              }
            }
          }
          // sleep : 1ms
          std::this_thread::sleep_for(duration);
        }
      }
      av_packet_unref(m_pkt);
    }
#endif
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
  if (m_pcmFrame)
  {
    av_frame_free(&m_pcmFrame);
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

void FFMPEGDecoder::stop()
{
  std::lock_guard<std::mutex> lock(m_mutex);
  m_decoderStatus = DecoderStatus::STOP;
}
