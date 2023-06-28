
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

  int openInputFile(const std::wstring url);
  void run();
  void stop();

private:
  AVFormatContext* m_fmtCtx = nullptr;
  const AVCodec* m_videoCodec = nullptr;
  AVCodecContext* m_videoCodecCtx = nullptr;
  AVPacket* m_pkt = nullptr;
  AVFrame* m_yuvFrame = nullptr;
  AVFrame* m_rgbFrame = nullptr;

  struct SwsContext* m_imgCtx = nullptr;
  unsigned char* m_outBuffer = nullptr;

  int m_videoStreamIndex = -1;
  int m_numBytes = -1;

  DecoderStatus m_decoderStatus = DecoderStatus::NONE;
};

#endif // FFMPEG_DECODER_H_
