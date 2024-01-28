
#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>
#include <string>
#include "streamclientstate.h"
#include "basertspclient.h"
//#include "sdlvideosink.h"

namespace client
{

class RtspController
{
public:
  explicit RtspController();
  ~RtspController() = default;

  // The main streaming routine (for each "rtsp://" URL):
  void openURL(UsageEnvironment& env, char const* progName, char const* rtspURL);

private:
  // RTSP 'response handlers':
  static void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString);
  static void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString);
  static void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString);
  // Other event handler functions:
  static void subsessionAfterPlaying(void* clientData); // called when a stream's subsession (e.g., audio or video substream) ends
  static void subsessionByeHandler(void* clientData, char const* reason);
  // called when a RTCP "BYE" is received for a subsession
  static void streamTimerHandler(void* clientData);
  // called at the end of a stream's expected duration (if the stream has not already signaled its end using a RTCP "BYE")

  // Used to iterate through each stream's 'subsessions', setting up each one:
  static void setupNextSubsession(RTSPClient* rtspClient);

  // Used to shut down and close a stream (including its "RTSPClient" object):
  static void shutdownStream(RTSPClient* rtspClient, int exitCode = 1);

  // Counts how many streams (i.e., "RTSPClient"s) are currently in use.
  static unsigned m_rtspClientCount;
};

} // client
