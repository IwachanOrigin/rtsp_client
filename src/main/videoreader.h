
#ifndef VIDEO_READER_H_
#define VIDEO_READER_H_

#include <string>
#include <memory>
#include "videostate.h"
#include "videodecoder.h"
#include "audiodecoder.h"
#include "audioresamplingstate.h"
#include "videorenderer.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/avstring.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <SDL.h>
#include <SDL_thread.h>
}

static int deviceID = 0;

class VideoReader
{
public:
  explicit VideoReader();
  ~VideoReader();

  int start(const std::string filename, const int audioDeviceIndex);
  int quitStatus() { return m_videoState->quit; }

private:
  VideoDecoder* m_videoDecoder;
  VideoRenderer* m_videoRenderer;
  VideoState* m_videoState;

  int stream_component_open(VideoState *videoState, int stream_index);
  int read_thread(void *arg);
};

#endif // VIDEO_READER_H_
