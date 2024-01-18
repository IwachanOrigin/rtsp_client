
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

namespace player
{
  void audioCallback(void* userdata, Uint8* stream, int len);
  int audioDecodeFrame(VideoState* vs, uint8_t* audio_buf, int buf_size, double& ptsPtr);
  int audioResampling(VideoState* vs, AVFrame* decodedAudioFrame, AVSampleFormat outSampleFmt, uint8_t* outBuf);
  int syncAudio(VideoState* vs, short* samples, int& samplesSize);
  void audioReleasePointer(AudioReSamplingState& arState);

}

#endif // AUDIO_DECODER_H_
