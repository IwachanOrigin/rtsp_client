
#ifndef AUDIO_DECODER_H_
#define AUDIO_DECODER_H_

#include <iostream>
#include <string>
#include "packetqueue.h"
#include "videostate.h"
#include "audioresamplingstate.h"

extern "C"
{
#include <SDL.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#include <mutex>

#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE  192000
#define AV_NOSYNC_THRESHOLD   1.0
#define AUDIO_DIFF_AVG_NB     20
#define SAMPLE_CORRECTION_PERCENT_MAX 10

void audio_callback(void *userdata, Uint8 *stream, int len);
int audio_decode_frame(VideoState *videoState, uint8_t *audio_buf, int buf_size, double *pts_ptr);
static int audio_resampling(VideoState *videoState, AVFrame *decoded_audio_frame, enum AVSampleFormat out_sample_fmt, uint8_t *out_buf);
int synchronize_audio(VideoState *videoState, short *samples, int samples_size);

#endif // AUDIO_DECODER_H_
