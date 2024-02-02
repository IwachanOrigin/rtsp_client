
#ifndef FRAME_CONTAINER_H_
#define FRAME_CONTAINER_H_

#include "framequeue.h"

namespace client
{

class FrameContainer
{
public:
  explicit FrameContainer();
  ~FrameContainer();

  int pushVideoFrameDecoded(AVFrame* frame);
  int pushAudioFrameDecoded(AVFrame* frame);
  int popVideoFrameDecoded(AVFrame* frame);
  int popAudioFrameDecoded(AVFrame* frame);
  int sizeVideoFrameDecoded() const { return m_videoFrameQueue.size(); }
  int sizeAudioFrameDecoded() const { return m_audioFrameQueue.size(); }
  void clearAudioFrameDecoded() { m_audioFrameQueue.clear(); }
  void clearVideoFrameDecoded() { m_videoFrameQueue.clear(); }

private:
  FrameQueue m_audioFrameQueue;
  FrameQueue m_videoFrameQueue;
};

} // client

#endif // FRAME_CONTAINER_H_
