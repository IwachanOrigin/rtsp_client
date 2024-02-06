
#include <string>
#include "rtspcontroller.h"
#include "streamclientstate.h"
#include "basertspclient.h"
#include "videosink.h"
#include "audiosink.h"

using namespace client;

// by default, print verbose output from each "RTSPClient"
constexpr int RTSP_CLIENT_VERBOSITY_LEVEL = 1;

// By default, we request that the server stream its data using RTP/UDP.
// If, instead, you want to request that the server stream via RTP-over-TCP, change the following to True:
constexpr bool REQUEST_STREAMING_OVER_TCP = false;

// Counts how many streams (i.e., "RTSPClient"s) are currently in use.
static unsigned m_rtspClientCount;

static MediaSubsession* m_workMediaSubsession;

static std::shared_ptr<FrameContainer> m_frameContainer;

RtspController::RtspController()
{
  m_rtspClientCount = 0;
  m_workMediaSubsession = nullptr;
  m_frameContainer = nullptr;
}

void RtspController::openURL(char const* progName, char const* rtspURL)
{
  // Begin by setting up our usage environment:
  TaskScheduler* scheduler = BasicTaskScheduler::createNew();
  m_env = BasicUsageEnvironment::createNew(*scheduler);

  // Begin by creating a "RTSPClient" object.  Note that there is a separate "RTSPClient" object for each stream that we wish
  // to receive (even if more than stream uses the same "rtsp://" URL).
  RTSPClient* rtspClient = BaseRtspClient::createNew(*m_env, rtspURL, RTSP_CLIENT_VERBOSITY_LEVEL, progName);
  if (rtspClient == NULL)
  {
    *m_env << "Failed to create a RTSP client for URL \"" << rtspURL << "\": " << m_env->getResultMsg() << "\n";
    return;
  }

  ++m_rtspClientCount;

  // Next, send a RTSP "DESCRIBE" command, to get a SDP description for the stream.
  // Note that this command - like all RTSP commands - is sent asynchronously; we do not block, waiting for a response.
  // Instead, the following function call returns immediately, and we handle the RTSP response later, from within the event loop:
  rtspClient->sendDescribeCommand(client::RtspController::continueAfterDESCRIBE);
}

void RtspController::setFrameContainer(std::shared_ptr<FrameContainer> frameContainer)
{
  m_frameContainer = frameContainer;
}

bool RtspController::isRtspClientFinished()
{
  if (m_rtspClientCount <= 0 || m_eventLoopWatchVariable != 0)
  {
    return true;
  }
  return false;
}

void RtspController::setStopStreaming()
{
  m_eventLoopWatchVariable = 1;
}

void RtspController::eventloop()
{
  m_env->taskScheduler().doEventLoop(&m_eventLoopWatchVariable);
  // This function call does not return, unless, at some point in time, "eventLoopWatchVariable" gets set to something non-zero.
}

void RtspController::continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString)
{
  do
  {
    UsageEnvironment& env = rtspClient->envir(); // alias
    StreamClientState& scs = ((BaseRtspClient*)rtspClient)->streamClientState(); // alias

    if (resultCode != 0)
    {
      env << "[URL:\"" << rtspClient->url() << "\"]: " << "Failed to get a SDP description: " << resultString << "\n";
      delete[] resultString;
      break;
    }

    char* const sdpDescription = resultString;
    env << "[URL:\"" << rtspClient->url() << "\"]: " << "Got a SDP description:\n" << sdpDescription << "\n";

    // Create a media session object from this SDP description:
    auto& session = scs.session();
    session = MediaSession::createNew(env, sdpDescription);
    delete[] sdpDescription; // because we don't need it anymore
    if (session == nullptr)
    {
      env << "[URL:\"" << rtspClient->url() << "\"]: " << "Failed to create a MediaSession object from the SDP description: " << env.getResultMsg() << "\n";
      break;
    }
    else if (!session->hasSubsessions())
    {
      env << "[URL:\"" << rtspClient->url() << "\"]: " << "This session has no media subsessions (i.e., no \"m=\" lines)\n";
      break;
    }

    // Then, create and set up our data source objects for the session.  We do this by iterating over the session's 'subsessions',
    // calling "MediaSubsession::initiate()", and then sending a RTSP "SETUP" command, on each one.
    // (Each 'subsession' will have its own data source.)
    auto& iterator = scs.iterator();
    iterator = new MediaSubsessionIterator(*session);
    setupNextSubsession(rtspClient);
    return;
  } while (0);

  // An unrecoverable error occurred with this stream.
  shutdownStream(rtspClient);
}

void RtspController::continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString)
{
  do {
    UsageEnvironment& env = rtspClient->envir(); // alias
    StreamClientState& scs = ((BaseRtspClient*)rtspClient)->streamClientState(); // alias
    MediaSubsession* subsession = nullptr;

    if (!m_workMediaSubsession)
    {
      break;
    }

    if (resultCode != 0)
    {
      env << "[URL:\"" << rtspClient->url() << "\"]: " << "Failed to set up the \"" << m_workMediaSubsession << "\" subsession: " << resultString << "\n";
      break;
    }

    env << "[URL:\"" << rtspClient->url() << "\"]: " << "Set up the \"" << subsession << "\" subsession (";
    if (m_workMediaSubsession->rtcpIsMuxed())
    {
      env << "client port " << m_workMediaSubsession->clientPortNum();
    }
    else
    {
      env << "client ports " << m_workMediaSubsession->clientPortNum() << "-" << m_workMediaSubsession->clientPortNum() + 1;
    }
    env << ")\n";

    env << "subsession : " << m_workMediaSubsession->mediumName() << "/" << m_workMediaSubsession->codecName() << "\n";
    const std::string mediaVideo = "video";
    const std::string mediaAudio = "audio";
    if (mediaVideo.compare(m_workMediaSubsession->mediumName()) == 0) // Video
    {
      subsession = scs.videoSubsession();
      subsession = m_workMediaSubsession;
      // Having successfully setup the subsession, create a data sink for it, and call "startPlaying()" on it.
      // (This will prepare the data sink to receive data; the actual flow of data from the client won't start happening until later,
      // after we've sent a RTSP "PLAY" command.)
      subsession->sink = VideoSink::createNew(env, *subsession, rtspClient->url(), m_frameContainer);
    }
    else if (mediaAudio.compare(m_workMediaSubsession->mediumName()) == 0) // Audio
    {
      subsession = scs.audioSubsession();
      subsession = m_workMediaSubsession;
      // Having successfully setup the subsession, create a data sink for it, and call "startPlaying()" on it.
      // (This will prepare the data sink to receive data; the actual flow of data from the client won't start happening until later,
      // after we've sent a RTSP "PLAY" command.)
      subsession->sink = AudioSink::createNew(env, *subsession, rtspClient->url(), m_frameContainer);
    }
    else
    {
      continue;
    }

    // perhaps use your own custom "MediaSink" subclass instead
    if (subsession->sink == nullptr)
    {
      env << "[URL:\"" << rtspClient->url() << "\"]: " << "Failed to create a data sink for the \"" << subsession << "\" subsession: " << env.getResultMsg() << "\n";
      break;
    }

    env << "[URL:\"" << rtspClient->url() << "\"]: " << "Created a data sink for the \"" << subsession << "\" subsession\n";
    subsession->miscPtr = rtspClient; // a hack to let subsession handler functions get the "RTSPClient" from the subsession 
    subsession->sink->startPlaying(*(subsession->readSource()), subsessionAfterPlaying, subsession);
    // Also set a handler to be called if a RTCP "BYE" arrives for this subsession:
    if (subsession->rtcpInstance() != nullptr)
    {
      subsession->rtcpInstance()->setByeWithReasonHandler(subsessionByeHandler, subsession);
    }
  } while (0);
  delete[] resultString;

  // Set up the next subsession, if any:
  setupNextSubsession(rtspClient);
}

void RtspController::continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString)
{
  Boolean success = False;

  do
  {
    UsageEnvironment& env = rtspClient->envir(); // alias
    StreamClientState& scs = ((BaseRtspClient*)rtspClient)->streamClientState(); // alias

    if (resultCode != 0)
    {
      env << "[URL:\"" << rtspClient->url() << "\"]: " << "Failed to start playing session: " << resultString << "\n";
      break;
    }

    // Set a timer to be handled at the end of the stream's expected duration (if the stream does not already signal its end
    // using a RTCP "BYE").  This is optional.  If, instead, you want to keep the stream active - e.g., so you can later
    // 'seek' back within it and do another RTSP "PLAY" - then you can omit this code.
    // (Alternatively, if you don't want to receive the entire stream, you could set this timer for some shorter value.)
    auto duration = scs.duration();
    if (duration > 0)
    {
      unsigned const delaySlop = 2; // number of seconds extra to delay, after the stream's expected duration.  (This is optional.)
      duration += delaySlop;
      scs.setDuration(duration);
      unsigned uSecsToDelay = (unsigned)(duration * 1000000);
      auto& streamTimerTask = scs.streamTimerTask();
      streamTimerTask = env.taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*)streamTimerHandler, rtspClient);
    }

    env << "[URL:\"" << rtspClient->url() << "\"]: " << "Started playing session";
    if (duration > 0)
    {
      env << " (for up to " << duration << " seconds)";
    }
    env << "...\n";

    success = True;
  } while (0);
  delete[] resultString;

  if (!success)
  {
    // An unrecoverable error occurred with this stream.
    shutdownStream(rtspClient);
  }
}

void RtspController::subsessionAfterPlaying(void* clientData)
{
  // called when a stream's subsession (e.g., audio or video substream) ends
  MediaSubsession* subsession = (MediaSubsession*)clientData;
  RTSPClient* rtspClient = (RTSPClient*)(subsession->miscPtr);

  // Release frames
  const std::string mediaVideo = "video";
  const std::string mediaAudio = "audio";
  if (mediaVideo.compare(subsession->mediumName()) == 0) // Video
  {
    m_frameContainer->clearVideoFrameDecoded();
  }
  else if (mediaAudio.compare(subsession->mediumName()) == 0) // Audio
  {
    m_frameContainer->clearAudioFrameDecoded();
  }

  // Begin by closing this subsession's stream:
  Medium::close(subsession->sink);
  subsession->sink = nullptr;

  // Next, check whether *all* subsessions' streams have now been closed:
  MediaSession& session = subsession->parentSession();
  MediaSubsessionIterator iter(session);
  while ((subsession = iter.next()) != nullptr)
  {
    if (subsession->sink != nullptr)
    {
      return; // this subsession is still active
    }
  }

  // All subsessions' streams have now been closed, so shutdown the client:
  shutdownStream(rtspClient);
}

void RtspController::subsessionByeHandler(void* clientData, char const* reason)
{
  MediaSubsession* subsession = (MediaSubsession*)clientData;
  RTSPClient* rtspClient = (RTSPClient*)subsession->miscPtr;
  UsageEnvironment& env = rtspClient->envir(); // alias

  env << "[URL:\"" << rtspClient->url() << "\"]: " << "Received RTCP \"BYE\"";
  if (reason != nullptr)
  {
    env << " (reason:\"" << reason << "\")";
    delete[] (char*)reason;
  }
  env << " on \"" << "[URL:\"" << rtspClient->url() << "\"]: " << "\" subsession\n";

  // Now act as if the subsession had closed:
  subsessionAfterPlaying(subsession);
}

// called when a RTCP "BYE" is received for a subsession
void RtspController::streamTimerHandler(void* clientData)
{
  BaseRtspClient* rtspClient = (BaseRtspClient*)clientData;
  StreamClientState& scs = rtspClient->streamClientState(); // alias
  auto& streamTimerTask = scs.streamTimerTask();

  streamTimerTask = nullptr;

  // Shut down the stream:
  shutdownStream(rtspClient);
}

// Used to iterate through each stream's 'subsessions', setting up each one:
void RtspController::setupNextSubsession(RTSPClient* rtspClient)
{
  UsageEnvironment& env = rtspClient->envir(); // alias
  StreamClientState& scs = ((BaseRtspClient*)rtspClient)->streamClientState(); // alias
  auto subsession = scs.iterator()->next();

  if (subsession != nullptr)
  {
    if (!subsession->initiate())
    {
      env << "[URL:\"" << rtspClient->url() << "\"]: " << "Failed to initiate the \"" << subsession << "\" subsession: " << env.getResultMsg() << "\n";
      setupNextSubsession(rtspClient); // give up on this subsession; go to the next one
    }
    else
    {
      env << "[URL:\"" << rtspClient->url() << "\"]: " << "Initiated the \"" << subsession << "\" subsession (";
      if (subsession->rtcpIsMuxed())
      {
        env << "client port " << subsession->clientPortNum();
      }
      else
      {
        env << "client ports " << subsession->clientPortNum() << "-" << subsession->clientPortNum()+1;
      }
      env << ")\n";

      m_workMediaSubsession = subsession;
      // Continue setting up this subsession, by sending a RTSP "SETUP" command:
      rtspClient->sendSetupCommand(*subsession, continueAfterSETUP, False, REQUEST_STREAMING_OVER_TCP);
    }
    return;
  }

  // We've finished setting up all of the subsessions.  Now, send a RTSP "PLAY" command to start the streaming:
  auto& session = scs.session();
  if (session->absStartTime() != nullptr)
  {
    // Special case: The stream is indexed by 'absolute' time, so send an appropriate "PLAY" command:
    rtspClient->sendPlayCommand(*session, continueAfterPLAY, session->absStartTime(), session->absEndTime());
  }
  else
  {
    auto duration = scs.duration();
    duration = session->playEndTime() - session->playStartTime();
    scs.setDuration(duration);
    rtspClient->sendPlayCommand(*session, continueAfterPLAY);
  }
}

// Used to shut down and close a stream (including its "RTSPClient" object):
void RtspController::shutdownStream(RTSPClient* rtspClient, int exitCode)
{
  UsageEnvironment& env = rtspClient->envir(); // alias
  StreamClientState& scs = ((BaseRtspClient*)rtspClient)->streamClientState(); // alias
  auto& session = scs.session();

  // First, check whether any subsessions have still to be closed:
  if (session != nullptr)
  {
    Boolean someSubsessionsWereActive = False;
    MediaSubsessionIterator iter(*session);
    MediaSubsession* subsession = nullptr;

    while ((subsession = iter.next()) != nullptr)
    {
      if (subsession->sink != nullptr)
      {
        Medium::close(subsession->sink);
        subsession->sink = nullptr;

        if (subsession->rtcpInstance() != nullptr)
        {
          subsession->rtcpInstance()->setByeHandler(nullptr, nullptr); // in case the server sends a RTCP "BYE" while handling "TEARDOWN"
        }
        someSubsessionsWereActive = True;
      }
    }

    if (someSubsessionsWereActive)
    {
      // Send a RTSP "TEARDOWN" command, to tell the server to shutdown the stream.
      // Don't bother handling the response to the "TEARDOWN".
      rtspClient->sendTeardownCommand(*session, nullptr);
    }
  }

  env << "[URL:\"" << rtspClient->url() << "\"]: " << "Closing the stream.\n";
  Medium::close(rtspClient);
  // Note that this will also cause this stream's "StreamClientState" structure to get reclaimed.

  if (--m_rtspClientCount == 0)
  {
    // The final stream has ended, so exit the application now.
    // (Of course, if you're embedding this code into your own application, you might want to comment this out,
    // and replace it with "eventLoopWatchVariable = 1;", so that we leave the LIVE555 event loop, and continue running "main()".)
    exit(exitCode);
  }
}

