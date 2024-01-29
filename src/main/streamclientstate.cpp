
#include "streamclientstate.h"

using namespace client;

StreamClientState::~StreamClientState()
{
  delete m_iterator;
  if (m_session != nullptr)
  {
    UsageEnvironment& env = m_session->envir();
    env.taskScheduler().unscheduleDelayedTask(m_streamTimerTask);
    Medium::close(m_session);
  }
}


