
#include "dummysink.h"

using namespace client;

// Even though we're not going to be doing anything with the incoming data, we still need to receive it.
// Define the size of the buffer that we'll use:
#define DUMMY_SINK_RECEIVE_BUFFER_SIZE 100000

DummySink* DummySink::createNew(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId)
{
  return new DummySink(env, subsession, streamId);
}

DummySink::DummySink(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId)
  : MediaSink(env)
  , m_subsession(subsession)
{
  m_streamId = strDup(streamId);
  m_receiveBuffer = new u_int8_t[DUMMY_SINK_RECEIVE_BUFFER_SIZE];
}

DummySink::~DummySink()
{
  delete[] m_receiveBuffer;
  delete[] m_streamId;
}

void DummySink::afterGettingFrame(
  void* clientData
  , unsigned frameSize
  , unsigned numTruncatedBytes
  , struct timeval presentationTime
  , unsigned durationInMicroseconds
  )
{
  DummySink* sink = (DummySink*)clientData;
  sink->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

// If you don't want to see debugging output for each received frame, then comment out the following line:
#define DEBUG_PRINT_EACH_RECEIVED_FRAME 1

void DummySink::afterGettingFrame(
  unsigned frameSize
  , unsigned numTruncatedBytes
  , struct timeval presentationTime
  , unsigned /*durationInMicroseconds*/)
{
  // We've just received a frame of data.  (Optionally) print out information about it:
#ifdef DEBUG_PRINT_EACH_RECEIVED_FRAME
  if (m_streamId != NULL)
  {
    envir() << "Stream \"" << m_streamId << "\"; ";
  }
  envir() << m_subsession.mediumName() << "/" << m_subsession.codecName() << ":\tReceived " << frameSize << " bytes";
  if (numTruncatedBytes > 0)
  {
    envir() << " (with " << numTruncatedBytes << " bytes truncated)";
  }
  char uSecsStr[6+1]{}; // used to output the 'microseconds' part of the presentation time
  sprintf(uSecsStr, "%06u", (unsigned)presentationTime.tv_usec);
  envir() << ".\tPresentation time: " << (int)presentationTime.tv_sec << "." << uSecsStr;
  if (m_subsession.rtpSource() != NULL && !m_subsession.rtpSource()->hasBeenSynchronizedUsingRTCP())
  {
    envir() << "!"; // mark the debugging output to indicate that this presentation time is not RTCP-synchronized
  }
#ifdef DEBUG_PRINT_NPT
  envir() << "\tNPT: " << fSubsession.getNormalPlayTime(presentationTime);
#endif
  envir() << "\n";
#endif

  // Then continue, to request the next frame of data:
  continuePlaying();
}

Boolean DummySink::continuePlaying()
{
  if (fSource == NULL)
  {
    return False; // sanity check (should not happen)
  }
  // Request the next frame of data from our input source.  "afterGettingFrame()" will get called later, when it arrives:
  fSource->getNextFrame(
    m_receiveBuffer
    , DUMMY_SINK_RECEIVE_BUFFER_SIZE
    , afterGettingFrame
    , this
    , onSourceClosure
    , this);
  return True;
}

