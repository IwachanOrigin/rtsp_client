
#ifndef STREAM_CLIENT_STATE_H_
#define STREAM_CLIENT_STATE_H_

#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>

namespace client
{

class StreamClientState
{
public:
  explicit StreamClientState() = default;
  virtual ~StreamClientState();

  MediaSubsessionIterator*& iterator() { return m_iterator; }
  MediaSession*& session() { return m_session; }
  MediaSubsession*& audioSubsession() { return m_audioSubsession; }
  MediaSubsession*& videoSubsession() { return m_videoSubsession; }
  TaskToken& streamTimerTask() { return m_streamTimerTask; }
  double& duration() { return m_duration; }
  void setDuration(const double& duration) { m_duration = duration; }

private:
  MediaSubsessionIterator* m_iterator = nullptr;
  MediaSession* m_session = nullptr;
  MediaSubsession* m_audioSubsession = nullptr;
  MediaSubsession* m_videoSubsession = nullptr;
  TaskToken m_streamTimerTask = nullptr;
  double m_duration = 0.0;
};

} // client

#endif // STREAM_CLIENT_STATE_H_
