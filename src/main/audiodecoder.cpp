
#include <thread>
#include <cassert>
#include "audiodecoder.h"

void audio_callback(void *userdata, Uint8 *stream, int len)
{
  // retrieve the videostate
  VideoState *videoState = (VideoState *) userdata;

  int len1 = -1;
  int audio_size = -1;

  double pts = 0;

  while (len > 0)
  {
    // check global quit flag
    if (videoState->quit)
    {
      return;
    }

    if (videoState->audio_buf_index >= videoState->audio_buf_size)
    {
      // we have already sent all avaialble data; get more
      audio_size = audio_decode_frame(videoState, videoState->audio_buf, sizeof(videoState->audio_buf), &pts);

      if (audio_size < 0)
      {
        // output silence
        videoState->audio_buf_size = 1024;

        // clear memory
        std::memset(videoState->audio_buf, 0, videoState->audio_buf_size);
      }
      else
      {
        audio_size = synchronize_audio(videoState, (int16_t *)videoState->audio_buf, audio_size);
        videoState->audio_buf_size = audio_size;
      }

      videoState->audio_buf_index = 0;
    }

    len1 = videoState->audio_buf_size - videoState->audio_buf_index;

    if (len1 > len)
    {
      len1 = len;
    }

    // copy data from audio buffer to the SDL stream
    std::memcpy(stream, (uint8_t *)videoState->audio_buf + videoState->audio_buf_index, len1);

    len -= len1;
    stream += len1;
    videoState->audio_buf_index += len1;
  }
}

int audio_decode_frame(VideoState *videoState, uint8_t *audio_buf, int buf_size, double *pts_ptr)
{
  AVPacket *avPacket = av_packet_alloc();
  static uint8_t *audio_pkt_data = nullptr;
  static int audio_pkt_size = 0;

  double pts = 0;
  int n = 0;

  // allocate a new frame, used to decode audio packets
  static AVFrame *avFrame = nullptr;
  avFrame = av_frame_alloc();
  if (!avFrame)
  {
    return -1;
  }

  int len1 = 0;
  int data_size = 0;

  for (;;)
  {
    // check global quit flag
    if (videoState->quit)
    {
      av_frame_unref(avFrame);
      avFrame = nullptr;
      return -1;
    }

    while (audio_pkt_size > 0)
    {
      int got_frame = 0;

      int ret = avcodec_receive_frame(videoState->audio_ctx, avFrame);
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
        ret = avcodec_send_packet(videoState->audio_ctx, avPacket);
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
        audio_pkt_size = 0;
        break;
      }

      audio_pkt_data += len1;
      audio_pkt_size -= len1;
      data_size = 0;

      if (got_frame)
      {
        // audio resampling
        data_size = audio_resampling(
          videoState
          , avFrame
          , AV_SAMPLE_FMT_S16
          , audio_buf);

        assert(data_size <= buf_size);
      }

      if (data_size <= 0)
      {
        // no data yet, get more frames
        continue;
      }

      // keep audio_clock up to date
      pts = videoState->audio_clock;
      *pts_ptr = pts;
      n = 2 * videoState->audio_ctx->ch_layout.nb_channels;
      videoState->audio_clock += (double)data_size / (double)(n * videoState->audio_ctx->sample_rate);

      if (avPacket->data)
      {
        // wipe the packet
        av_packet_unref(avPacket);
      }
      av_frame_free(&avFrame);

      // we have the data, return it and come back for more later
      return data_size;
    }

    if (avPacket->data)
    {
      av_packet_unref(avPacket);
    }

    // get more audio AVPacket
    int ret = videoState->audioq.get(avPacket, 1);

    // if packet_queue_get returns < 0, the global quit flag was set
    if (ret < 0)
    {
      return -1;
    }

    if (avPacket->data == videoState->flush_pkt->data)
    {
      avcodec_flush_buffers(videoState->audio_ctx);
      continue;
    }

    audio_pkt_data = avPacket->data;
    audio_pkt_size = avPacket->size;

    // keep audio_clock up to date
    if (avPacket->pts != AV_NOPTS_VALUE)
    {
      videoState->audio_clock = av_q2d(videoState->audio_st->time_base) * avPacket->pts;
    }
  }

  av_frame_unref(avFrame);
  avFrame = nullptr;
  return 0;
}

static int audio_resampling(VideoState *videoState
                            , AVFrame *decoded_audio_frame
                            , enum AVSampleFormat out_sample_fmt
                            , uint8_t *out_buf)
{
  // get an instance of the audioresamplingstate struct
  AudioReSamplingState *arState = new AudioReSamplingState();
  arState->init(videoState->audio_ctx->ch_layout.nb_channels);
  if (!arState->swr_ctx)
  {
    printf("swr_alloc error.\n");
    return -1;
  }

  // get input audio channels
  arState->in_channel_layout = videoState->audio_ctx->ch_layout.nb_channels;

  // check input audio channels correctly retrieved
  if (arState->in_channel_layout <= 0)
  {
    return -1;
  }

  // set output audio channels based on the input audio channels
  if (videoState->audio_ctx->ch_layout.nb_channels == 1)
  {
    arState->out_channel_layout = AV_CH_LAYOUT_MONO;
  }
  else if (videoState->audio_ctx->ch_layout.nb_channels == 2)
  {
    arState->out_channel_layout = AV_CH_LAYOUT_STEREO;
  }
  else
  {
    arState->out_channel_layout = AV_CH_LAYOUT_SURROUND;
  }

  // retrieve number of audio samples (per channel)
  arState->in_nb_samples = decoded_audio_frame->nb_samples;
  if (arState->in_nb_samples <= 0)
  {
    printf("in_nb_samples error.\n");
    return -1;
  }

  // Set SwrContext parameters for resampling
  av_opt_set_int(arState->swr_ctx, "in_channel_layout", arState->in_channel_layout, 0);
  av_opt_set_int(arState->swr_ctx, "in_sample_rate", videoState->audio_ctx->sample_rate, 0);
  av_opt_set_sample_fmt(arState->swr_ctx, "in_sample_fmt", videoState->audio_ctx->sample_fmt, 0);
  av_opt_set_int(arState->swr_ctx, "out_channel_layout", arState->out_channel_layout, 0);
  av_opt_set_int(arState->swr_ctx, "out_sample_rate", videoState->audio_ctx->sample_rate, 0);
  av_opt_set_sample_fmt(arState->swr_ctx, "out_sample_fmt", out_sample_fmt, 0);

  // Once all values have been set for the SwrContext, it must be initialized
  // with swr_init().
  int ret = swr_init(arState->swr_ctx);;
  if (ret < 0)
  {
    printf("Failed to initialize the resampling context.\n");
    return -1;
  }

  arState->max_out_nb_samples = arState->out_nb_samples = av_rescale_rnd(
    arState->in_nb_samples,
    videoState->audio_ctx->sample_rate,
    videoState->audio_ctx->sample_rate,
    AV_ROUND_UP
    );

  // check rescaling was successful
  if (arState->max_out_nb_samples <= 0)
  {
    printf("av_rescale_rnd error.\n");
    return -1;
  }

  // get number of output audio channels
  //arState->out_nb_channels = av_get_channel_layout_nb_channels(arState->out_channel_layout);
  arState->out_nb_channels = videoState->audio_ctx->ch_layout.nb_channels;

  ret = av_samples_alloc_array_and_samples(
    &arState->resampled_data,
    &arState->out_linesize,
    arState->out_nb_channels,
    arState->out_nb_samples,
    out_sample_fmt,
    0
    );

  if (ret < 0)
  {
    printf("av_samples_alloc_array_and_samples() error: Could not allocate destination samples.\n");
    return -1;
  }

  // retrieve output samples number taking into account the progressive delay
  arState->out_nb_samples = av_rescale_rnd(
    swr_get_delay(arState->swr_ctx, videoState->audio_ctx->sample_rate) + arState->in_nb_samples
    , videoState->audio_ctx->sample_rate
    , videoState->audio_ctx->sample_rate
    , AV_ROUND_UP
    );

  // check output samples number was correctly retrieved
  if (arState->out_nb_samples <= 0)
  {
    return -1;
  }

  if (arState->out_nb_samples > arState->max_out_nb_samples)
  {
    // free memory block and set pointer to NULL
    av_free(arState->resampled_data[0]);

    // Allocate a samples buffer for out_nb_samples samples
    ret = av_samples_alloc(
      arState->resampled_data
      , &arState->out_linesize
      , arState->out_nb_channels
      , arState->out_nb_samples
      , out_sample_fmt
      , 1
      );

    // check samples buffer correctly allocated
    if (ret < 0)
    {
      printf("av_samples_alloc failed.\n");
      return -1;
    }

    arState->max_out_nb_samples = arState->out_nb_samples;
  }

  if (arState->swr_ctx)
  {
    // do the actual audio data resampling
    ret = swr_convert(
      arState->swr_ctx
      , arState->resampled_data
      , arState->out_nb_samples
      , (const uint8_t **) decoded_audio_frame->data
      , decoded_audio_frame->nb_samples);

    // check audio conversion was successful
    if (ret < 0)
    {
      printf("swr_convert_error.\n");
      return -1;
    }

    // Get the required buffer size for the given audio parameters
    arState->resampled_data_size = av_samples_get_buffer_size(
      &arState->out_linesize
      , arState->out_nb_channels
      , ret
      , out_sample_fmt
      , 1);

    // check audio buffer size
    if (arState->resampled_data_size < 0)
    {
      printf("av_samples_get_buffer_size error.\n");
      return -1;
    }
  }
  else
  {
    printf("swr_ctx null error.\n");
    return -1;
  }

  // copy the resampled data to the output buffer
  std::memcpy(out_buf, arState->resampled_data[0], arState->resampled_data_size);

  /*
   * Memory Cleanup.
   */
  if (arState->resampled_data)
  {
    // free memory block and set pointer to NULL
    av_freep(&arState->resampled_data[0]);
  }

  av_freep(&arState->resampled_data);
  arState->resampled_data = NULL;

  if (arState->swr_ctx)
  {
    // Free the given SwrContext and set the pointer to NULL
    swr_free(&arState->swr_ctx);
  }

  return arState->resampled_data_size;
}

int synchronize_audio(VideoState *videoState, short *samples, int samples_size)
{
  int n = 0;
  double ref_clock = 0;

  n = 2 * videoState->audio_ctx->ch_layout.nb_channels;

  // check
  if (videoState->av_sync_type != SYNC_TYPE::AV_SYNC_AUDIO_MASTER)
  {
    double diff = 0, avg_diff = 0;
    int wanted_size = 0, min_size = 0, max_size = 0;
    ref_clock = videoState->get_master_clock();
    diff = videoState->get_audio_clock() - ref_clock;

    if (diff < AV_NOSYNC_THRESHOLD)
    {
      // accumulate the diffs
      videoState->audio_diff_cum = diff + videoState->audio_diff_avg_coef * videoState->audio_diff_cum;
      if (videoState->audio_diff_avg_count < AUDIO_DIFF_AVG_NB)
      {
        videoState->audio_diff_avg_count++;
      }
      else
      {
        avg_diff = videoState->audio_diff_cum * (1.0 - videoState->audio_diff_avg_coef);
        if (fabs(avg_diff) >= videoState->audio_diff_threshold)
        {
          wanted_size = samples_size + ((int)(diff * videoState->audio_ctx->sample_rate) * n);
          min_size = samples_size * ((100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100);
          max_size = samples_size * ((100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100);

          if (wanted_size < min_size)
          {
            wanted_size = min_size;
          }
          else if (wanted_size > max_size)
          {
            wanted_size = max_size;
          }

          if (wanted_size < samples_size)
          {
            // remove samples
            samples_size = wanted_size;
          }
          else if (wanted_size > samples_size)
          {
            uint8_t *samples_end = nullptr, *q = nullptr;
            int nb = 0;

            // add samples by copying final sample
            nb = (samples_size - wanted_size);
            samples_end = (uint8_t *)samples + samples_size - n;
            q = samples_end + n;

            while (nb > 0)
            {
              std::memcpy(q, samples_end, n);
              q += n;
              nb -= n;
            }

            samples_size = wanted_size;
          }
        }
      }
    }
    else
    {
      // difference is TOO big, reset diff stuff
      videoState->audio_diff_avg_count = 0;
      videoState->audio_diff_cum = 0;
    }
  }

  return samples_size;
}
