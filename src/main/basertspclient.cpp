
#include "basertspclient.h"

using namespace client;

BaseRtspClient* BaseRtspClient::createNew(
    UsageEnvironment& env
    , char const* rtspURL
    , int verbosityLevel
    , char const* applicationName
    , portNumBits tunnelOverHTTPPortNum)
{
  return new BaseRtspClient(env, rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum);
}

BaseRtspClient::BaseRtspClient(
    UsageEnvironment& env
    , char const* rtspURL
    , int verbosityLevel
    , char const* applicationName
    , portNumBits tunnelOverHTTPPortNum)
  : RTSPClient(env,rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum, -1)
{
}

BaseRtspClient::~BaseRtspClient()
{
}

