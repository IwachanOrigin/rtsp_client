
#ifndef AUDIO_RENDERER_H_
#define AUDIO_RENDERER_H_

#include "audioresamplingstate.h"
#include "framecontainer.h"

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
  explicit AudioRenderer() = default;
  ~AudioRenderer();

  int init(const int& outAudioDeviceIndex, std::shared_ptr<FrameContainer> frameContainer);
  int start();
  int stop();

private:
  std::shared_ptr<FrameContainer> m_frameContainer = nullptr;
  int m_sdlAudioDeviceIndex = -1;
  SDL_AudioSpec m_wants{0};
  

  // Callback from SDL2
  static void audioCallback(void* userdata, Uint8* stream, int len);

  // Callback from audioCallback func.
  void internalAudioCallback(Uint8* stream, int len);
  int audioResampling(AVFrame* decodedAudioFrame, AVSampleFormat outSampleFmt, std::unique_ptr<uint8_t>& outBuf);
  void audioReleasePointer(AudioReSamplingState& arState);
};

}

#endif // AUDIO_RENDERER_H_
