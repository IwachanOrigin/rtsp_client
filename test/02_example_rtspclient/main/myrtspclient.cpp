
#include "myrtspclient.h"

using namespace client;

MyRtspClient* MyRtspClient::createNew(
    UsageEnvironment& env
    , char const* rtspURL
    , int verbosityLevel
    , char const* applicationName
    , portNumBits tunnelOverHTTPPortNum)
{
  return new MyRtspClient(env, rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum);
}

MyRtspClient::MyRtspClient(
    UsageEnvironment& env
    , char const* rtspURL
    , int verbosityLevel
    , char const* applicationName
    , portNumBits tunnelOverHTTPPortNum)
  : RTSPClient(env,rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum, -1)
{
}

MyRtspClient::~MyRtspClient()
{
}

