
#ifndef FFMPEG_DECODER_H_
#define FFMPEG_DECODER_H_

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
}

#include <string>
#include <mutex>

class FFMPEGDecoder
{
  enum class DecoderStatus
  {
    NONE = 0
    , READY = 1
    , START = 2
    , STOP = 3
  };

public:
  explicit FFMPEGDecoder();
  ~FFMPEGDecoder();

  int openInputFile(const std::wstring url, uint32_t& outFrameWidth, uint32_t& outFrameHeight);
  void run();
  void stop();

private:
  int setupVideoCodec();
  int setupAudioCodec();

  // for audio and video
  AVFormatContext* m_fmtCtx = nullptr;
  AVDictionary* m_rtspOptions = nullptr;
  AVPacket* m_pkt = nullptr;
  DecoderStatus m_decoderStatus = DecoderStatus::NONE;
  std::mutex m_mutex;

  // for audio
  const AVCodec* m_audioCodec = nullptr;
  AVCodecContext* m_audioCodecCtx = nullptr;
  AVFrame* m_pcmFrame = nullptr;
  int m_audioStreamIndex = -1;

  // for video
  const AVCodec* m_videoCodec = nullptr;
  AVCodecContext* m_videoCodecCtx = nullptr;
  AVFrame* m_yuvFrame = nullptr;
  AVFrame* m_rgbFrame = nullptr;
  int m_videoStreamIndex = -1;
  int m_numBytes = -1;
  struct SwsContext* m_imgCtx = nullptr;
  unsigned char* m_outBuffer = nullptr;

};

#endif // FFMPEG_DECODER_H_
