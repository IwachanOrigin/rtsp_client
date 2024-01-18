
#ifndef OPTIONS_H_
#define OPTIONS_H_

namespace player
{
  struct Options
  {
    int audioIndex = 0;
    int syncType = 0;
    int scanAllPmts = 0;
    int rtspTransport = 0;
    int maxDelay = 0;
    int stimeout = 0;
    int bufferSize = 0;
  };
} // player

#endif // OPTIONS_H_
