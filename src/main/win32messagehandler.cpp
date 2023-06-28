
#include "stdafx.h"
#include "win32messagehandler.h"

using namespace message_handler;

Win32MessageHandler::Win32MessageHandler()
  : m_hwnd(nullptr)
{
}

Win32MessageHandler::~Win32MessageHandler()
{
}

Win32MessageHandler& Win32MessageHandler::getInstance()
{
  static Win32MessageHandler inst;
  return inst;
}

bool Win32MessageHandler::init(HINSTANCE hinst, int nCmdShow)
{
  // Parse the command line parameters
  // TODO

  const WCHAR className[100] = L"Capture Engine MainWindow Class";

  // Initialize the window class.
  WNDCLASSEXW windowClass = {0};
  windowClass.cbSize = sizeof(WNDCLASSEX);
  windowClass.style = CS_HREDRAW | CS_VREDRAW;
  windowClass.lpfnWndProc = this->MessageProcedure;
  windowClass.hInstance = hinst;
  windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
  windowClass.lpszClassName = className;
  RegisterClassExW(&windowClass);

  RECT windowRect = {0, 0, static_cast<LONG>(1920), static_cast<LONG>(1080)};
  AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

  // Create the window and store a handle to it.
  m_hwnd = CreateWindowExW(
    0
    , windowClass.lpszClassName
    , L"USB Video Capture"
    , WS_OVERLAPPEDWINDOW
    , CW_USEDEFAULT
    , CW_USEDEFAULT
    , windowRect.right - windowRect.left
    , windowRect.bottom - windowRect.top
    , nullptr // We have no parent window.
    , nullptr // We are not using menus.
    , hinst
    , nullptr
  );

  if (m_hwnd == nullptr)
  {
    return false;
  }

  ShowWindow(m_hwnd, nCmdShow);

  return true;
}

int Win32MessageHandler::run()
{
  // Initialize the window.
  //window->onInit();

  // Main loop.
  MSG msg = {};
  while (msg.message != WM_QUIT)
  {
    // Process any messages in the queue.
    if (GetMessage(&msg, NULL, 0, 0))
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  //window->onDestroy();

  return static_cast<char>(msg.wParam);
}

LRESULT WINAPI Win32MessageHandler::MessageProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
  {
  case WM_CREATE:
  {
  }
  return 0;

  case WM_KEYDOWN:
  {
  }
  return 0;

  case WM_KEYUP:
  {
  }
  return 0;

  case WM_PAINT:
  {
    PAINTSTRUCT ps{};
    HDC hdc = BeginPaint(hwnd, &ps);
    EndPaint(hwnd, &ps);
  }
  return 0;

  case WM_DESTROY:
  {
    PostQuitMessage(0);
  }
  return 0;

  }

  // Handle any messages the switch statement didn't.
  return DefWindowProc(hwnd, message, wParam, lParam);
}
