
#ifndef VIDEO_STATE_H_
#define VIDEO_STATE_H_

#include <string>
#include <atomic>
#include <memory>
#include "packetqueue.h"
#include "videopicture.h"

extern "C"
{
#include <SDL.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixfmt.h>
#include <libavutil/time.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE 192000

#define VIDEO_PICTURE_QUEUE_SIZE 1

namespace player
{

enum class SYNC_TYPE
{
  // sync to audio clock
  AV_SYNC_AUDIO_MASTER,
  // sync to video clock
  AV_SYNC_VIDEO_MASTER,
  // sync to external clock : the computer clock
  AV_SYNC_EXTERNAL_MASTER,
};

class VideoState
{
public:
  explicit VideoState();
  ~VideoState();

  // Common
  AVFormatContext*& formatCtx() { return m_formatCtx; }
  int& videoStreamIndex() { return m_videoStreamIndex; }
  int& audioStreamIndex() { return m_audioStreamIndex; }
  AVPacket*& flushPacket() { return m_flushPkt; }
  AVCodecContext*& videoCodecCtx() { return m_videoCtx; }
  AVStream*& videoStream() { return m_videoStream; }
  AVCodecContext*& audioCodecCtx() { return m_audioCtx; }
  AVStream*& audioStream() { return m_audioStream; }
  int outputAudioDeviceIndex() const { return m_outputAudioDeviceIndex; }
  void setOutputAudioDeviceIndex(const int& outputAudioDeviceIndex) { m_outputAudioDeviceIndex = outputAudioDeviceIndex; }
  SDL_AudioDeviceID sdlAudioDeviceID() const { return m_sdlAudioDeviceID; }
  void setSdlAudioDeviceID(const SDL_AudioDeviceID& audioDeviceID) { m_sdlAudioDeviceID = audioDeviceID; };
  bool isPlayerFinished() const { return m_isPlayerFinished; }
  void setPlayerFinished() { m_isPlayerFinished = true; }
  int& videoPictureQueueSize() { return m_pictqSize; }
  int& videoPictureQueueRIndex() { return m_pictqRindex; }
  int& videoPictureQueueWIndex() { return m_pictqWindex; }
  VideoPicture& videoPicture() { return m_pictureQueue[m_pictqRindex]; }
  SDL_mutex*& pictureQueueMutex() { return m_pictqMutex; }
  SDL_cond*& pictureQueueCond() { return m_pictqCond; }
  SYNC_TYPE syncType() const { return m_avSyncType; }
  void setSyncType(const SYNC_TYPE& syncType) { m_avSyncType = syncType; }
  int queuePicture(AVFrame* pFrame, const double& pts);

  // For Read(Audio/Video)
  int pushAudioPacketRead(AVPacket* packet);
  int pushVideoPacketRead(AVPacket* packet);
  int popAudioPacketRead(AVPacket* packet);
  int popVideoPacketRead(AVPacket* packet);
  int sizeAudioPacketRead() const { return m_audioPacketQueue.size(); }
  int sizeVideoPacketRead() const { return m_videoPacketQueue.size(); }
  int nbPacketsAudioRead() const { return m_audioPacketQueue.nbPackets(); }
  int nbPacketsVideoRead() const { return m_videoPacketQueue.nbPackets(); }
  void clearAudioPacketRead() { m_audioPacketQueue.clear(); }
  void clearVideoPacketRead() { m_videoPacketQueue.clear(); }


  // For Video Decode
  struct SwsContext*& decodeVideoSwsCtx() { return m_decodeVideoSwsCtx; }
  SDL_mutex*& screenMutex() { return m_screenMutex; }
  double frameDecodeTimer() const { return m_frameDecodeTimer; }
  void setFrameDecodeTimer(const double& frameTimer) { m_frameDecodeTimer = frameTimer; }
  double frameDecodeLastPts() const { return m_frameDecodeLastPts; }
  void setFrameDecodeLastPts(const double& frameLastPts) { m_frameDecodeLastPts = frameLastPts; }
  double frameDecodeLastDelay() const { return m_frameDecodeLastDelay; }
  void setFrameDecodeLastDelay(const double& frameLastDelay) { m_frameDecodeLastDelay = frameLastDelay; }
  double videoClock() const { return m_videoClock; }
  void setVideoClock(const double& videoClock) { m_videoClock = videoClock; }
  double videoDecodeCurrentPts() const { return m_videoDecodeCurrentPts; }
  void setVideoDecodeCurrentPts(const double& videoCurrentPts) { m_videoDecodeCurrentPts = videoCurrentPts; }
  int64_t videoDecodeCurrentPtsTime() const { return m_videoDecodeCurrentPtsTime; }
  void setVideoDecodeCurrentPtsTime(const int64_t& videoCurrentPtsTime) { m_videoDecodeCurrentPtsTime = videoCurrentPtsTime; }

  // For Audio Decode  
  double audioClock() const { return m_audioClock; }
  void setAudioClock(const double& audioClock) { m_audioClock = audioClock; }
  unsigned int audioBufSize() const { return m_audioBufSize; }
  void setAudioBufSize(const unsigned int& audioBufSize) { m_audioBufSize = audioBufSize; }
  unsigned int audioBufIndex() const { return m_audioBufIndex; }
  void setAudioBufIndex(const unsigned int& audioBufIndex) { m_audioBufIndex = audioBufIndex; }
  double audioDiffCum() const { return m_audioDiffCum; }
  void setAudioDiffCum(const double& diffCum) { m_audioDiffCum = diffCum; }
  double audioDiffAvgCoef() const { return m_audioDiffAvgCoef; }
  void setAudioDiffAvgCoef(const double& diffAvgCoef) { m_audioDiffAvgCoef = diffAvgCoef; }
  double audioDiffThreshold() const { return m_audioDiffThreshold; }
  void setAudioDiffThreshold(const double& diffThreshold) { m_audioDiffThreshold = diffThreshold; }
  double audioDiffAvgCount() const { return m_audioDiffAvgCount; }
  void setAudioDiffAvgCount(const double& diffAvgCount) { m_audioDiffAvgCount = diffAvgCount; }
  uint8_t* audioArrayBuf() { return m_audioBuf; }
  int audioArrayBufSize() const { return (MAX_AUDIO_FRAME_SIZE * 3) / 2; }

  // For calculate clock.
  double masterClock();
  double calcAudioClock(); // For AudioDecoder

  // For Seek
  int seekRequest() const { return m_seekReq; }
  void setSeekRequest(const int& req) { m_seekReq = req; }
  int64_t seekPos() const { return m_seekPos; }
  int seekFlags() const { return m_seekFlags; }
  void streamSeek(const int64_t& pos, const int& rel);

private:
  void allocPicture();
  double calcVideoClock();
  double calcExternalClock();

  AVFormatContext* m_formatCtx = nullptr;

  // audio
  int m_audioStreamIndex = -1;
  AVStream* m_audioStream = nullptr;
  AVCodecContext* m_audioCtx = nullptr;
  PacketQueue m_audioPacketQueue;
  uint8_t m_audioBuf[(MAX_AUDIO_FRAME_SIZE * 3) / 2]{};
  unsigned int m_audioBufSize = 0;
  unsigned int m_audioBufIndex = 0;
  int m_audioPktSize = 0;
  double m_audioClock = 0.0;
  double m_audioDiffCum = 0.0;
  double m_audioDiffAvgCoef = 0.0;
  double m_audioDiffThreshold = 0.0;
  int m_audioDiffAvgCount = 0;

  // video
  int m_videoStreamIndex = -1;
  AVStream* m_videoStream = nullptr;
  AVCodecContext* m_videoCtx = nullptr;
  PacketQueue m_videoPacketQueue;
  struct SwsContext* m_decodeVideoSwsCtx = nullptr;
  double m_frameDecodeTimer = 0.0;
  double m_frameDecodeLastPts = 0.0;
  double m_frameDecodeLastDelay = 0.0;
  double m_videoClock = 0.0;
  double m_videoDecodeCurrentPts = 0.0;
  int64_t m_videoDecodeCurrentPtsTime = 0;
  // SDL_surface mutex
  SDL_mutex* m_screenMutex = nullptr;

  // av sync
  SYNC_TYPE m_avSyncType = SYNC_TYPE::AV_SYNC_AUDIO_MASTER;
  double m_externalClock = 0.0;
  int64_t m_externalClockTime = 0;

  // seeking
  int m_seekReq = 0;
  int m_seekFlags = 0;
  int64_t m_seekPos = 0;

  // video picture queue
  VideoPicture m_pictureQueue[VIDEO_PICTURE_QUEUE_SIZE]{};
  int m_pictqSize = 0;
  int m_pictqRindex = 0;
  int m_pictqWindex = 0;
  SDL_mutex* m_pictqMutex = nullptr;
  SDL_cond* m_pictqCond = nullptr;


  // output audio device index in windows
  int m_outputAudioDeviceIndex = -1;
  SDL_AudioDeviceID m_sdlAudioDeviceID = 0;

  //
  AVPacket* m_flushPkt = nullptr;

  std::atomic_bool m_isPlayerFinished = false;

};

} // player

#endif // VIDEO_STATE_H_

