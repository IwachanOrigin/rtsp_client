
#include "audiosink.h"
#include <H264VideoRTPSource.hh>
#include <cassert>
#include <iostream>

using namespace client;

// Even though we're not going to be doing anything with the incoming data, we still need to receive it.
// Define the size of the buffer that we'll use:
#define DUMMY_SINK_RECEIVE_BUFFER_SIZE 1000000

AudioSink* AudioSink::createNew(UsageEnvironment& env, MediaSubsession& subsession,  char const* streamURL, std::shared_ptr<FrameContainer> frameContainer)
{
  return new AudioSink(env, subsession, streamURL, frameContainer);
}

AudioSink::AudioSink(UsageEnvironment& env, MediaSubsession& subsession, char const* streamURL, std::shared_ptr<FrameContainer> frameContainer)
  : MediaSink(env)
  , m_subsession(subsession)
  , m_frameContainer(frameContainer)
{
  m_streamURL = strDup(streamURL);
  m_receiveBuffer = new u_int8_t[DUMMY_SINK_RECEIVE_BUFFER_SIZE];
  this->init(streamURL);
}

AudioSink::~AudioSink()
{
  delete[] m_receiveBuffer;
  delete[] m_streamURL;

  if (m_audioCodecContext)
  {
    avcodec_close(m_audioCodecContext);
    m_audioCodecContext = nullptr;
  }
}

void AudioSink::afterGettingFrame(
  void* clientData
  , unsigned frameSize
  , unsigned numTruncatedBytes
  , struct timeval presentationTime
  , unsigned durationInMicroseconds
  )
{
  AudioSink* sink = (AudioSink*)clientData;
  sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

void AudioSink::afterGettingFrame(
  unsigned frameSize
  , unsigned numTruncatedBytes
  , struct timeval presentationTime
  , unsigned /*durationInMicroseconds*/)
{
  if (m_frameContainer->sizeAudioFrameDecoded() > 50)
  {
    std::chrono::milliseconds ms(1000);
    std::this_thread::sleep_for(ms);
  }

  auto packet = av_packet_alloc();

  packet->size = frameSize;
  packet->data = new uint8_t[frameSize];
  std::memcpy(packet->data, m_receiveBuffer, frameSize);

  if (avcodec_send_packet(m_audioCodecContext, packet) != 0)
  {
    envir() << "Failed to send packet to decoder." << "\n";
  }

  auto frame = av_frame_alloc();
  while (avcodec_receive_frame(m_audioCodecContext, frame) == 0)
  {
    // To queue
    m_frameContainer->pushAudioFrameDecoded(frame);
    //envir() << "audio decoded." << "\n";
  }

  av_packet_unref(packet);
  av_packet_free(&packet);
  av_frame_unref(frame);
  av_frame_free(&frame);

  // Then continue, to request the next frame of data:
  continuePlaying();
}

Boolean AudioSink::continuePlaying()
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

bool AudioSink::init(char const* streamId)
{
  AVFormatContext* pFormatCtx = nullptr;
  auto ret = avformat_open_input(&pFormatCtx, streamId, nullptr, nullptr);
  if (ret < 0)
  {
    std::cerr << "Could not open url " << streamId << std::endl;
    return false;
  }

  // Read packets of the media file to get stream info
  ret = avformat_find_stream_info(pFormatCtx, nullptr);
  if (ret < 0)
  {
    std::cerr << "Could not find stream info " << streamId << std::endl;
    this->releaseFormatCtx(pFormatCtx);
    return false;
  }

  // Loop through the streams that have been found
  auto audioStream = -1;
  for (int i = 0; i < pFormatCtx->nb_streams; i++)
  {
    // Look for the audio stream
    if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audioStream < 0)
    {
      audioStream = i;
    }
  }

  if (audioStream < 0)
  {
    std::cerr << "Could not find audio stream index." << std::endl;
    this->releaseFormatCtx(pFormatCtx);
    return false;
  }

  // Retrieve codec for the given stream index
  const AVCodec* codec = avcodec_find_decoder(pFormatCtx->streams[audioStream]->codecpar->codec_id);
  assert(codec);
  if (codec == nullptr)
  {
    std::cerr << "Unsupported codec" << std::endl;
    this->releaseFormatCtx(pFormatCtx);
    return false;
  }

  m_audioCodecContext = avcodec_alloc_context3(codec);
  assert(m_audioCodecContext);

  ret = avcodec_parameters_to_context(m_audioCodecContext, pFormatCtx->streams[audioStream]->codecpar);
  if (ret != 0)
  {
    std::cerr << "Could not copy codec context" << std::endl;
    this->releaseFormatCtx(pFormatCtx);
    return false;
  }

  if (avcodec_open2(m_audioCodecContext, codec, nullptr) < 0)
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

void AudioSink::releaseFormatCtx(AVFormatContext*& formatctx)
{
  if (formatctx)
  {
    // Close the opened input AVFormatContext
    avformat_close_input(&formatctx);
  }
  formatctx = nullptr;
}
