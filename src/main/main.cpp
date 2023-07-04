
#include "stdafx.h"
#include "win32messagehandler.h"
#include "dx11manager.h"
#include "ffmpegdecoder.h"

#include <chrono>
#include <thread>

#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

using namespace message_handler;
using namespace manager;

int main(int argc, char* argv[])
{
  // Set locale(use to the system default locale)
  std::wcout.imbue(std::locale(""));

  if (argc < 2)
  {
    std::cout << argv[0] << " <file name or url>" << std::endl;
    return -1;
  }

  // Create main window.
  bool result = Win32MessageHandler::getInstance().init((HINSTANCE)0, 1);
  if (!result)
  {
    MessageBoxW(nullptr, L"Failed to create main window.", L"Error", MB_OK);
    return -1;
  }

  // Create decoder
  FFMPEGDecoder* decoder = new FFMPEGDecoder();
  std::string url = argv[1];
  std::wstring wsURL = std::wstring(url.begin(), url.end());
  uint32_t width = 0, height = 0;
  if(decoder->openInputFile(wsURL, width, height) < 0)
  {
    MessageBoxW(nullptr, L"Failed to open file or url by ffmpeg decoder.", L"Error", MB_OK);
    delete decoder;
    decoder = nullptr;
    return -1;
  }

  HWND previewWnd = Win32MessageHandler::getInstance().hwnd();
  // Init window buffer size
  uint32_t fps = 0;
  fps = 60;
  // Create dx11 device, context, swapchain
  result = DX11Manager::getInstance().init(previewWnd, width, height, fps);
  if (!result)
  {
    MessageBoxW(nullptr, L"Failed to init dx11.", L"Error", MB_OK);
    delete decoder;
    decoder = nullptr;
    return -1;
  }

  // Start to decode thread
  std::thread([&](FFMPEGDecoder* decoder)
  {
    decoder->run();
  }, decoder).detach();

  // Start message loop
  Win32MessageHandler::getInstance().run();

  // decoder stop
  decoder->stop();

  // wait to finish the decoder thread
  std::chrono::milliseconds duration(1000);
  std::this_thread::sleep_for(duration);

  delete decoder;
  decoder = nullptr;

  return 0;
}
