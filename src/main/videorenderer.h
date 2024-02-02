
#ifndef VIDEO_RENDERER_H_
#define VIDEO_RENDERER_H_

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

#include "framecontainer.h"
#include <atomic>

namespace client
{

class VideoRenderer
{
public:
  explicit VideoRenderer();
  ~VideoRenderer();

  bool init(const int& x, const int& y, const int& w, const int& h, std::shared_ptr<FrameContainer> frameContainer);
  int start();
  void stop();

private:
  SDL_Renderer* m_renderer = nullptr;
  SDL_Texture* m_texture = nullptr;
  SDL_mutex* m_mutex = nullptr;
  SDL_Window* m_window = nullptr;

  std::shared_ptr<FrameContainer> m_frameContainer = nullptr;
  std::atomic_bool m_isStop = false;

  int displayThread();
  static Uint32 sdlRefreshTimerCb(Uint32 interval, void* param);
  void scheduleRefresh(int delay);
  void videoRefreshTimer();
  void videoDisplay(AVFrame* frame);
};

} // client

#endif // VIDEO_RENDERER_H_
