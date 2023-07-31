
#include "audioresamplingstate.h"

AudioReSamplingState::AudioReSamplingState()
  : swr_ctx(nullptr)
  , in_channel_layout(0)
  , out_channel_layout(0)
  , out_nb_channels(0)
  , out_linesize(0)
  , in_nb_samples(0)
  , out_nb_samples(0)
  , max_out_nb_samples(0)
  , resampled_data(nullptr)
  , resampled_data_size(0)
{
}

AudioReSamplingState::~AudioReSamplingState()
{
}

void AudioReSamplingState::init(uint64_t channel_layout)
{
  swr_ctx = swr_alloc();
  in_channel_layout = channel_layout;
  out_channel_layout = AV_CH_LAYOUT_STEREO;
  out_nb_channels = 0;
  out_linesize = 0;
  in_nb_samples = 0;
  out_nb_samples = 0;
  max_out_nb_samples = 0;
  resampled_data = nullptr;
  resampled_data_size = 0;
}
