
#ifndef VIDEO_PICTURE_H_
#define VIDEO_PICTURE_H_

extern "C"
{
#include <libavformat/avformat.h>
}

class VideoPicture
{
public:
  explicit VideoPicture();
  ~VideoPicture();

  AVFrame *frame;
  int width;
  int height;
  int allocated;
  double pts;
};

#endif // VIDEO_PICTURE_H_
