
#include "videosink.h"
#include <H264VideoRTPSource.hh>
#include <cassert>
#include <iostream>

using namespace client;

// Even though we're not going to be doing anything with the incoming data, we still need to receive it.
// Define the size of the buffer that we'll use:
#define DUMMY_SINK_RECEIVE_BUFFER_SIZE 1000000

VideoSink* VideoSink::createNew(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId)
{
  return new VideoSink(env, subsession, streamId);
}

VideoSink::VideoSink(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId)
  : MediaSink(env)
  , m_subsession(subsession)
{
  m_streamId = strDup(streamId);
  m_receiveBuffer = new u_int8_t[DUMMY_SINK_RECEIVE_BUFFER_SIZE];
  this->initDecRender();
}

VideoSink::~VideoSink()
{
  delete[] m_receiveBuffer;
  delete[] m_streamId;

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
  packet->size = frameSize + startCodeLength;
  packet->data = new uint8_t[frameSize + 4];

  std::memcpy(packet->data, startCode, startCodeLength);
  std::memcpy(packet->data + 4, m_receiveBuffer, frameSize);

  if (avcodec_send_packet(m_videoCodecContext, packet) != 0)
  {
    envir() << "Failed to send pacekkt to decoder." << "\n";
  }

  auto frame = av_frame_alloc();
  while (avcodec_receive_frame(m_videoCodecContext, frame) == 0)
  {
    // To queue
    envir() << "decode." << "\n";
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
  if (fSource == NULL)
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

bool VideoSink::initDecRender()
{
  const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
  assert(codec);

  m_videoCodecContext = avcodec_alloc_context3(codec);
  assert(m_videoCodecContext);
  if (avcodec_open2(m_videoCodecContext, codec, nullptr) < 0)
  {
    std::cerr << "Failed to avcodec_open2 func." << std::endl;
    return false;
  }

  return true;
}

