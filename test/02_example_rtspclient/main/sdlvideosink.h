
#ifndef SDL_VIDEO_SINK_H_
#define SDL_VIDEO_SINK_H_

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/avstring.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <SDL.h>
#include <SDL_mutex.h>
}

#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>

namespace client
{

class SdlVideoSink : public MediaSink
{
public:
  static SdlVideoSink* createNew(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId = nullptr);

private:
  explicit SdlVideoSink(UsageEnvironment& env, MediaSubsession& subsession, char const* stramId);
  virtual ~SdlVideoSink();

  static void afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes
                                , struct timeval presentationTime, unsigned durationInMicroseconds);
  void afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes
                         , struct timeval presentationTime, unsigned durationInMicroseconds);

  virtual Boolean continuePlaying();
  bool initDecRender();

  u_int8_t* m_receiveBuffer = nullptr;
  MediaSubsession& m_subsession;
  char* m_streamId = nullptr;

  AVCodecContext* m_videoCodecContext = nullptr;

  struct PlayContext
  {
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    SDL_mutex* mutex = nullptr;
    SDL_Window* window = nullptr;
    SDL_Event event;
  } m_playerContext;
};

} // player

#endif // SDL_VIDEO_SINK_H_

