
#ifndef FRAME_QUEUE_H_
#define FRAME_QUEUE_H_

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
}

#include <queue>
#include <mutex>

namespace client
{

struct Frame
{
  AVFrame* frame;
  int width;
  int height;
  double pts;
};

enum class FrameQueueStatus
{
  EMPTY = -1
  , OK    = 0
  , FULL  = 1
};

class FrameQueue
{
public:
  explicit FrameQueue();
  ~FrameQueue();

  int init(const unsigned int& maxSize);
  int pop(AVFrame* frame);
  int push(AVFrame* frame);
  void clear();
  int size() const { return m_qFrames.size(); }

private:
  std::queue<Frame*> m_qFrames;
  std::mutex m_mutex;
  unsigned int m_maxSize;
};

} // common

#endif // FRAME_QUEUE_H_
