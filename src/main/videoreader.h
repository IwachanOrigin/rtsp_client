
#ifndef VIDEO_READER_H_
#define VIDEO_READER_H_

#include <string>
#include <memory>
#include <atomic>

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
}

#include "videorenderer.h"
#include "videodecoder.h"
#include "options.h"

namespace player
{

class VideoState;

class VideoReader
{
public:
  explicit VideoReader() = default;
  ~VideoReader() = default;

  int start(const std::string& filename, const Options& opt);
  void stop();
  bool isFinished() const { return m_isFinished; }

private:
  std::shared_ptr<VideoState> m_videoState = nullptr;
  std::unique_ptr<VideoDecoder> m_videoDecoder = nullptr;
  std::unique_ptr<VideoRenderer> m_videoRenderer = nullptr;
  std::string m_filename = "";
  std::atomic_bool m_isFinished = false;

  int streamComponentOpen(std::shared_ptr<VideoState> vs, const int& streamIndex);
  int readThread(std::shared_ptr<VideoState> vs, const Options& opt);
  static int decodeInterruptCB(void* videoState);
};

}

#endif // VIDEO_READER_H_
