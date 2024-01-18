
#ifndef VIDEO_RENDERER_H_
#define VIDEO_RENDERER_H_

#include "videostate.h"

namespace player
{

class VideoRenderer
{
public:
  explicit VideoRenderer() = default;
  ~VideoRenderer();

  int start(std::shared_ptr<VideoState> vs);
  void stop();

private:
  std::shared_ptr<VideoState> m_vs = nullptr;
  SDL_Window* m_screen = nullptr;
  SDL_Texture* m_texture = nullptr;
  SDL_Renderer* m_renderer = nullptr;
  
  int displayThread();
  void scheduleRefresh(int delay);
  void videoRefreshTimer();
  static Uint32 sdlRefreshTimerCb(Uint32 interval, void* param);
  void videoDisplay();
  double getAudioClock();
};

} // player

#endif // VIDEO_RENDERER_H_

