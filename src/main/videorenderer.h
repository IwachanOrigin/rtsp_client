
#ifndef VIDEO_RENDERER_H_
#define VIDEO_RENDERER_H_

#include "videostate.h"

class VideoRenderer
{
public:
  explicit VideoRenderer();
  ~VideoRenderer();

  int start(VideoState *videoState);

private:
  VideoState *m_videoState;
  SDL_Window *m_screen;

  int display_thread();
  void schedule_refresh(int delay);
  void video_refresh_timer();
  static Uint32 sdl_refresh_timer_cb(Uint32 interval, void *param);
  void video_display();
  double get_audio_clock();
};

#endif // VIDEO_RENDERER_H_
