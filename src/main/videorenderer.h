
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
  VideoState* m_videoState;
  SDL_Window* m_screen;

  int displayThread();
  void scheduleRefresh(int delay);
  void videoRefreshTimer();
  static Uint32 sdlRefreshTimerCB(Uint32 interval, void *param);
  void videoDisplay();
};

#endif // VIDEO_RENDERER_H_
