
#ifndef BASE_RTSP_CLIENT_H_
#define BASE_RTSP_CLIENT_H_

#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>
#include "streamclientstate.h"

namespace client
{

class BaseRtspClient : public RTSPClient
{
public:
  static BaseRtspClient* createNew(
    UsageEnvironment& env
    , char const* rtspURL
    , int verbosityLevel = 0
    , char const* applicationName = nullptr
    , portNumBits tunnelOverHTTPPortNum = 0);

  StreamClientState& streamClientState() { return scs; }

protected:
  explicit BaseRtspClient(
    UsageEnvironment& env
    , char const* rtspURL
    , int verbosityLevel
    , char const* applicationName
    , portNumBits tunnelOverHTTPPortNum);
  virtual ~BaseRtspClient();

private:
  StreamClientState scs;
};

} // player

#endif // RTSP_CLIENT_H_
