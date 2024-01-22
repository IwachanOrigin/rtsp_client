
#ifndef DUMMY_SINK_H_
#define DUMMY_SINK_H_

#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>

namespace client
{

class DummySink : public MediaSink
{
public:
  static DummySink* createNew(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId = nullptr);

private:
  explicit DummySink(UsageEnvironment& env, MediaSubsession& subsession, char const* stramId);
  virtual ~DummySink();

  static void afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes
                                , struct timeval presentationTime, unsigned durationInMicroseconds);
  void afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes
                         , struct timeval presentationTime, unsigned durationInMicroseconds);

  virtual Boolean continuePlaying();

  u_int8_t* m_receiveBuffer;
  MediaSubsession& m_subsession;
  char* m_streamId;
};

} // player

#endif // DUMMY_SINK_H_

