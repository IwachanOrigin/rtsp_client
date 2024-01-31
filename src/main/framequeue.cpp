
#include "framequeue.h"

using namespace client;

FrameQueue::FrameQueue()
  : m_maxSize(300)
{
}

FrameQueue::~FrameQueue()
{
  this->clear();
}

int FrameQueue::init(const unsigned int& maxSize)
{
  m_maxSize = maxSize;
  return (int)FrameQueueStatus::OK;
}

int FrameQueue::pop(AVFrame* frame)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_qFrames.empty())
  {
    return (int)FrameQueueStatus::EMPTY;
  }

  auto queueFrame = m_qFrames.front();
  if (queueFrame)
  {
    // move
    av_frame_move_ref(frame, queueFrame->frame);

    // Release frame
    av_frame_unref(queueFrame->frame);
    av_frame_free(&queueFrame->frame);

    // Release pointer
    delete queueFrame;

    m_qFrames.pop();
  }
  return (int)FrameQueueStatus::OK;
}

int FrameQueue::push(AVFrame* frame)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  Frame* queueFrame = new Frame();
  queueFrame->frame = av_frame_alloc();
  av_frame_move_ref(queueFrame->frame, frame);
  m_qFrames.push(queueFrame);

  return (int)FrameQueueStatus::OK;
}

void FrameQueue::clear()
{
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!m_qFrames.empty())
  {
    while (!m_qFrames.empty())
    {
      Frame* frame = m_qFrames.front();
      // Release frame
      av_frame_unref(frame->frame);
      av_frame_free(&frame->frame);
      // Release pointer
      delete frame;
      //Pop
      m_qFrames.pop();
    }
  }
}

