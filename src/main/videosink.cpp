
#include "videosink.h"
#include <H264VideoRTPSource.hh>
#include <cassert>
#include <iostream>
#include <algorithm>

using namespace client;

// Even though we're not going to be doing anything with the incoming data, we still need to receive it.
// Define the size of the buffer that we'll use:
#define DUMMY_SINK_RECEIVE_BUFFER_SIZE 1000000

VideoSink* VideoSink::createNew(UsageEnvironment& env, MediaSubsession& subsession,  char const* streamURL)
{
  return new VideoSink(env, subsession, streamURL);
}

VideoSink::VideoSink(UsageEnvironment& env, MediaSubsession& subsession, char const* streamURL)
  : MediaSink(env)
  , m_subsession(subsession)
{
  m_streamURL = strDup(streamURL);
  m_receiveBuffer = new u_int8_t[DUMMY_SINK_RECEIVE_BUFFER_SIZE];
  this->init(streamURL);
}

VideoSink::~VideoSink()
{
  delete[] m_receiveBuffer;
  delete[] m_streamURL;

  if (m_videoCodecContext)
  {
    avcodec_close(m_videoCodecContext);
    m_videoCodecContext = nullptr;
  }
}

void VideoSink::afterGettingFrame(
  void* clientData
  , unsigned frameSize
  , unsigned numTruncatedBytes
  , struct timeval presentationTime
  , unsigned durationInMicroseconds
  )
{
  VideoSink* sink = (VideoSink*)clientData;
  sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

void VideoSink::afterGettingFrame(
  unsigned frameSize
  , unsigned numTruncatedBytes
  , struct timeval presentationTime
  , unsigned /*durationInMicroseconds*/)
{
  const int startCodeLength = 4;
  uint8_t startCode[4] = {0x00, 0x00, 0x00, 0x01};

  auto packet = av_packet_alloc();
  switch (m_videoCodecContext->codec_id)
  {
    case AV_CODEC_ID_H264:
    {
      packet->size = frameSize + startCodeLength;
      packet->data = new uint8_t[frameSize + 4];
      std::memcpy(packet->data, startCode, startCodeLength);
      std::memcpy(packet->data + 4, m_receiveBuffer, frameSize);
    }
    break;

    default:
    {
      packet->size = frameSize;
      packet->data = new uint8_t[frameSize];
      std::memcpy(packet->data, m_receiveBuffer, frameSize);
    }
    break;
  }

  if (avcodec_send_packet(m_videoCodecContext, packet) != 0)
  {
    envir() << "Failed to send packet to decoder." << "\n";
  }

  auto frame = av_frame_alloc();
  while (avcodec_receive_frame(m_videoCodecContext, frame) == 0)
  {
    // To queue
    envir() << "video decoded." << "\n";
  }

  av_packet_unref(packet);
  av_packet_free(&packet);
  av_frame_unref(frame);
  av_frame_free(&frame);

  // Then continue, to request the next frame of data:
  continuePlaying();
}

Boolean VideoSink::continuePlaying()
{
  if (fSource == nullptr)
  {
    return False; // sanity check (should not happen)
  }
  // Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
  fSource->getNextFrame(
    m_receiveBuffer
    , DUMMY_SINK_RECEIVE_BUFFER_SIZE
    , afterGettingFrame
    , this
    , onSourceClosure
    , this);
  return True;
}

bool VideoSink::init(char const* streamURL)
{
  AVFormatContext* pFormatCtx = nullptr;
  auto ret = avformat_open_input(&pFormatCtx, streamURL, nullptr, nullptr);
  if (ret < 0)
  {
    std::cerr << "Could not open url " << streamURL << std::endl;
    return false;
  }

  // Read packets of the media file to get stream info
  ret = avformat_find_stream_info(pFormatCtx, nullptr);
  if (ret < 0)
  {
    std::cerr << "Could not find stream info " << streamURL << std::endl;
    this->releaseFormatCtx(pFormatCtx);
    return false;
  }

  // Loop through the streams that have been found
  auto videoStream = -1;
  for (int i = 0; i < pFormatCtx->nb_streams; i++)
  {
    // Look for the audio stream
    if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && videoStream < 0)
    {
      videoStream = i;
    }
  }

  if (videoStream < 0)
  {
    std::cerr << "Could not find audio stream index." << std::endl;
    this->releaseFormatCtx(pFormatCtx);
    return false;
  }

  // Retrieve codec for the given stream index
  const AVCodec* codec = avcodec_find_decoder(pFormatCtx->streams[videoStream]->codecpar->codec_id);
  assert(codec);
  if (codec == nullptr)
  {
    std::cerr << "Unsupported codec" << std::endl;
    this->releaseFormatCtx(pFormatCtx);
    return false;
  }

  m_videoCodecContext = avcodec_alloc_context3(codec);
  assert(m_videoCodecContext);
  if (avcodec_open2(m_videoCodecContext, codec, nullptr) < 0)
  {
    std::cerr << "Failed to avcodec_open2 func." << std::endl;
    std::cerr << "Codec ID is " << codec->id << std::endl;
    this->releaseFormatCtx(pFormatCtx);
    return false;
  }

  // Release 
  this->releaseFormatCtx(pFormatCtx);

  return true;
}

void VideoSink::releaseFormatCtx(AVFormatContext*& formatctx)
{
  if (formatctx)
  {
    // Close the opened input AVFormatContext
    avformat_close_input(&formatctx);
  }
  formatctx = nullptr;
}
