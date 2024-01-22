
#ifndef MY_RTSP_CLIENT_H_
#define MY_RTSP_CLIENT_H_

#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>
#include "streamclientstate.h"

namespace player
{

class MyRtspClient : public RTSPClient
{
public:
  static MyRtspClient* createNew(
    UsageEnvironment& env
    , char const* rtspURL
    , int verbosityLevel = 0
    , char const* applicationName = nullptr
    , portNumBits tunnelOverHTTPPortNum = 0);

  StreamClientState scs;

protected:
  explicit MyRtspClient(
    UsageEnvironment& env
    , char const* rtspURL
    , int verbosityLevel
    , char const* applicationName
    , portNumBits tunnelOverHTTPPortNum);
  virtual ~MyRtspClient();
};

} // player

#endif // RTSP_CLIENT_H_
