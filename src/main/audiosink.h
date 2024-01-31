
#ifndef AUDIO_SINK_H_
#define AUDIO_SINK_H_

#include <string>

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
}

#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>

namespace client
{

class AudioSink : public MediaSink
{
public:
  static AudioSink* createNew(UsageEnvironment& env, MediaSubsession& subsession, char const* streamURL);

private:
  explicit AudioSink(UsageEnvironment& env, MediaSubsession& subsession, char const* streamURL);
  virtual ~AudioSink();

  static void afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes
                                , struct timeval presentationTime, unsigned durationInMicroseconds);
  void afterGettingFrame(unsigned frameSize, unsigned numTruncatedBytes
                         , struct timeval presentationTime, unsigned durationInMicroseconds);

  virtual Boolean continuePlaying();
  bool init(char const* streamURL);
  void releaseFormatCtx(AVFormatContext*& formatctx);

  u_int8_t* m_receiveBuffer = nullptr;
  MediaSubsession& m_subsession;
  char* m_streamURL = nullptr;

  AVCodecContext* m_audioCodecContext = nullptr;
};

} // player

#endif // AUDIO_SINK_H_

