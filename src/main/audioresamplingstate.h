
#ifndef AUDIO_RESAMPLING_STATE_H_
#define AUDIO_RESAMPLING_STATE_H_

extern "C"
{
#include <libswresample/swresample.h>
}

class AudioReSamplingState
{
public:
  explicit AudioReSamplingState();
  ~AudioReSamplingState();

  void init(uint64_t channel_layout);

  SwrContext *swr_ctx;
  int64_t in_channel_layout;
  uint64_t out_channel_layout;
  int out_nb_channels;
  int out_linesize;
  int in_nb_samples;
  int64_t out_nb_samples;
  int64_t max_out_nb_samples;
  uint8_t **resampled_data;
  int resampled_data_size;
};

#endif // AUDIO_RESAMPLING_STATE_H_
