
#include <iostream>
#include <string>
#include <thread>
#include <cassert>
#include "audiorenderer.h"

using namespace client;

constexpr int SDL_AUDIO_BUFFER_SIZE = 1024;
constexpr int MAX_AUDIO_FRAME_SIZE = 192000;
constexpr double AV_NOSYNC_THRESHOLD = 1.0;
constexpr int AUDIO_DIFF_AVG_NB = 20;
constexpr int SAMPLE_CORRECTION_PERCENT_MAX = 10;

AudioRenderer::~AudioRenderer()
{
}

int AudioRenderer::init(const int& outAudioDeviceIndex, std::shared_ptr<FrameContainer> frameContainer)
{
  if (outAudioDeviceIndex < 0)
  {
    return -1;
  }
  if (!frameContainer)
  {
    return -2;
  }

  m_frameContainer = frameContainer;

  SDL_AudioSpec spec{0};
  m_wants.freq = 44100; // audioCodecCtx->sample_rate
  m_wants.format = AUDIO_S16SYS;
  m_wants.channels = 2; // audioCodecCtx->ch_layout.nb_channels
  m_wants.silence = 0;
  m_wants.samples = SDL_AUDIO_BUFFER_SIZE; // 1024
  m_wants.callback = audioCallback; // static func
  m_wants.userdata = this; // AudioRenderer

  m_sdlAudioDeviceIndex = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(outAudioDeviceIndex, 0), false, &m_wants, &spec, 0);
  assert(m_sdlAudioDeviceIndex > 0);
  if (m_sdlAudioDeviceIndex <= 0)
  {
    return -3;
  }

  return 0;
}

int AudioRenderer::start()
{
  if (m_sdlAudioDeviceIndex <= 0)
  {
    return -1;
  }
  SDL_PauseAudioDevice(m_sdlAudioDeviceIndex, 0);
  return 0;
}

int AudioRenderer::stop()
{
  if (m_sdlAudioDeviceIndex <= 0)
  {
    return -1;
  }
  SDL_PauseAudioDevice(m_sdlAudioDeviceIndex, 1);
  return 0;
}


void AudioRenderer::audioCallback(void* userdata, Uint8* stream, int len)
{
  // Retrieve the AudioRenderer
  auto renderer = reinterpret_cast<AudioRenderer*>(userdata);
  renderer->internalAudioCallback(stream, len);
}

void AudioRenderer::internalAudioCallback(Uint8* stream, int len)
{
  double pts = 0;

  if (m_frameContainer->sizeAudioFrameDecoded() <= 0 && m_frameContainer->sizeVideoFrameDecoded() <= 0)
  {
    SDL_PauseAudioDevice(m_sdlAudioDeviceIndex, 1);
    return;
  }

  // allocate a new frame, used to decode audio packets
  AVFrame* frame = av_frame_alloc();
  if (!frame)
  {
    return;
  }

  auto ret = m_frameContainer->popAudioFrameDecoded(frame);
  if (ret < 0)
  {
    std::cerr << "" << std::endl;
    av_frame_unref(frame);
    av_frame_free(&frame);
    return;
  }

  // Audio resampling
  std::unique_ptr<uint8_t> audioBuf = std::make_unique<uint8_t>((MAX_AUDIO_FRAME_SIZE * 3) / 2);
  auto dataSize = this->audioResampling(
    frame
    , AVSampleFormat::AV_SAMPLE_FMT_S16
    , audioBuf);

  // Copy data from audio buffer to the SDL stream
  std::memcpy(stream, (uint8_t *)audioBuf + audioBufIndex, len1);
}

int AudioRenderer::audioResampling(
  AVFrame* decodedAudioFrame
  , AVSampleFormat outSampleFmt
  , std::unique_ptr<uint8_t>& outBuf)
{
  // get an instance of the audioresamplingstate struct
  AudioReSamplingState arState;

  arState.init(decodedAudioFrame->ch_layout.nb_channels);
  if (!arState.swr_ctx)
  {
    std::cerr << "swr_alloc error." << std::endl;
    return -1;
  }

  // Check input audio channels correctly retrieved
  if (arState.in_channel_layout <= 0)
  {
    this->audioReleasePointer(arState);
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
    this->audioReleasePointer(arState);
    return -1;
  }

  // Set SwrContext parameters for resampling
  av_opt_set_int(arState.swr_ctx, "in_channel_layout", arState.in_channel_layout, 0);
  av_opt_set_int(arState.swr_ctx, "in_sample_rate", decodedAudioFrame->sample_rate, 0);
  av_opt_set_sample_fmt(arState.swr_ctx, "in_sample_fmt", (AVSampleFormat)decodedAudioFrame->format, 0);
  av_opt_set_int(arState.swr_ctx, "out_channel_layout", arState.out_channel_layout, 0);
  av_opt_set_int(arState.swr_ctx, "out_sample_rate", decodedAudioFrame->sample_rate, 0);
  av_opt_set_sample_fmt(arState.swr_ctx, "out_sample_fmt", outSampleFmt, 0);

  // Once all values have been set for the SwrContext, it must be initialized
  // with swr_init().
  int ret = swr_init(arState.swr_ctx);
  if (ret < 0)
  {
    std::cerr << "Failed to initialize the resampling context." << std::endl;
    this->audioReleasePointer(arState);
    return -1;
  }

  arState.max_out_nb_samples = arState.out_nb_samples = av_rescale_rnd(
    arState.in_nb_samples,
    decodedAudioFrame->sample_rate,
    decodedAudioFrame->sample_rate,
    AV_ROUND_UP
    );

  // check rescaling was successful
  if (arState.max_out_nb_samples <= 0)
  {
    std::cerr << "av_rescale_rnd error." << std::endl;
    this->audioReleasePointer(arState);
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
    std::cerr << "av_samples_alloc_array_and_samples() error: Could not allocate destination samples." << std::endl;
    this->audioReleasePointer(arState);
    return -1;
  }

  // retrieve output samples number taking into account the progressive delay
  auto swrDelay = swr_get_delay(arState.swr_ctx, decodedAudioFrame->sample_rate);
  arState.out_nb_samples = av_rescale_rnd(
    swrDelay + arState.in_nb_samples
    , decodedAudioFrame->sample_rate
    , decodedAudioFrame->sample_rate
    , AV_ROUND_UP
    );

  // check output samples number was correctly retrieved
  if (arState.out_nb_samples <= 0)
  {
    this->audioReleasePointer(arState);
    return -1;
  }

  if (arState.out_nb_samples > arState.max_out_nb_samples)
  {
    // Free memory block and set pointer to NULL
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

    // Check samples buffer correctly allocated
    if (ret < 0)
    {
      std::cerr << "av_samples_alloc failed." << std::endl;
      this->audioReleasePointer(arState);
      return -1;
    }
    arState.max_out_nb_samples = arState.out_nb_samples;
  }

  // 
  if (!arState.swr_ctx)
  {
    std::cerr << "swr_ctx null error." << std::endl;
    this->audioReleasePointer(arState);
    return -1;
  }

  // Do the actual audio data resampling
  ret = swr_convert(
    arState.swr_ctx
    , arState.resampled_data
    , arState.out_nb_samples
    , (const uint8_t **) decodedAudioFrame->data
    , decodedAudioFrame->nb_samples);

  // Check audio conversion was successful
  if (ret < 0)
  {
    std::cerr << "swr_convert error." << std::endl;
    this->audioReleasePointer(arState);
    return -1;
  }

  // Get the required buffer size for the given audio parameters
  arState.resampled_data_size = av_samples_get_buffer_size(
    &arState.out_linesize
    , arState.out_nb_channels
    , ret
    , outSampleFmt
    , 1);

  // Check audio buffer size
  if (arState.resampled_data_size < 0)
  {
    std::cerr << "av_samples_get_buffer_size error." << std::endl;
    this->audioReleasePointer(arState);
    return -1;
  }

  // Copy the resampled data to the output buffer
  std::memcpy(outBuf.get(), arState.resampled_data[0], arState.resampled_data_size);

  // Memory Cleanup.
  this->audioReleasePointer(arState);

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

