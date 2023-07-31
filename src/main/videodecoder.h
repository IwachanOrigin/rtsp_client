
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

class VideoDecoder
{
public:
  explicit VideoDecoder();
  ~VideoDecoder();

  int start(VideoState *videoState);

private:
  VideoState *m_videoState;

  int video_thread(void *arg);
  int64_t guess_correct_pts(AVCodecContext *ctx, int64_t reordered_pts, int64_t dts);
  double sync_video(VideoState *videoState, AVFrame *src_frame, double pts);
};


#endif // VIDEO_DECODER_H_
