
#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>
#include "streamclientstate.h"
#include "myrtspclient.h"
#include "sdlvideosink.h"

using namespace client;

#define RTSP_CLIENT_VERBOSITY_LEVEL 1 // by default, print verbose output from each "RTSPClient"
static unsigned rtspClientCount = 0; // Counts how many streams (i.e., "RTSPClient"s) are currently in use.


// RTSP 'response handlers':
void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString);
void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString);
void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString);

// Other event handler functions:
void subsessionAfterPlaying(void* clientData); // called when a stream's subsession (e.g., audio or video substream) ends
void subsessionByeHandler(void* clientData, char const* reason);
// called when a RTCP "BYE" is received for a subsession
void streamTimerHandler(void* clientData);
// called at the end of a stream's expected duration (if the stream has not already signaled its end using a RTCP "BYE")

// The main streaming routine (for each "rtsp://" URL):
void openURL(UsageEnvironment& env, char const* progName, char const* rtspURL);

// Used to iterate through each stream's 'subsessions', setting up each one:
void setupNextSubsession(RTSPClient* rtspClient);

// Used to shut down and close a stream (including its "RTSPClient" object):
void shutdownStream(RTSPClient* rtspClient, int exitCode = 1);

// A function that outputs a string that identifies each stream (for debugging output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env, const RTSPClient& rtspClient) {
  return env << "[URL:\"" << rtspClient.url() << "\"]: ";
}

// A function that outputs a string that identifies each subsession (for debugging output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env, const MediaSubsession& subsession) {
  return env << subsession.mediumName() << "/" << subsession.codecName();
}

void usage(UsageEnvironment& env, char const* progName) {
  env << "Usage: " << progName << " <rtsp-url-1> ... <rtsp-url-N>\n";
  env << "\t(where each <rtsp-url-i> is a \"rtsp://\" URL)\n";
}

char eventLoopWatchVariable = 0;

#undef main

int main(int argc, char* argv[])
{
  // Begin by setting up our usage environment:
  TaskScheduler* scheduler = BasicTaskScheduler::createNew();
  UsageEnvironment* env = BasicUsageEnvironment::createNew(*scheduler);

  // We need at least one "rtsp://" URL argument:
  if (argc < 2)
  {
    usage(*env, argv[0]);
    return 1;
  }

  // There are argc-1 URLs: argv[1] through argv[argc-1].  Open and start streaming each one:
  for (int i = 1; i <= argc-1; ++i)
  {
    openURL(*env, argv[0], argv[i]);
  }

  // All subsequent activity takes place within the event loop:
  env->taskScheduler().doEventLoop(&eventLoopWatchVariable);
  // This function call does not return, unless, at some point in time, "eventLoopWatchVariable" gets set to something non-zero.
  return 0;
}


void openURL(UsageEnvironment& env, char const* progName, char const* rtspURL)
{
  // Begin by creating a "RTSPClient" object.  Note that there is a separate "RTSPClient" object for each stream that we wish
  // to receive (even if more than stream uses the same "rtsp://" URL).
  RTSPClient* rtspClient = MyRtspClient::createNew(env, rtspURL, RTSP_CLIENT_VERBOSITY_LEVEL, progName);
  if (rtspClient == NULL)
  {
    env << "Failed to create a RTSP client for URL \"" << rtspURL << "\": " << env.getResultMsg() << "\n";
    return;
  }

  ++rtspClientCount;

  // Next, send a RTSP "DESCRIBE" command, to get a SDP description for the stream.
  // Note that this command - like all RTSP commands - is sent asynchronously; we do not block, waiting for a response.
  // Instead, the following function call returns immediately, and we handle the RTSP response later, from within the event loop:
  rtspClient->sendDescribeCommand(continueAfterDESCRIBE); 
}


void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString)
{
  do
  {
    UsageEnvironment& env = rtspClient->envir(); // alias
    StreamClientState& scs = ((MyRtspClient*)rtspClient)->scs; // alias

    if (resultCode != 0)
    {
      env << *rtspClient << "Failed to get a SDP description: " << resultString << "\n";
      delete[] resultString;
      break;
    }

    char* const sdpDescription = resultString;
    env << *rtspClient << "Got a SDP description:\n" << sdpDescription << "\n";

    // Create a media session object from this SDP description:
    scs.m_session = MediaSession::createNew(env, sdpDescription);
    delete[] sdpDescription; // because we don't need it anymore
    if (scs.m_session == NULL)
    {
      env << *rtspClient << "Failed to create a MediaSession object from the SDP description: " << env.getResultMsg() << "\n";
      break;
    }
    else if (!scs.m_session->hasSubsessions())
    {
      env << *rtspClient << "This session has no media subsessions (i.e., no \"m=\" lines)\n";
      break;
    }

    // Then, create and set up our data source objects for the session.  We do this by iterating over the session's 'subsessions',
    // calling "MediaSubsession::initiate()", and then sending a RTSP "SETUP" command, on each one.
    // (Each 'subsession' will have its own data source.)
    scs.m_iter = new MediaSubsessionIterator(*scs.m_session);
    setupNextSubsession(rtspClient);
    return;
  } while (0);

  // An unrecoverable error occurred with this stream.
  shutdownStream(rtspClient);
}

// By default, we request that the server stream its data using RTP/UDP.
// If, instead, you want to request that the server stream via RTP-over-TCP, change the following to True:
#define REQUEST_STREAMING_OVER_TCP False

void setupNextSubsession(RTSPClient* rtspClient)
{
  UsageEnvironment& env = rtspClient->envir(); // alias
  StreamClientState& scs = ((MyRtspClient*)rtspClient)->scs; // alias

  scs.m_subsession = scs.m_iter->next();
  if (scs.m_subsession != NULL)
  {
    if (!scs.m_subsession->initiate())
    {
      env << *rtspClient << "Failed to initiate the \"" << *scs.m_subsession << "\" subsession: " << env.getResultMsg() << "\n";
      setupNextSubsession(rtspClient); // give up on this subsession; go to the next one
    }
    else
    {
      env << *rtspClient << "Initiated the \"" << *scs.m_subsession << "\" subsession (";
      if (scs.m_subsession->rtcpIsMuxed())
      {
        env << "client port " << scs.m_subsession->clientPortNum();
      }
      else
      {
        env << "client ports " << scs.m_subsession->clientPortNum() << "-" << scs.m_subsession->clientPortNum()+1;
      }
      env << ")\n";

      // Continue setting up this subsession, by sending a RTSP "SETUP" command:
      rtspClient->sendSetupCommand(*scs.m_subsession, continueAfterSETUP, False, REQUEST_STREAMING_OVER_TCP);
    }
    return;
  }

  // We've finished setting up all of the subsessions.  Now, send a RTSP "PLAY" command to start the streaming:
  if (scs.m_session->absStartTime() != NULL)
  {
    // Special case: The stream is indexed by 'absolute' time, so send an appropriate "PLAY" command:
    rtspClient->sendPlayCommand(*scs.m_session, continueAfterPLAY, scs.m_session->absStartTime(), scs.m_session->absEndTime());
  }
  else
  {
    scs.m_duration = scs.m_session->playEndTime() - scs.m_session->playStartTime();
    rtspClient->sendPlayCommand(*scs.m_session, continueAfterPLAY);
  }
}

void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString)
{
  do {
    UsageEnvironment& env = rtspClient->envir(); // alias
    StreamClientState& scs = ((MyRtspClient*)rtspClient)->scs; // alias

    if (resultCode != 0)
    {
      env << *rtspClient << "Failed to set up the \"" << *scs.m_subsession << "\" subsession: " << resultString << "\n";
      break;
    }

    env << *rtspClient << "Set up the \"" << *scs.m_subsession << "\" subsession (";
    if (scs.m_subsession->rtcpIsMuxed())
    {
      env << "client port " << scs.m_subsession->clientPortNum();
    }
    else
    {
      env << "client ports " << scs.m_subsession->clientPortNum() << "-" << scs.m_subsession->clientPortNum()+1;
    }
    env << ")\n";

    // Having successfully setup the subsession, create a data sink for it, and call "startPlaying()" on it.
    // (This will prepare the data sink to receive data; the actual flow of data from the client won't start happening until later,
    // after we've sent a RTSP "PLAY" command.)

    scs.m_subsession->sink = SdlVideoSink::createNew(env, *scs.m_subsession, rtspClient->url());
    // perhaps use your own custom "MediaSink" subclass instead
    if (scs.m_subsession->sink == NULL)
    {
      env << *rtspClient << "Failed to create a data sink for the \"" << *scs.m_subsession << "\" subsession: " << env.getResultMsg() << "\n";
      break;
    }

    env << *rtspClient << "Created a data sink for the \"" << *scs.m_subsession << "\" subsession\n";
    scs.m_subsession->miscPtr = rtspClient; // a hack to let subsession handler functions get the "RTSPClient" from the subsession 
    scs.m_subsession->sink->startPlaying(*(scs.m_subsession->readSource()), subsessionAfterPlaying, scs.m_subsession);
    // Also set a handler to be called if a RTCP "BYE" arrives for this subsession:
    if (scs.m_subsession->rtcpInstance() != NULL)
    {
      scs.m_subsession->rtcpInstance()->setByeWithReasonHandler(subsessionByeHandler, scs.m_subsession);
    }
  } while (0);
  delete[] resultString;

  // Set up the next subsession, if any:
  setupNextSubsession(rtspClient);
}

void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString)
{
  Boolean success = False;

  do
  {
    UsageEnvironment& env = rtspClient->envir(); // alias
    StreamClientState& scs = ((MyRtspClient*)rtspClient)->scs; // alias

    if (resultCode != 0)
    {
      env << *rtspClient << "Failed to start playing session: " << resultString << "\n";
      break;
    }

    // Set a timer to be handled at the end of the stream's expected duration (if the stream does not already signal its end
    // using a RTCP "BYE").  This is optional.  If, instead, you want to keep the stream active - e.g., so you can later
    // 'seek' back within it and do another RTSP "PLAY" - then you can omit this code.
    // (Alternatively, if you don't want to receive the entire stream, you could set this timer for some shorter value.)
    if (scs.m_duration > 0)
    {
      unsigned const delaySlop = 2; // number of seconds extra to delay, after the stream's expected duration.  (This is optional.)
      scs.m_duration += delaySlop;
      unsigned uSecsToDelay = (unsigned)(scs.m_duration * 1000000);
      scs.m_streamTimerTask = env.taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*)streamTimerHandler, rtspClient);
    }

    env << *rtspClient << "Started playing session";
    if (scs.m_duration > 0)
    {
      env << " (for up to " << scs.m_duration << " seconds)";
    }
    env << "...\n";

    success = True;
  } while (0);
  delete[] resultString;

  if (!success) {
    // An unrecoverable error occurred with this stream.
    shutdownStream(rtspClient);
  }
}

void subsessionAfterPlaying(void* clientData)
{
  MediaSubsession* subsession = (MediaSubsession*)clientData;
  RTSPClient* rtspClient = (RTSPClient*)(subsession->miscPtr);

  // Begin by closing this subsession's stream:
  Medium::close(subsession->sink);
  subsession->sink = NULL;

  // Next, check whether *all* subsessions' streams have now been closed:
  MediaSession& session = subsession->parentSession();
  MediaSubsessionIterator iter(session);
  while ((subsession = iter.next()) != NULL)
  {
    if (subsession->sink != NULL)
    {
      return; // this subsession is still active
    }
  }

  // All subsessions' streams have now been closed, so shutdown the client:
  shutdownStream(rtspClient);
}

void subsessionByeHandler(void* clientData, char const* reason)
{
  MediaSubsession* subsession = (MediaSubsession*)clientData;
  RTSPClient* rtspClient = (RTSPClient*)subsession->miscPtr;
  UsageEnvironment& env = rtspClient->envir(); // alias

  env << *rtspClient << "Received RTCP \"BYE\"";
  if (reason != NULL)
  {
    env << " (reason:\"" << reason << "\")";
    delete[] (char*)reason;
  }
  env << " on \"" << *subsession << "\" subsession\n";

  // Now act as if the subsession had closed:
  subsessionAfterPlaying(subsession);
}

void streamTimerHandler(void* clientData) {
  MyRtspClient* rtspClient = (MyRtspClient*)clientData;
  StreamClientState& scs = rtspClient->scs; // alias

  scs.m_streamTimerTask = nullptr;

  // Shut down the stream:
  shutdownStream(rtspClient);
}

void shutdownStream(RTSPClient* rtspClient, int exitCode)
{
  UsageEnvironment& env = rtspClient->envir(); // alias
  StreamClientState& scs = ((MyRtspClient*)rtspClient)->scs; // alias

  // First, check whether any subsessions have still to be closed:
  if (scs.m_session != NULL)
  {
    Boolean someSubsessionsWereActive = False;
    MediaSubsessionIterator iter(*scs.m_session);
    MediaSubsession* subsession;

    while ((subsession = iter.next()) != NULL)
    {
      if (subsession->sink != NULL)
      {
        Medium::close(subsession->sink);
        subsession->sink = NULL;

        if (subsession->rtcpInstance() != NULL)
        {
          subsession->rtcpInstance()->setByeHandler(NULL, NULL); // in case the server sends a RTCP "BYE" while handling "TEARDOWN"
        }
        someSubsessionsWereActive = True;
      }
    }

    if (someSubsessionsWereActive)
    {
      // Send a RTSP "TEARDOWN" command, to tell the server to shutdown the stream.
      // Don't bother handling the response to the "TEARDOWN".
      rtspClient->sendTeardownCommand(*scs.m_session, NULL);
    }
  }

  env << *rtspClient << "Closing the stream.\n";
  Medium::close(rtspClient);
  // Note that this will also cause this stream's "StreamClientState" structure to get reclaimed.

  if (--rtspClientCount == 0)
  {
    // The final stream has ended, so exit the application now.
    // (Of course, if you're embedding this code into your own application, you might want to comment this out,
    // and replace it with "eventLoopWatchVariable = 1;", so that we leave the LIVE555 event loop, and continue running "main()".)
    exit(exitCode);
  }
}

