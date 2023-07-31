
#ifndef VIDEO_STATE_H_
#define VIDEO_STATE_H_

#include <string>
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

#define DEFAULT_AV_SYNC_TYPE SYNC_TYPE::AV_SYNC_AUDIO_MASTER

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

  int queue_picture(AVFrame *pFrame, double pts);

  double get_master_clock();
  double get_video_clock();
  double get_audio_clock();
  double get_external_clock();
  void stream_seek(int64_t pos, int rel);

  AVFormatContext *pFormatCtx;

  // audio
  int audioStream;
  AVStream* audio_st;
  AVCodecContext* audio_ctx;
  PacketQueue audioq;
  uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) /2];
  unsigned int audio_buf_size;
  unsigned int audio_buf_index;
  AVFrame audio_frame;
  AVPacket audio_pkt;
  uint8_t* audio_pkt_data;
  int audio_pkt_size;
  double audio_clock;

  // video
  int videoStream;
  AVStream* video_st;
  AVCodecContext* video_ctx;
  SDL_Texture* texture;
  SDL_Renderer* renderer;
  PacketQueue videoq;
  struct SwsContext *sws_ctx;
  double frame_timer;
  double frame_last_pts;
  double frame_last_delay;
  double video_clock;
  double video_current_pts;
  int64_t video_current_pts_time;
  double audio_diff_cum;
  double audio_diff_avg_coef;
  double audio_diff_threshold;
  int audio_diff_avg_count;

  // av sync
  SYNC_TYPE av_sync_type;
  double external_clock;
  int64_t external_clock_time;

  // seeking
  int seek_req;
  int seek_flags;
  int64_t seek_pos;

  // SDL_surface mutex
  SDL_mutex* screen_mutex;

  // video picture queue
  VideoPicture pictq[VIDEO_PICTURE_QUEUE_SIZE];
  int pictq_size;
  int pictq_rindex;
  int pictq_windex;
  SDL_mutex* pictq_mutex;
  SDL_cond* pictq_cond;

  // file name
  std::string filename;

  // output audio device index
  int output_audio_device_index;

  // quit flag
  int quit;

  //
  AVPacket* flush_pkt;

private:
  void alloc_picture();
};

#endif // VIDEO_STATE_H_

