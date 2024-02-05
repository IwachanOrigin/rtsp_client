
#ifndef AUDIO_RENDERER_H_
#define AUDIO_RENDERER_H_

#include <iostream>
#include <string>
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

namespace client
{

class AudioRenderer
{
public:
  explicit AudioRenderer();
  ~AudioRenderer();

  static void audioCallback(void* userdata, Uint8* stream, int len);
  static int audioDecodeFrame(uint8_t* audio_buf, int buf_size, double& ptsPtr);
  static int audioResampling(AVFrame* decodedAudioFrame, AVSampleFormat outSampleFmt, uint8_t* outBuf);
  static int syncAudio(short* samples, int& samplesSize);
  static void audioReleasePointer(AudioReSamplingState& arState);
};

}

#endif // AUDIO_RENDERER_H_
