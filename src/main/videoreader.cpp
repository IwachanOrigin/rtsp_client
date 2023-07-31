
#include <thread>
#include "videoreader.h"

#define MAX_QUEUE_SIZE (15 * 1024 * 1024)

VideoReader::VideoReader()
  : m_videoDecoder(nullptr)
  , m_videoRenderer(nullptr)
  , m_videoState(new VideoState())
{
}

VideoReader::~VideoReader()
{
}

int VideoReader::start(const std::string filename, const int audioDeviceIndex)
{
  if (m_videoState == nullptr)
  {
    return -1;
  }

  // set filename
  m_videoState->filename = filename;

  // set output audio device index
  m_videoState->output_audio_device_index = audioDeviceIndex;

  // start read thread
  std::thread([&](VideoReader *reader)
    {
      reader->read_thread(m_videoState);
    }, this).detach();

  return 0;
}

int VideoReader::read_thread(void *arg)
{
  int ret = -1;

  // retrieve global VideoState reference
  VideoState *videoState = (VideoState *)arg;
  videoState->quit = 0;

  // video and audio stream indexes
  int videoStream = -1;
  int audioStream = -1;
  AVPacket* packet = nullptr;

  AVFormatContext* pFormatCtx = nullptr;
  AVDictionary* options = nullptr;
  av_dict_set(&options, "rtsp_transport", "tcp", 0);
  ret = avformat_open_input(&pFormatCtx, videoState->filename.c_str(), nullptr, &options);
  if (ret < 0)
  {
    std::cerr << "Could not open file " << videoState->filename << std::endl;
    return -1;
  }
  av_dict_free(&options);
  options = nullptr;

  // reset streamindex
  videoState->videoStream = -1;
  videoState->audioStream = -1;

  // set the avformatcontext for the global videostate ref
  videoState->pFormatCtx = pFormatCtx;

  // read packets of the media file to get stream info
  ret = avformat_find_stream_info(pFormatCtx, nullptr);
  if (ret < 0)
  {
    std::cerr << "Could not find stream info " << videoState->filename << std::endl;
    return -1;
  }

  // dump info about file onto standard error
  av_dump_format(pFormatCtx, 0, videoState->filename.c_str(), 0);

  // loop through the streams that have been found
  for (int i = 0; i < pFormatCtx->nb_streams; i++)
  {
    // look for the video stream
    if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && videoStream < 0)
    {
      videoStream = i;
    }

    // look for the audio stream
    if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audioStream < 0)
    {
      audioStream = i;
    }
  }

  // return with error in case no video stream was found
  if (videoStream == -1)
  {
    std::cerr << "Could not open video stream" << std::endl;
    goto fail;
  }
  else
  {
    // open video stream
    ret = stream_component_open(videoState, videoStream);

    // check video codec was opened correctly
    if (ret < 0)
    {
      std::cerr << "Could not find video codec" << std::endl;
      goto fail;
    }

    m_videoRenderer = new VideoRenderer();
    m_videoRenderer->start(videoState);
  }

  // return with error in case no audio stream was found
  if (audioStream == -1)
  {
    std::cerr << "Could not find audio stream" << std::endl;
    goto fail;
  }
  else
  {
    // open audio stream component codec
    ret = stream_component_open(videoState, audioStream);

    // check audio codec was opened correctly
    if (ret < 0)
    {
      std::cerr << "Could not find audio codec" << std::endl;
      goto fail;
    }
  }
  if (videoState->videoStream < 0 || videoState->audioStream < 0)
  {
    std::cerr << "Could not open codecs " << videoState->filename << std::endl;
    goto fail;
  }

  packet = av_packet_alloc();
  if (packet == nullptr)
  {
    std::cerr << "Could not alloc packet" << std::endl;
    goto fail;
  }

  // main decode loop. read in a packet and put it on the queue
  for (;;)
  {
    // check global quit flag
    if (videoState->quit)
    {
      break;
    }

    // seek stuff goes here
    if (videoState->seek_req)
    {
      int video_stream_index = -1;
      int audio_stream_index = -1;
      int64_t seek_target_video = videoState->seek_pos;
      int64_t seek_target_audio = videoState->seek_pos;

      if (videoState->videoStream >= 0)
      {
        video_stream_index = videoState->videoStream;
      }

      if (videoState->audioStream >= 0)
      {
        audio_stream_index = videoState->audioStream;
      }

      if (video_stream_index >= 0 && audio_stream_index >= 0)
      {
        // MSVC does not support compound literals like AV_TIME_BASE_Q in C++ code (compiler error C4576)
        AVRational timebase;
        timebase.num = 1;
        timebase.den = AV_TIME_BASE;

        seek_target_video = av_rescale_q(
          seek_target_video
          , timebase
          , pFormatCtx->streams[video_stream_index]->time_base);
        seek_target_audio = av_rescale_q(
          seek_target_audio
          , timebase
          , pFormatCtx->streams[audio_stream_index]->time_base);
      }

      ret = av_seek_frame(
        videoState->pFormatCtx
        , video_stream_index
        , seek_target_video
        , videoState->seek_flags);
      ret &= av_seek_frame(
        videoState->pFormatCtx
        , audio_stream_index
        , seek_target_audio
        , videoState->seek_flags);

      if (ret < 0)
      {
        //
      }
      else
      {
        if (videoState->videoStream >= 0)
        {
          videoState->videoq.flush();
          videoState->videoq.put(videoState->flush_pkt);
        }

        if (videoState->audioStream >= 0)
        {
          videoState->audioq.flush();
          videoState->audioq.put(videoState->flush_pkt);
        }
        videoState->seek_req = 0;
      }
    }

    // check audio and video packets queues size
    if (videoState->audioq.size + videoState->videoq.size > MAX_QUEUE_SIZE)
    {
      // wait for audio and video queues to decrease size
      SDL_Delay(10);
      continue;
    }
    // read data from the AVFormatContext by repeatedly calling av_read_frame
    ret = av_read_frame(videoState->pFormatCtx, packet);
    if (ret < 0)
    {
      if (ret == AVERROR_EOF)
      {
        // wait for the rest of the program to end
        while (videoState->videoq.nb_packets > 0 && videoState->audioq.nb_packets > 0)
        {
          SDL_Delay(10);
        }

        // media EOF reached, quit
        videoState->quit = 1;
        break;
      }
      else if (videoState->pFormatCtx->pb->error == 0)
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
    if (packet->stream_index == videoState->videoStream)
    {
      videoState->videoq.put(packet);
    }
    else if (packet->stream_index == videoState->audioStream)
    {
      videoState->audioq.put(packet);
    }
    else
    {
      // otherwise free the memory
      av_packet_unref(packet);
    }
  }

  // wait for the rest of the program to end
  while (!videoState->quit)
  {
    SDL_Delay(100);
  }

fail:
  {
    // device stop, memory release
    if (deviceID > 0)
    {
      SDL_LockAudioDevice(deviceID);
      SDL_PauseAudioDevice(deviceID, 1);
      SDL_UnlockAudioDevice(deviceID);

      SDL_CloseAudioDevice(deviceID);
    }
    deviceID = 0;

    SDL_Quit();

    if (packet)
    {
      av_packet_free(&packet);
    }
    packet = nullptr;

    if (pFormatCtx)
    {
      // close the opened input avformatcontext
      avformat_close_input(&pFormatCtx);
    }
    pFormatCtx = nullptr;
    // clear queue
    videoState->videoq.clear();
    videoState->audioq.clear();

    if (videoState->audio_ctx)
    {
      avcodec_free_context(&videoState->audio_ctx);
    }
    videoState->audio_ctx = nullptr;

    if (videoState->video_ctx)
    {
      avcodec_free_context(&videoState->video_ctx);
    }
    videoState->video_ctx = nullptr;
    // clean up memory
    //av_free(videoState);

  }
  return 0;
}

int VideoReader::stream_component_open(VideoState *videoState, int stream_index)
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

  // use multi core
  // fhd, 60p = 1 threads
  // 4k, 60p  = 4 threads
  //codecCtx->thread_count = 4;
  //codecCtx->thread_type = FF_THREAD_FRAME;

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
    wants.callback = audio_callback;
    wants.userdata = videoState;

    // open audio device
    deviceID = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(m_videoState->output_audio_device_index, 0), false, &wants, &spec, 0);
    if (deviceID <= 0)
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
      SDL_PauseAudioDevice(deviceID, 0);
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
