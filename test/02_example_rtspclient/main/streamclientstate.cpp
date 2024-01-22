
#include "streamclientstate.h"

using namespace client;

StreamClientState::StreamClientState()
  : m_iter(nullptr)
  , m_session(nullptr)
  , m_subsession(nullptr)
  , m_streamTimerTask(nullptr)
  , m_duration(0.0)
{
}

StreamClientState::~StreamClientState()
{
  delete m_iter;
  if (m_session != nullptr)
  {
    UsageEnvironment& env = m_session->envir();
    env.taskScheduler().unscheduleDelayedTask(m_streamTimerTask);
    Medium::close(m_session);
  }
}


