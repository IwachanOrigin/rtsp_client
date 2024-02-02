
#include "framecontainer.h"

using namespace client;

FrameContainer::FrameContainer()
{
}

FrameContainer::~FrameContainer()
{
}

int FrameContainer::pushVideoFrameDecoded(AVFrame* frame)
{
  return m_videoFrameQueue.push(frame);
}

int FrameContainer::pushAudioFrameDecoded(AVFrame* frame)
{
  return m_audioFrameQueue.push(frame);
}

int FrameContainer::popVideoFrameDecoded(AVFrame* frame)
{
  int ret = m_videoFrameQueue.pop(frame);
  return (frame == nullptr) ? -1 : 0;
}

int FrameContainer::popAudioFrameDecoded(AVFrame* frame)
{
  int ret = m_audioFrameQueue.pop(frame);
  return (frame == nullptr) ? -1 : 0;
}

