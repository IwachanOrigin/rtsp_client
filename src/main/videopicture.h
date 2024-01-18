
#ifndef VIDEO_PICTURE_H_
#define VIDEO_PICTURE_H_

extern "C"
{
#include <libavformat/avformat.h>
}

namespace player
{

class VideoPicture
{
public:
  explicit VideoPicture() = default;
  ~VideoPicture() = default;

  AVFrame *frame = nullptr;
  int width = 0;
  int height = 0;
  int allocated = 0;
  double pts = 0.0;
};

} // player

#endif // VIDEO_PICTURE_H_
