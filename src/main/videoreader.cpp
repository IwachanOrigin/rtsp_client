
#include <cstring>
#include <thread>
#include <utility>

#include "videostate.h"
#include "videoreader.h"

#include "audiodecoder.h"
#include "audioresamplingstate.h"

#define MAX_QUEUE_SIZE (15 * 1024 * 1024)

using namespace player;

int VideoReader::start(const std::string& filename, const Options& opt)
{
  m_videoState = std::make_unique<VideoState>();
  if (m_videoState == nullptr)
  {
    return -1;
  }

  m_filename = filename;
  // set output audio device index
  m_videoState->setOutputAudioDeviceIndex(opt.audioIndex);

  // start read thread
  std::thread([&](VideoReader* reader)
  {
    reader->readThread(m_videoState, opt);
  }, this).detach();

  return 0;
}

void VideoReader::stop()
{
  if (m_videoRenderer)
  {
    m_videoRenderer->stop();
  }

  if (m_videoDecoder)
  {
    m_videoDecoder->stop();
  }

  if (m_videoState)
  {
    m_videoState->clearAudioPacketRead();
    m_videoState->clearVideoPacketRead();
  }

  m_isFinished = true;
}

int VideoReader::readThread(std::shared_ptr<VideoState> vs, const Options& opt)
{
  int ret = -1;

  // retrieve global VideoState reference
  auto& videoState = vs;

  // Set the AVFormatContext for the global videostate ref
  auto& formatCtx = videoState->formatCtx();
  AVDictionary* options = nullptr;

  if (opt.scanAllPmts)
  {
    // 'scan_all_pmts' is an option primarily related to streaming MPEG-TS and reading files.
    // When enabled, all PMTs are scanned, not just the first PMT.
    // For MPEG-TS with multiple PMTs, all stream information can be retrieved.
    // This option is always set in ffplay.
    av_dict_set(&options, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);
  }

  if (opt.rtspTransport)
  {
    // 'rtsp_transport' sets the receive protocol for the RTSP stream.
    // The default is to use UDP.
    av_dict_set(&options, "rtsp_transport", "tcp", 0);
  }

  if (opt.maxDelay > 0)
  {
    // Sets the maximum delay time.
    // The unit is us.
    av_dict_set(&options, "max_delay", std::to_string(opt.maxDelay).c_str(), 0);
  }

  if (opt.stimeout > 0)
  {
    // set timeout (in microseconds) of socket TCP I/O operations.
    av_dict_set(&options, "stimeout", std::to_string(opt.stimeout).c_str(), 0);
  }

  if (opt.bufferSize > 0)
  {
    // Underlying protocol send/receive buffer size.
    av_dict_set(&options, "buffer_size", std::to_string(opt.bufferSize).c_str(), 0);
  }

  ret = avformat_open_input(&formatCtx, m_filename.c_str(), nullptr, &options);
  if (ret < 0)
  {
    std::cerr << "Could not open file " << m_filename << std::endl;
    return -1;
  }

  av_dict_free(&options);
  options = nullptr;

  // interrupt_callback is a callback function for checking interrupted I/O operations.
  formatCtx->interrupt_callback.callback = decodeInterruptCB;
  formatCtx->interrupt_callback.opaque = videoState.get();

  // reset streamindex
  auto& videoStreamIndex = videoState->videoStreamIndex();
  auto& audioStreamIndex = videoState->audioStreamIndex();
  videoStreamIndex = -1;
  audioStreamIndex = -1;

  // read packets of the media file to get stream info
  ret = avformat_find_stream_info(formatCtx, nullptr);
  if (ret < 0)
  {
    std::cerr << "Could not find stream info " << m_filename << std::endl;
    return -1;
  }

  // dump info about file onto standard error
  av_dump_format(formatCtx, 0, m_filename.c_str(), 0);

  // loop through the streams that have been found
  for (int i = 0; i < formatCtx->nb_streams; i++)
  {
    // look for the video stream
    if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && videoStreamIndex < 0)
    {
      videoStreamIndex = i;
    }

    // look for the audio stream
    if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audioStreamIndex < 0)
    {
      audioStreamIndex = i;
    }
  }

  // return with error in case no video stream was found
  if (videoStreamIndex == -1)
  {
    std::cerr << "Could not open video stream" << std::endl;
    return -1;
  }
  
  // open video stream
  ret = streamComponentOpen(videoState, videoStreamIndex);

  // check video codec was opened correctly
  if (ret < 0)
  {
    std::cerr << "Could not find video codec" << std::endl;
    return -1;
  }

  m_videoRenderer = std::make_unique<VideoRenderer>();
  m_videoRenderer->start(videoState);

  // return with error in case no audio stream was found
  if (audioStreamIndex == -1)
  {
    std::cerr << "Could not find audio stream" << std::endl;
    videoState->setSyncType(player::SYNC_TYPE::AV_SYNC_EXTERNAL_MASTER);
  }
  else
  {
    // open audio stream component codec
    ret = streamComponentOpen(videoState, audioStreamIndex);
    // check audio codec was opened correctly
    if (ret < 0)
    {
      std::cerr << "Could not find audio codec" << std::endl;
      return -1;
    }
  }

  AVPacket* packet = av_packet_alloc();
  if (packet == nullptr)
  {
    std::cerr << "Could not alloc packet" << std::endl;
    return -1;
  }

  // main decode loop. read in a packet and put it on the queue
  for (;;)
  {
    {
      if (m_videoState->isPlayerFinished())
      {
        m_isFinished = true;
        break;
      }
    }
    // seek stuff goes here
    auto seekReq = videoState->seekRequest();
    if (seekReq)
    {
      auto seekTargetVideo = videoState->seekPos();
      auto seekTargetAudio = videoState->seekPos();

      if (videoStreamIndex >= 0 && audioStreamIndex >= 0)
      {
        // MSVC does not support compound literals like AV_TIME_BASE_Q in C++ code (compiler error C4576)
        AVRational timebase{};
        timebase.num = 1;
        timebase.den = AV_TIME_BASE;

        seekTargetVideo = av_rescale_q(
          seekTargetVideo
          , timebase
          , formatCtx->streams[videoStreamIndex]->time_base);
        seekTargetAudio = av_rescale_q(
          seekTargetAudio
          , timebase
          , formatCtx->streams[audioStreamIndex]->time_base);
      }

      ret = av_seek_frame(
        formatCtx
        , videoStreamIndex
        , seekTargetVideo
        , videoState->seekFlags());
      ret &= av_seek_frame(
        formatCtx
        , audioStreamIndex
        , seekTargetAudio
        , videoState->seekFlags());

      if (ret >= 0)
      {
        if (videoStreamIndex >= 0)
        {
          auto& flushPacket = videoState->flushPacket();
          videoState->clearVideoPacketRead();
          videoState->pushVideoPacketRead(flushPacket);
        }

        if (audioStreamIndex >= 0)
        {
          auto& flushPacket = videoState->flushPacket();
          videoState->clearAudioPacketRead();
          videoState->pushAudioPacketRead(flushPacket);
        }
        videoState->setSeekRequest(0);
      }
    }

    // check audio and video packets queues size
    if (videoState->sizeAudioPacketRead() + videoState->sizeVideoPacketRead() > MAX_QUEUE_SIZE)
    {
      // wait for audio and video queues to decrease size
      SDL_Delay(10);
      continue;
    }
    // read data from the AVFormatContext by repeatedly calling av_read_frame
    ret = av_read_frame(formatCtx, packet);
    if (ret < 0)
    {
      if (ret == AVERROR_EOF)
      {
        // wait for the rest of the program to end
        while (videoState->nbPacketsAudioRead() > 0 && videoState->nbPacketsVideoRead() > 0)
        {
          SDL_Delay(10);
        }

        // media EOF reached, quit
        break;
      }
      else if (formatCtx->pb->error == 0)
      {
        // no read error, wait for user input
        SDL_Delay(10);
        continue;
      }
      else
      {
        // exit for loop in case of error
        break;
      }
    }

    // put the packet in the appropriate queue
    if (packet->stream_index == videoStreamIndex)
    {
      videoState->pushVideoPacketRead(packet);
    }
    else if (packet->stream_index == audioStreamIndex)
    {
      videoState->pushAudioPacketRead(packet);
    }
    else
    {
      // Otherwise free the memory
      av_packet_unref(packet);
    }
  }

  // Wait for the rest of the program to end
  while (!videoState->isPlayerFinished())
  {
    SDL_Delay(100);
  }

  return 0;
}

int VideoReader::streamComponentOpen(std::shared_ptr<VideoState> vs, const int& streamIndex)
{
  // retrieve file I/O context
  auto& formatCtx = vs->formatCtx();

  // check the given stream index in valid
  if (streamIndex < 0 || streamIndex >= formatCtx->nb_streams)
  {
    std::cerr << "invalid stream index" << std::endl;
    return -1;
  }

  // retrieve codec for the given stream index
  const AVCodec* codec = avcodec_find_decoder(formatCtx->streams[streamIndex]->codecpar->codec_id);
  if (codec == nullptr)
  {
    std::cerr << "unsupported codec" << std::endl;
    return -1;
  }

  // retrieve codec context
  AVCodecContext* codecCtx = avcodec_alloc_context3(codec);

  // use multi core
  // fhd, 60p = 1 threads
  // 4k, 60p  = 4 threads
  //codecCtx->thread_count = 4;
  //codecCtx->thread_type = FF_THREAD_FRAME;

  int ret = avcodec_parameters_to_context(codecCtx, formatCtx->streams[streamIndex]->codecpar);
  if (ret != 0)
  {
    std::cerr << "Could not copy codec context" << std::endl;
    return -1;
  }

  // init the AVCodecContext to use the given AVCodec
  if (avcodec_open2(codecCtx, codec, nullptr) < 0)
  {
    std::cerr << "unsupported codec" << std::endl;
    return -1;
  }

  switch (codecCtx->codec_type)
  {
    case AVMEDIA_TYPE_AUDIO:
    {
      auto& audioCodecCtx = vs->audioCodecCtx();
      audioCodecCtx = std::move(codecCtx);
      auto& audioStream = vs->audioStream();
      audioStream = formatCtx->streams[streamIndex];

      SDL_AudioSpec wants{};
      SDL_AudioSpec spec{};
      wants.freq = audioCodecCtx->sample_rate;
      wants.format = AUDIO_S16SYS;
      wants.channels = audioCodecCtx->ch_layout.nb_channels;
      wants.silence = 0;
      wants.samples = SDL_AUDIO_BUFFER_SIZE;
      wants.callback = audioCallback;
      wants.userdata = vs.get();

      // open audio device
      auto outputAudioDeviceIndex = vs->outputAudioDeviceIndex();
      auto sdlAudioDeviceID = vs->sdlAudioDeviceID();
      sdlAudioDeviceID = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(outputAudioDeviceIndex, 0), false, &wants, &spec, 0);
      if (sdlAudioDeviceID <= 0)
      {
        ret = -1;
        return -1;
      }
      vs->setSdlAudioDeviceID(sdlAudioDeviceID);

      // start playing audio device
      SDL_PauseAudioDevice(sdlAudioDeviceID, 0);
    }
    break;

    case AVMEDIA_TYPE_VIDEO:
    {
      // !!! Don't forget to init the frame timer
      // previous frame delay: 1ms = 1e-6s
      vs->setFrameDecodeTimer((double)av_gettime() / 1000000.0);
      vs->setFrameDecodeLastDelay(40e-3);
      vs->setVideoDecodeCurrentPtsTime(av_gettime());

      auto& videoCodecCtx = vs->videoCodecCtx();
      videoCodecCtx = std::move(codecCtx);
      auto& videoStream = vs->videoStream();
      videoStream = formatCtx->streams[streamIndex];

      // start video thread
      m_videoDecoder = std::make_unique<VideoDecoder>();
      m_videoDecoder->start(vs);

      // set up the videostate swscontext to convert the image data to yuv420
      auto& decodeVideoSwsCtx = vs->decodeVideoSwsCtx();
      decodeVideoSwsCtx = sws_getContext(
        videoCodecCtx->width
        , videoCodecCtx->height
        , videoCodecCtx->pix_fmt
        , videoCodecCtx->width
        , videoCodecCtx->height
        , AV_PIX_FMT_YUV420P
        , SWS_BILINEAR
        , nullptr
        , nullptr
        , nullptr);
    }
    break;
  }
  return 0;
}

int VideoReader::decodeInterruptCB(void* videoState)
{
  auto vs = static_cast<VideoState*>(videoState);
  if (vs)
  {
    return vs->isPlayerFinished();
  }
  return 1;
}