
#ifndef VIDEO_DECODER_H_
#define VIDEO_DECODER_H_

extern "C"
{
#include <SDL.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#include "videostate.h"

namespace player
{

class VideoDecoder
{
public:
  explicit VideoDecoder() = default;
  ~VideoDecoder();

  int start(std::shared_ptr<VideoState> vs);
  void stop();

private:
  std::shared_ptr<VideoState> m_vs = nullptr;
  std::mutex m_mutex;
  bool m_finishedDecoder = false;

  int decodeThread(std::shared_ptr<VideoState> vs);
  int64_t guessCorrectPts(AVCodecContext* ctx, const int64_t& reordered_pts, const int64_t& dts);
  double syncVideo(std::shared_ptr<VideoState> vs, AVFrame* srcFrame, double pts);
};

} // player

#endif // VIDEO_DECODER_H_
