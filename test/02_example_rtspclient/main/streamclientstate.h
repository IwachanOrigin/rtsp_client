
#ifndef STREAM_CLIENT_STATE_H_
#define STREAM_CLIENT_STATE_H_

#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>

namespace client
{

class StreamClientState
{
public:
  explicit StreamClientState();
  virtual ~StreamClientState();

  MediaSubsessionIterator* m_iter;
  MediaSession* m_session;
  MediaSubsession* m_subsession;
  TaskToken m_streamTimerTask;
  double m_duration;
};

} // client

#endif // STREAM_CLIENT_STATE_H_
