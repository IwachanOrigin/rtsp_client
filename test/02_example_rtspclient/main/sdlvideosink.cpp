
#include "sdlvideosink.h"
#include <H264VideoRTPSource.hh>
#include <cassert>
#include <iostream>

using namespace client;

// Even though we're not going to be doing anything with the incoming data, we still need to receive it.
// Define the size of the buffer that we'll use:
#define DUMMY_SINK_RECEIVE_BUFFER_SIZE 100000

SdlVideoSink* SdlVideoSink::createNew(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId)
{
  return new SdlVideoSink(env, subsession, streamId);
}

SdlVideoSink::SdlVideoSink(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId)
  : MediaSink(env)
  , m_subsession(subsession)
{
  m_streamId = strDup(streamId);
  m_receiveBuffer = new u_int8_t[DUMMY_SINK_RECEIVE_BUFFER_SIZE];
  this->initDecRender();
}

SdlVideoSink::~SdlVideoSink()
{
  delete[] m_receiveBuffer;
  delete[] m_streamId;

  if (m_videoCodecContext)
  {
    avcodec_close(m_videoCodecContext);
    m_videoCodecContext = nullptr;
  }
}

void SdlVideoSink::afterGettingFrame(
  void* clientData
  , unsigned frameSize
  , unsigned numTruncatedBytes
  , struct timeval presentationTime
  , unsigned durationInMicroseconds
  )
{
  SdlVideoSink* sink = (SdlVideoSink*)clientData;
  sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

void SdlVideoSink::afterGettingFrame(
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
    SDL_LockMutex(m_playerContext.mutex);

     SDL_UpdateYUVTexture(
       m_playerContext.texture
       , nullptr
       , frame->data[0]
       , frame->linesize[0]
       , frame->data[1]
       , frame->linesize[1]
       , frame->data[2]
       , frame->linesize[2]
       );

    SDL_RenderClear(m_playerContext.renderer);
    SDL_RenderCopy(m_playerContext.renderer, m_playerContext.texture, nullptr, nullptr);
    SDL_RenderPresent(m_playerContext.renderer);

    SDL_UnlockMutex(m_playerContext.mutex);
  }

  av_packet_unref(packet);
  av_packet_free(&packet);
  av_frame_unref(frame);
  av_frame_free(&frame);

  SDL_PollEvent(&m_playerContext.event);
  switch(m_playerContext.event.type)
  {
    case SDL_QUIT:
    {
      SDL_Quit();
      exit(0);
    }
    break;
  }

  // Then continue, to request the next frame of data:
  continuePlaying();
}

Boolean SdlVideoSink::continuePlaying()
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

bool SdlVideoSink::initDecRender()
{
#if 1
  const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
  assert(codec);

  m_videoCodecContext = avcodec_alloc_context3(codec);
  assert(m_videoCodecContext);
  if (avcodec_open2(m_videoCodecContext, codec, nullptr) < 0)
  {
    std::cerr << "Failed to avcodec_open2 func." << std::endl;
    return false;
  }

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER))
  {
    exit(-1);
  }

  m_playerContext = {nullptr};
  m_playerContext.window = SDL_CreateWindow("Player", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, 0);
  m_playerContext.renderer = SDL_CreateRenderer(m_playerContext.window, -1, 0);
  m_playerContext.texture = SDL_CreateTexture(m_playerContext.renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, 640, 480);
  m_playerContext.mutex = SDL_CreateMutex();

#else
  m_videoCodecContext = avcodec_alloc_context3(nullptr);
  assert(m_videoCodecContext);
  auto sprops = m_subsession.fmtp_spropparametersets();
  unsigned num = 1;
  auto records = parseSPropParameterSets(sprops, num);
#endif

  return true;
}

