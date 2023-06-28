
#include "stdafx.h"
#include "win32messagehandler.h"
#include "dx11manager.h"

#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

using namespace message_handler;
using namespace manager;

int main(int argc, char* argv[])
{
  HRESULT hr = S_OK;

  // Set locale(use to the system default locale)
  std::wcout.imbue(std::locale(""));

  // Create main window.
  bool result = Win32MessageHandler::getInstance().init((HINSTANCE)0, 1);
  if (!result)
  {
    MessageBoxW(nullptr, L"Failed to create main window.", L"Error", MB_OK);
    return -1;
  }

  HWND previewWnd = Win32MessageHandler::getInstance().hwnd();
  uint32_t width = 0, height = 0, fps = 0;
  width = 1280;
  height = 720;
  fps = 60;
  // Create dx11 device, context, swapchain
  result = DX11Manager::getInstance().init(previewWnd, width, height, fps);
  if (!result)
  {
    MessageBoxW(nullptr, L"Failed to init dx11.", L"Error", MB_OK);
    return -1;
  }

  // Start message loop
  Win32MessageHandler::getInstance().run();

  return 0;
}
