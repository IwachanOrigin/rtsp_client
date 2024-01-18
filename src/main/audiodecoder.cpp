
#include <cstring>
#include <thread>
#include <cassert>
#include "audiodecoder.h"

using namespace player;

#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE  192000
#define AV_NOSYNC_THRESHOLD   1.0
#define AUDIO_DIFF_AVG_NB     20
#define SAMPLE_CORRECTION_PERCENT_MAX 10

void player::audioCallback(void* userdata, Uint8* stream, int len)
{
  // retrieve the videostate
  VideoState* videoState = (VideoState *) userdata;

  int len1 = -1;
  int audioSize = -1;

  double pts = 0;
  uint8_t* audioBuf = nullptr;
  auto audioArrayBufSize = videoState->audioArrayBufSize();
  unsigned int audioBufIndex = 0;

  while (len > 0)
  {
    if (videoState->isPlayerFinished())
    {
      auto sdlAudioDeviceID = videoState->sdlAudioDeviceID();
      SDL_PauseAudioDevice(sdlAudioDeviceID, 1);
      return;
    }

    audioBufIndex = videoState->audioBufIndex();
    auto audioBufSize = videoState->audioBufSize();
    if (audioBufIndex >= audioBufSize)
    {
      // we have already sent all avaialble data; get more
      audioBuf = videoState->audioArrayBuf(); 
      audioSize = player::audioDecodeFrame(videoState, audioBuf, audioArrayBufSize, pts);

      if (audioSize < 0)
      {
        // output silence
        audioBufSize = 1024;
        videoState->setAudioBufSize(audioBufSize);

        // clear memory
        std::memset(audioBuf, 0, audioBufSize);
      }
      else
      {
        audioSize = player::syncAudio(videoState, (int16_t *)audioBuf, audioSize);
        audioBufSize = audioSize;
        videoState->setAudioBufSize(audioBufSize);
      }
      audioBufIndex = 0;
      videoState->setAudioBufIndex(audioBufIndex);
    }

    len1 = audioBufSize - audioBufIndex;

    if (len1 > len)
    {
      len1 = len;
    }

    // copy data from audio buffer to the SDL stream
    std::memcpy(stream, (uint8_t *)audioBuf + audioBufIndex, len1);

    len -= len1;
    stream += len1;
    videoState->setAudioBufIndex(audioBufIndex + len1);
  }
}

int player::audioDecodeFrame(VideoState* vs, uint8_t* audio_buf, int bufSize, double& pts_ptr)
{
  AVPacket* avPacket = av_packet_alloc();
  static uint8_t* audioPktData = nullptr;
  static int audioPktSize = 0;

  int n = 0;

  // allocate a new frame, used to decode audio packets
  AVFrame* avFrame = av_frame_alloc();
  if (!avFrame)
  {
    return -1;
  }

  int len1 = 0;
  int dataSize = 0;

  auto& audioCodecCtx = vs->audioCodecCtx();

  for (;;)
  {
    while (audioPktSize > 0)
    {
      int got_frame = 0;

      int ret = avcodec_receive_frame(audioCodecCtx, avFrame);
      if (ret == 0)
      {
        got_frame = 1;
      }

      if (ret == AVERROR(EAGAIN))
      {
        ret = 0;
      }

      if (ret == 0)
      {
        ret = avcodec_send_packet(audioCodecCtx, avPacket);
      }

      if (ret == AVERROR(EAGAIN))
      {
        ret = 0;
      }
      else if (ret < 0)
      {
        return -1;
      }
      else
      {
        len1 = avPacket->size;
      }

      if (len1 < 0)
      {
        // if error, skip frame
        audioPktSize = 0;
        break;
      }

      audioPktData += len1;
      audioPktSize -= len1;
      dataSize = 0;

      if (got_frame)
      {
        // audio resampling
        dataSize = player::audioResampling(
          vs
          , avFrame
          , AVSampleFormat::AV_SAMPLE_FMT_S16
          , audio_buf);

        assert(dataSize <= bufSize);
      }

      if (dataSize <= 0)
      {
        // no data yet, get more frames
        continue;
      }

      // keep audio_clock up to date
      auto audioClock = vs->audioClock();
      pts_ptr = audioClock;
      n = 2 * audioCodecCtx->ch_layout.nb_channels;
      audioClock += (double)dataSize / (double)(n * audioCodecCtx->sample_rate);
      vs->setAudioClock(audioClock);

      if (avPacket->data)
      {
        // wipe the packet
        av_packet_unref(avPacket);
      }
      av_frame_free(&avFrame);

      // we have the data, return it and come back for more later
      return dataSize;
    }

    if (avPacket->data)
    {
      av_packet_unref(avPacket);
    }

    // get more audio AVPacket
    int ret = vs->popAudioPacketRead(avPacket);

    // if packet_queue_get returns < 0, the global quit flag was set
    if (ret < 0)
    {
      return -1;
    }

    auto& flushPacket = vs->flushPacket();
    if (flushPacket)
    {
      if (avPacket->data == flushPacket->data)
      {
        avcodec_flush_buffers(audioCodecCtx);
        continue;
      }
    }

    audioPktData = avPacket->data;
    audioPktSize = avPacket->size;

    // keep audio_clock up to date
    if (avPacket->pts != AV_NOPTS_VALUE)
    {
      auto& audioStream = vs->audioStream();
      auto audioClock = av_q2d(audioStream->time_base) * avPacket->pts;
      vs->setAudioClock(audioClock);
    }
  }

  av_frame_unref(avFrame);
  avFrame = nullptr;
  return 0;
}

int player::audioResampling(
  VideoState* vs
  , AVFrame* decoded_audio_frame
  , AVSampleFormat out_sample_fmt
  , uint8_t* out_buf)
{
  // get an instance of the audioresamplingstate struct
  AudioReSamplingState arState;
  auto& audioCodecCtx = vs->audioCodecCtx();
  arState.init(audioCodecCtx->ch_layout.nb_channels);
  if (!arState.swr_ctx)
  {
    printf("swr_alloc error.\n");
    return -1;
  }

  // get input audio channels
  arState.in_channel_layout = audioCodecCtx->ch_layout.nb_channels;

  // check input audio channels correctly retrieved
  if (arState.in_channel_layout <= 0)
  {
    player::audioReleasePointer(arState);
    return -1;
  }

  // set output audio channels based on the input audio channels
  if (audioCodecCtx->ch_layout.nb_channels == 1)
  {
    arState.out_channel_layout = AV_CH_LAYOUT_MONO;
  }
  else if (audioCodecCtx->ch_layout.nb_channels == 2)
  {
    arState.out_channel_layout = AV_CH_LAYOUT_STEREO;
  }
  else
  {
    arState.out_channel_layout = AV_CH_LAYOUT_SURROUND;
  }

  // retrieve number of audio samples (per channel)
  arState.in_nb_samples = decoded_audio_frame->nb_samples;
  if (arState.in_nb_samples <= 0)
  {
    printf("in_nb_samples error.\n");
    player::audioReleasePointer(arState);
    return -1;
  }

  // Set SwrContext parameters for resampling
  av_opt_set_int(arState.swr_ctx, "in_channel_layout", arState.in_channel_layout, 0);
  av_opt_set_int(arState.swr_ctx, "in_sample_rate", audioCodecCtx->sample_rate, 0);
  av_opt_set_sample_fmt(arState.swr_ctx, "in_sample_fmt", audioCodecCtx->sample_fmt, 0);
  av_opt_set_int(arState.swr_ctx, "out_channel_layout", arState.out_channel_layout, 0);
  av_opt_set_int(arState.swr_ctx, "out_sample_rate", audioCodecCtx->sample_rate, 0);
  av_opt_set_sample_fmt(arState.swr_ctx, "out_sample_fmt", out_sample_fmt, 0);

  // Once all values have been set for the SwrContext, it must be initialized
  // with swr_init().
  int ret = swr_init(arState.swr_ctx);
  if (ret < 0)
  {
    printf("Failed to initialize the resampling context.\n");
    player::audioReleasePointer(arState);
    return -1;
  }

  arState.max_out_nb_samples = arState.out_nb_samples = av_rescale_rnd(
    arState.in_nb_samples,
    audioCodecCtx->sample_rate,
    audioCodecCtx->sample_rate,
    AV_ROUND_UP
    );

  // check rescaling was successful
  if (arState.max_out_nb_samples <= 0)
  {
    printf("av_rescale_rnd error.\n");
    player::audioReleasePointer(arState);
    return -1;
  }

  // get number of output audio channels
  arState.out_nb_channels = audioCodecCtx->ch_layout.nb_channels;

  ret = av_samples_alloc_array_and_samples(
    &arState.resampled_data,
    &arState.out_linesize,
    arState.out_nb_channels,
    arState.out_nb_samples,
    out_sample_fmt,
    0
    );

  if (ret < 0)
  {
    printf("av_samples_alloc_array_and_samples() error: Could not allocate destination samples.\n");
    player::audioReleasePointer(arState);
    return -1;
  }

  // retrieve output samples number taking into account the progressive delay
  auto swrDelay = swr_get_delay(arState.swr_ctx, audioCodecCtx->sample_rate);
  arState.out_nb_samples = av_rescale_rnd(
    swrDelay + arState.in_nb_samples
    , audioCodecCtx->sample_rate
    , audioCodecCtx->sample_rate
    , AV_ROUND_UP
    );

  // check output samples number was correctly retrieved
  if (arState.out_nb_samples <= 0)
  {
    player::audioReleasePointer(arState);
    return -1;
  }

  if (arState.out_nb_samples > arState.max_out_nb_samples)
  {
    // free memory block and set pointer to NULL
    av_free(arState.resampled_data[0]);

    // Allocate a samples buffer for out_nb_samples samples
    ret = av_samples_alloc(
      arState.resampled_data
      , &arState.out_linesize
      , arState.out_nb_channels
      , arState.out_nb_samples
      , out_sample_fmt
      , 1
      );

    // check samples buffer correctly allocated
    if (ret < 0)
    {
      printf("av_samples_alloc failed.\n");
      player::audioReleasePointer(arState);
      return -1;
    }
    arState.max_out_nb_samples = arState.out_nb_samples;
  }

  if (arState.swr_ctx)
  {
    // do the actual audio data resampling
    ret = swr_convert(
      arState.swr_ctx
      , arState.resampled_data
      , arState.out_nb_samples
      , (const uint8_t **) decoded_audio_frame->data
      , decoded_audio_frame->nb_samples);

    // check audio conversion was successful
    if (ret < 0)
    {
      printf("swr_convert_error.\n");
      player::audioReleasePointer(arState);
      return -1;
    }

    // Get the required buffer size for the given audio parameters
    arState.resampled_data_size = av_samples_get_buffer_size(
      &arState.out_linesize
      , arState.out_nb_channels
      , ret
      , out_sample_fmt
      , 1);

    // check audio buffer size
    if (arState.resampled_data_size < 0)
    {
      printf("av_samples_get_buffer_size error.\n");
      player::audioReleasePointer(arState);
      return -1;
    }
  }
  else
  {
    printf("swr_ctx null error.\n");
    player::audioReleasePointer(arState);
    return -1;
  }

  // copy the resampled data to the output buffer
  std::memcpy(out_buf, arState.resampled_data[0], arState.resampled_data_size);

  /*
   * Memory Cleanup.
   */
  player::audioReleasePointer(arState);

  return arState.resampled_data_size;
}

int player::syncAudio(VideoState* vs, short* samples, int& samplesSize)
{
  auto& audioCodecCtx = vs->audioCodecCtx();
  int n = 2 * audioCodecCtx->ch_layout.nb_channels;

  // check
  auto avSyncType = vs->syncType();
  if (avSyncType != SYNC_TYPE::AV_SYNC_AUDIO_MASTER)
  {
    auto refClock = vs->masterClock();
    auto audioClock = vs->calcAudioClock();
    auto diff = audioClock - refClock;

    if (diff < AV_NOSYNC_THRESHOLD)
    {
      // accumulate the diffs
      auto audioDiffAvgCoef = vs->audioDiffAvgCoef();
      auto audioDiffCum = vs->audioDiffCum();
      vs->setAudioDiffCum(diff + audioDiffAvgCoef * audioDiffCum);
      auto audioDiffAvgCount = vs->audioDiffAvgCount();
      if (audioDiffAvgCount < AUDIO_DIFF_AVG_NB)
      {
        audioDiffAvgCount++;
        vs->setAudioDiffAvgCount(audioDiffAvgCount);
      }
      else
      {
        auto audioDiffCum = vs->audioDiffCum();
        auto audioDiffAvgCoef = vs->audioDiffAvgCoef();
        auto avgDiff = audioDiffCum * (1.0 - audioDiffAvgCoef);
        auto audioDiffThreshold = vs->audioDiffThreshold();
        if (fabs(avgDiff) >= audioDiffThreshold)
        {
          auto sampleRate = audioCodecCtx->sample_rate;
          auto wantedSize = samplesSize + ((int)(diff * sampleRate) * n);
          auto minSize = samplesSize * ((100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100);
          auto maxSize = samplesSize * ((100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100);

          if (wantedSize < minSize)
          {
            wantedSize = minSize;
          }
          else if (wantedSize > maxSize)
          {
            wantedSize = maxSize;
          }

          if (wantedSize < samplesSize)
          {
            // remove samples
            samplesSize = wantedSize;
          }
          else if (wantedSize > samplesSize)
          {
            uint8_t* samplesEnd = nullptr;
            uint8_t* dst = nullptr;

            // add samples by copying final sample
            auto nb = (samplesSize - wantedSize);
            samplesEnd = (uint8_t *)samples + samplesSize - n;
            dst = samplesEnd + n;

            while (nb > 0)
            {
              std::memcpy(dst, samplesEnd, n);
              dst += n;
              nb -= n;
            }

            samplesSize = wantedSize;
          }
        }
      }
    }
    else
    {
      // difference is TOO big, reset diff stuff
      vs->setAudioDiffAvgCount(0);
      vs->setAudioDiffCum(0);
    }
  }

  return samplesSize;
}

void player::audioReleasePointer(AudioReSamplingState& arState)
{
  /*
   * Memory Cleanup.
   */
  if (arState.resampled_data)
  {
    // free memory block and set pointer to NULL
    av_freep(&arState.resampled_data[0]);
  }

  av_freep(&arState.resampled_data);

  arState.resampled_data = NULL;
  if (arState.swr_ctx)
  {
    // Free the given SwrContext and set the pointer to NULL
    swr_free(&arState.swr_ctx);
  }
}
