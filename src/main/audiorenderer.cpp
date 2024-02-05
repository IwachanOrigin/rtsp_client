
#include <cstring>
#include <thread>
#include <cassert>
#include "audiorenderer.h"
#include "framecontainer.h"

using namespace client;

#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE  192000
#define AV_NOSYNC_THRESHOLD   1.0
#define AUDIO_DIFF_AVG_NB     20
#define SAMPLE_CORRECTION_PERCENT_MAX 10

AudioRenderer::AudioRenderer()
{}

AudioRenderer::~AudioRenderer()
{}

void AudioRenderer::audioCallback(void* userdata, Uint8* stream, int len)
{
  // Retrieve the FrameContainter
  auto frameContainer = reinterpret_cast<FrameContainer*>(userdata);

  double pts = 0;

  if (frameContainer->sizeAudioFrameDecoded() <= 0 && frameContainer->sizeVideoFrameDecoded() <= 0)
  {
    //auto sdlAudioDeviceID = videoState->sdlAudioDeviceID();
    //SDL_PauseAudioDevice(sdlAudioDeviceID, 1);
    return;
  }

  // allocate a new frame, used to decode audio packets
  AVFrame* frame = av_frame_alloc();
  if (!frame)
  {
    return;
  }

  auto ret = frameContainer->popAudioFrameDecoded(frame);
  if (ret < 0)
  {
    std::cerr << "" << std::endl;
    av_frame_unref(frame);
    av_frame_free(&frame);
  }

  // audio resampling
  auto dataSize = audioResampling(
    frame
    , AVSampleFormat::AV_SAMPLE_FMT_S16
    , audio_buf);

  // copy data from audio buffer to the SDL stream
  std::memcpy(stream, (uint8_t *)audioBuf + audioBufIndex, len1);

}

int AudioRenderer::audioResampling(
  AVFrame* decodedAudioFrame
  , AVSampleFormat outSampleFmt
  , uint8_t* outBuf)
{
  // get an instance of the audioresamplingstate struct
  AudioReSamplingState arState;

  arState.init(2 /* channel_layout */);
  if (!arState.swr_ctx)
  {
    printf("swr_alloc error.\n");
    return -1;
  }

  // Get input audio channels
  arState.in_channel_layout = 2; // channel_layout

  // Check input audio channels correctly retrieved
  if (arState.in_channel_layout <= 0)
  {
    audioReleasePointer(arState);
    return -1;
  }

  // Set output audio channels based on the input audio channels
  if (arState.in_channel_layout == 1)
  {
    arState.out_channel_layout = AV_CH_LAYOUT_MONO;
  }
  else if (arState.in_channel_layout == 2)
  {
    arState.out_channel_layout = AV_CH_LAYOUT_STEREO;
  }
  else
  {
    arState.out_channel_layout = AV_CH_LAYOUT_SURROUND;
  }

  // retrieve number of audio samples (per channel)
  arState.in_nb_samples = decodedAudioFrame->nb_samples;
  if (arState.in_nb_samples <= 0)
  {
    printf("in_nb_samples error.\n");
    audioReleasePointer(arState);
    return -1;
  }

  // Set SwrContext parameters for resampling
  av_opt_set_int(arState.swr_ctx, "in_channel_layout", arState.in_channel_layout, 0);
  av_opt_set_int(arState.swr_ctx, "in_sample_rate", audioCodecCtx->sample_rate, 0);
  av_opt_set_sample_fmt(arState.swr_ctx, "in_sample_fmt", audioCodecCtx->sample_fmt, 0);
  av_opt_set_int(arState.swr_ctx, "out_channel_layout", arState.out_channel_layout, 0);
  av_opt_set_int(arState.swr_ctx, "out_sample_rate", audioCodecCtx->sample_rate, 0);
  av_opt_set_sample_fmt(arState.swr_ctx, "out_sample_fmt", outSampleFmt, 0);

  // Once all values have been set for the SwrContext, it must be initialized
  // with swr_init().
  int ret = swr_init(arState.swr_ctx);
  if (ret < 0)
  {
    printf("Failed to initialize the resampling context.\n");
    audioReleasePointer(arState);
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
    audioReleasePointer(arState);
    return -1;
  }

  // Get number of output audio channels
  arState.out_nb_channels = arState.in_channel_layout;

  ret = av_samples_alloc_array_and_samples(
    &arState.resampled_data,
    &arState.out_linesize,
    arState.out_nb_channels,
    arState.out_nb_samples,
    outSampleFmt,
    0
    );

  if (ret < 0)
  {
    printf("av_samples_alloc_array_and_samples() error: Could not allocate destination samples.\n");
    audioReleasePointer(arState);
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
    audioReleasePointer(arState);
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
      , outSampleFmt
      , 1
      );

    // check samples buffer correctly allocated
    if (ret < 0)
    {
      std::cerr << "av_samples_alloc failed." << std::endl;
      audioReleasePointer(arState);
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
      , (const uint8_t **) decodedAudioFrame->data
      , decodedAudioFrame->nb_samples);

    // check audio conversion was successful
    if (ret < 0)
    {
      std::cerr << "swr_convert_error." << std::endl;
      audioReleasePointer(arState);
      return -1;
    }

    // Get the required buffer size for the given audio parameters
    arState.resampled_data_size = av_samples_get_buffer_size(
      &arState.out_linesize
      , arState.out_nb_channels
      , ret
      , outSampleFmt
      , 1);

    // check audio buffer size
    if (arState.resampled_data_size < 0)
    {
      printf("av_samples_get_buffer_size error.\n");
      audioReleasePointer(arState);
      return -1;
    }
  }
  else
  {
    printf("swr_ctx null error.\n");
    audioReleasePointer(arState);
    return -1;
  }

  // copy the resampled data to the output buffer
  std::memcpy(outBuf, arState.resampled_data[0], arState.resampled_data_size);

  /*
   * Memory Cleanup.
   */
  audioReleasePointer(arState);

  return arState.resampled_data_size;
}

void AudioRenderer::audioReleasePointer(AudioReSamplingState& arState)
{
  // Memory Cleanup.
  if (arState.resampled_data)
  {
    // Free memory block and set pointer to NULL
    av_freep(&arState.resampled_data[0]);
  }

  av_freep(&arState.resampled_data);

  arState.resampled_data = nullptr;
  if (arState.swr_ctx)
  {
    // Free the given SwrContext and set the pointer to nullptr
    swr_free(&arState.swr_ctx);
  }
}
