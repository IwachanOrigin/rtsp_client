
#include <thread>
#include "videoreader.h"

#define MAX_QUEUE_SIZE (15 * 1024 * 1024)

VideoReader::VideoReader()
  : m_videoDecoder(nullptr)
  , m_videoRenderer(nullptr)
  , m_videoState(nullptr)
  , m_deviceID(0)
{
}

VideoReader::~VideoReader()
{
}

int VideoReader::start(VideoState* videoState, const Options& opt)
{
  if (videoState == nullptr)
  {
    return -1;
  }
  m_videoState = videoState;

  // set output audio device index
  m_videoState->output_audio_device_index = opt.audioIndex;

  // start read thread
  std::thread([&](VideoReader *reader, const Options& opt)
  {
    reader->readThread(m_videoState, opt);
  }, this, opt).detach();

  return 0;
}

int VideoReader::readThread(void *arg, const Options& opt)
{
  int ret = -1;

  // Retrieve global VideoState reference
  VideoState *videoState = (VideoState *)arg;
  videoState->quit = 0;

  int videoStream = -1;
  int audioStream = -1;

  AVFormatContext* pFormatCtx = nullptr;
  AVDictionary* options = nullptr;

  pFormatCtx = avformat_alloc_context();
  if (!pFormatCtx)
  {
    std::cerr << "Failed to alloc avformat context." << std::endl;
    return -1;
  }
  // interrupt_callback is a callback function for checking interrupted I/O operations.
  pFormatCtx->interrupt_callback.callback = decodeInterruptCB;
  pFormatCtx->interrupt_callback.opaque = videoState;

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

  ret = avformat_open_input(&pFormatCtx, videoState->filename.c_str(), nullptr, &options);
  if (ret < 0)
  {
    std::cerr << "Could not open file " << videoState->filename << std::endl;
    return -1;
  }
  av_dict_free(&options);
  options = nullptr;

  // Reset streamindex
  videoState->videoStream = -1;
  videoState->audioStream = -1;

  // Set the avformatcontext for the global videostate ref
  videoState->pFormatCtx = pFormatCtx;

  // Read packets of the media file to get stream info
  ret = avformat_find_stream_info(pFormatCtx, nullptr);
  if (ret < 0)
  {
    std::cerr << "Could not find stream info " << videoState->filename << std::endl;
    return -1;
  }

  // Dump info about file onto standard error
  av_dump_format(pFormatCtx, 0, videoState->filename.c_str(), 0);

  // Loop through the streams that have been found
  for (int i = 0; i < pFormatCtx->nb_streams; i++)
  {
    // Look for the video stream
    if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && videoStream < 0)
    {
      videoStream = i;
    }

    // Look for the audio stream
    if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audioStream < 0)
    {
      audioStream = i;
    }
  }

  // Return with error in case no video stream was found
  if (videoStream == -1)
  {
    std::cerr << "Could not open video stream" << std::endl;
    this->releasePointer();
    return -1;
  }
  else
  {
    // Open video stream
    ret = this->streamComponentOpen(videoState, videoStream);
    if (ret < 0)
    {
      std::cerr << "Could not find video codec" << std::endl;
      this->releasePointer();
      return -1;
    }

    m_videoRenderer = new VideoRenderer();
    m_videoRenderer->start(videoState);
  }

  // Return with error in case no audio stream was found
  if (audioStream == -1)
  {
    std::cerr << "Could not find audio stream" << std::endl;
    this->releasePointer();
    return -1;
  }
  else
  {
    // Open audio stream component codec
    ret = this->streamComponentOpen(videoState, audioStream);
    if (ret < 0)
    {
      std::cerr << "Could not find audio codec" << std::endl;
      this->releasePointer();
      return -1;
    }
  }
  if (videoState->videoStream < 0 || videoState->audioStream < 0)
  {
    std::cerr << "Could not open codecs " << videoState->filename << std::endl;
    this->releasePointer();
    return -1;
  }

  m_packet = av_packet_alloc();
  if (m_packet == nullptr)
  {
    std::cerr << "Could not alloc packet" << std::endl;
    this->releasePointer();
    return -1;
  }

  // Main decode loop. read in a packet and put it on the queue
  for (;;)
  {
    // Check global quit flag
    if (videoState->quit)
    {
      break;
    }

    // Check audio and video packets queues size
    if (videoState->audioq.size + videoState->videoq.size > MAX_QUEUE_SIZE)
    {
      // Wait for audio and video queues to decrease size
      SDL_Delay(10);
      continue;
    }
    // Read data from the AVFormatContext by repeatedly calling av_read_frame
    ret = av_read_frame(videoState->pFormatCtx, m_packet);
    if (ret < 0)
    {
      if (ret == AVERROR_EOF)
      {
        // Wait for the rest of the program to end
        while (videoState->videoq.nb_packets > 0 && videoState->audioq.nb_packets > 0)
        {
          SDL_Delay(10);
        }

        // Media EOF reached, quit
        videoState->quit = 1;
        break;
      }
      else if (!videoState->pFormatCtx->pb && videoState->pFormatCtx->pb->error == 0)
      {
        // No read error, wait for user input
        SDL_Delay(10);
        continue;
      }
      else
      {
        // Exit for loop in case of error
        break;
      }
    }

    // Put the packet in the appropriate queue
    if (m_packet->stream_index == videoState->videoStream)
    {
      videoState->videoq.put(m_packet);
    }
    else if (m_packet->stream_index == videoState->audioStream)
    {
      videoState->audioq.put(m_packet);
    }
    else
    {
      // Otherwise free the memory
      av_packet_unref(m_packet);
    }
  }

  // Wait for the rest of the program to end
  while (!videoState->quit)
  {
    SDL_Delay(100);
  }

  this->releasePointer();
  return 0;
}

int VideoReader::streamComponentOpen(VideoState *videoState, int stream_index)
{
  // retrieve file I/O context
  AVFormatContext *pFormatCtx = videoState->pFormatCtx;

  // check the given stream index in valid
  if (stream_index < 0 || stream_index >= pFormatCtx->nb_streams)
  {
    std::cerr << "invalid stream index" << std::endl;
    return -1;
  }

  // retrieve codec for the given stream index
  const AVCodec* codec = nullptr;
  codec = avcodec_find_decoder(pFormatCtx->streams[stream_index]->codecpar->codec_id);
  if (codec == nullptr)
  {
    std::cerr << "unsupported codec" << std::endl;
    return -1;
  }

  // retrieve codec context
  AVCodecContext *codecCtx = nullptr;
  codecCtx = avcodec_alloc_context3(codec);

  int ret = avcodec_parameters_to_context(codecCtx, pFormatCtx->streams[stream_index]->codecpar);
  if (ret != 0)
  {
    std::cerr << "Could not copy codec context" << std::endl;
    return -1;
  }

  if (codecCtx->codec_type == AVMEDIA_TYPE_AUDIO)
  {
    SDL_AudioSpec wants;
    SDL_AudioSpec spec;

    wants.freq = codecCtx->sample_rate;
    wants.format = AUDIO_S16SYS;
    wants.channels = codecCtx->ch_layout.nb_channels;
    wants.silence = 0;
    wants.samples = SDL_AUDIO_BUFFER_SIZE;
    wants.callback = audioCallback;
    wants.userdata = videoState;

    // open audio device
    m_deviceID = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(m_videoState->output_audio_device_index, 0), false, &wants, &spec, 0);
    if (m_deviceID <= 0)
    {
      ret = -1;
      return -1;
    }
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
      // set videostate audio
      videoState->audioStream = stream_index;
      videoState->audio_st = pFormatCtx->streams[stream_index];
      videoState->audio_ctx = codecCtx;
      videoState->audio_buf_size = 0;
      videoState->audio_buf_index = 0;
      videoState->av_sync_type = DEFAULT_AV_SYNC_TYPE;

      // zero out the block of memory pointed
      std::memset(&videoState->audio_pkt, 0, sizeof(videoState->audio_pkt));

      // init audio pkt queue
      videoState->audioq.init();

      // start playing audio device
      SDL_PauseAudioDevice(m_deviceID, 0);
    }
    break;

    case AVMEDIA_TYPE_VIDEO:
    {
      // set videostate video
      videoState->videoStream = stream_index;
      videoState->video_st = pFormatCtx->streams[stream_index];
      videoState->video_ctx = codecCtx;

      // !!! Don't forget to init the frame timer
      // previous frame delay: 1ms = 1e-6s
      videoState->frame_timer = (double)av_gettime() / 1000000.0;
      videoState->frame_last_delay = 40e-3;
      videoState->video_current_pts_time = av_gettime();

      // init video packet queue
      videoState->videoq.init();

      // start video thread
      m_videoDecoder = new VideoDecoder();
      m_videoDecoder->start(videoState);

      // set up the videostate swscontext to convert the image data to yuv420
      videoState->sws_ctx = sws_getContext(videoState->video_ctx->width
                                           , videoState->video_ctx->height
                                           , videoState->video_ctx->pix_fmt
                                           , videoState->video_ctx->width
                                           , videoState->video_ctx->height
                                           , AV_PIX_FMT_YUV420P
                                           , SWS_BILINEAR
                                           , nullptr
                                           , nullptr
                                           , nullptr);
      // init sdl_surface mutex ref
      videoState->screen_mutex = SDL_CreateMutex();

    }
    break;

    default:
    {
      // nothing
    }
  }
  return 0;
}

void VideoReader::releasePointer()
{
  // Device stop, memory release
  if (m_deviceID > 0)
  {
    SDL_LockAudioDevice(m_deviceID);
    SDL_PauseAudioDevice(m_deviceID, 1);
    SDL_UnlockAudioDevice(m_deviceID);

    SDL_CloseAudioDevice(m_deviceID);
  }
  m_deviceID = 0;

  SDL_Quit();

  if (m_packet)
  {
    m_packet = nullptr;
  }
}

int VideoReader::decodeInterruptCB(void* videoState)
{
  VideoState* is = (VideoState*)videoState;
  return is->quit;
}

