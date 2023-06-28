
#include "stdafx.h"
#include "dxhelper.h"
#include "win32messagehandler.h"
#include "videocapturemanager.h"
#include "audiocapturemanager.h"
#include "audiooutputdevicemanager.h"
#include "dx11manager.h"

#pragma comment(lib, "mf")
#pragma comment(lib, "mfplat")
#pragma comment(lib, "mfuuid")
#pragma comment(lib, "mfreadwrite")

#pragma comment(lib, "shlwapi")
#pragma comment(lib, "wmcodecdspuuid.lib")

#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

#pragma comment(lib, "SDL2")

#undef main // for SDL2

using namespace helper;
using namespace message_handler;
using namespace manager;

static inline bool getCaptureDevices(uint32_t& deviceCount, IMFActivate**& devices, bool audioMode = false)
{
  GUID searchGUID = MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID;
  if (audioMode)
  {
    searchGUID = MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID;
  }

  std::shared_ptr<IMFAttributes> pAttributes;
  IMFAttributes* pRawAttributes = nullptr;
  HRESULT hr = MFCreateAttributes(&pRawAttributes, 1);
  if (FAILED(hr))
  {
    std::wcout << "Failed to set create attributes." << std::endl;
    return false;
  }
  pAttributes = std::shared_ptr<IMFAttributes>(pRawAttributes, [](auto* p) { p->Release(); });

  hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, searchGUID);
  if (FAILED(hr))
  {
    std::wcout << "Failed to set attribute to MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE." << std::endl;
    return false;
  }

  hr = MFEnumDeviceSources(pAttributes.get(), &devices, &deviceCount);
  if (FAILED(hr))
  {
    std::wcout << "Failed to initialize the media foundation." << std::endl;
    return false;
  }

  return true;
}

static inline void releaseAllDevices(IMFActivate**& devices, uint32_t& deviceCount)
{
  for (uint32_t i = 0; i < deviceCount; i++)
  {
    devices[i]->Release();
  }
  CoTaskMemFree(devices);
}

int main(int argc, char* argv[])
{
  HRESULT hr = S_OK;

  // Set locale(use to the system default locale)
  std::wcout.imbue(std::locale(""));

  // INIT
  ThrowIfFailed(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE));
  ThrowIfFailed(MFStartup(MF_VERSION));

  // Get video capture devices.
  uint32_t videoDeviceCount = 0;
  IMFActivate** videoDevices = nullptr;
  std::wcout << "----- Video Capture Devices -----" << std::endl;
  if (getCaptureDevices(videoDeviceCount, videoDevices))
  {
    for (uint32_t i = 0; i < videoDeviceCount; i++)
    {
      wchar_t* buffer = nullptr;
      uint32_t length = 0;
      hr = videoDevices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &buffer, &length);
      if (FAILED(hr))
      {
        std::wcerr << "Failed to get video capture device name." << std::endl;
        continue;
      }
      std::wcout << "No. " << i << " : " << buffer << std::endl;
    }
  }

  if (videoDeviceCount == 0)
  {
    std::wcerr << "Video capture device not found." << std::endl;
    CoTaskMemFree(videoDevices);
    ThrowIfFailed(MFShutdown());
    CoUninitialize();
    return -1;
  }

  // Input video capture device no.
  uint32_t videoSelectionNo = 0;

  std::wcout << "Please input video capture device no : ";
  std::wcin >> videoSelectionNo;
  if (videoSelectionNo > videoDeviceCount)
  {
    std::wcout << "Failed video capture device select.";
    return -1;
  }

  // Get audio capture devices.
  uint32_t audioDeviceCount = 0;
  IMFActivate** audioDevices = nullptr;
  std::wcout << "----- Audio Capture Devices -----" << std::endl;
  if (getCaptureDevices(audioDeviceCount, audioDevices, true))
  {
    for (uint32_t i = 0; i < audioDeviceCount; i++)
    {
      wchar_t* buffer = nullptr;
      uint32_t length = 0;
      hr = audioDevices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &buffer, &length);
      if (FAILED(hr))
      {
        std::wcerr << "Failed to get audio capture device name." << std::endl;
        continue;
      }
      std::wcout << "No. " << i << " : " << buffer << std::endl;
    }
  }

  if (audioDeviceCount == 0)
  {
    std::wcerr << "Audio capture device not found." << std::endl;
    if (!videoDevices)
    {
      releaseAllDevices(videoDevices, videoDeviceCount);
    }
    CoTaskMemFree(audioDevices);
    ThrowIfFailed(MFShutdown());
    CoUninitialize();
    return -1;
  }

  // Input audio capture device no.
  uint32_t audioSelectionNo = 0;
  std::wcout << "Please input audio capture device no : ";
  std::wcin >> audioSelectionNo;
  if (audioSelectionNo > audioDeviceCount)
  {
    std::wcerr << "Failed audio capture device select." << std::endl;
    return -1;
  }

  // Get audio output devices.
  std::vector<std::wstring> vecAudioOutDevNames;
  std::wcout << "----- Audio Output Devices -----" << std::endl;
  AudioOutputDeviceManager::getInstance().getAudioDeviceList(vecAudioOutDevNames);
  if (vecAudioOutDevNames.empty())
  {
    std::wcerr << "Failed to get audio output devices." << std::endl;
    return -1;
  }
  for (uint32_t i = 0; i < vecAudioOutDevNames.size(); i++)
  {
    std::wcout << "No. " << i << " : " << vecAudioOutDevNames[i] << std::endl;
  }

  // Input audio output device no.
  uint32_t audioOutputSelectionNo = 0;
  std::wcout << "Please input audio output device no : ";
  std::wcin >> audioOutputSelectionNo;
  if (audioOutputSelectionNo > vecAudioOutDevNames.size())
  {
    std::wcerr << "Failed audio output device select." << std::endl;
    return -1;
  }

  // Create main window.
  bool result = Win32MessageHandler::getInstance().init((HINSTANCE)0, 1);
  if (!result)
  {
    if (videoDevices != nullptr)
    {
      releaseAllDevices(videoDevices, videoDeviceCount);
    }
    ThrowIfFailed(MFShutdown());
    CoUninitialize();

    MessageBoxW(nullptr, L"Failed to create main window.", L"Error", MB_OK);
    return -1;
  }

  // Create video capturemanager.
  int retInt = VideoCaptureManager::getInstance().init(videoDevices[videoSelectionNo]);
  if (retInt < 0)
  {
    if (videoDevices != nullptr)
    {
      releaseAllDevices(videoDevices, videoDeviceCount);
    }
    ThrowIfFailed(MFShutdown());
    CoUninitialize();

    MessageBoxW(nullptr, L"Failed to create main window.", L"Error", MB_OK);
    return -1;
  }
  videoDevices[videoSelectionNo]->AddRef();

  HWND previewWnd = Win32MessageHandler::getInstance().hwnd();
  uint32_t width = 0, height = 0, fps = 0;
  width = VideoCaptureManager::getInstance().getCaptureWidth();
  height = VideoCaptureManager::getInstance().getCaptureHeight();
  fps = VideoCaptureManager::getInstance().getCaptureFps();
  // Create dx11 device, context, swapchain
  result = DX11Manager::getInstance().init(previewWnd, width, height, fps);
  if (!result)
  {
    if (videoDevices != nullptr)
    {
      releaseAllDevices(videoDevices, videoDeviceCount);
    }

    if (audioDevices != nullptr)
    {
      releaseAllDevices(audioDevices, audioDeviceCount);
    }

    ThrowIfFailed(MFShutdown());
    CoUninitialize();
  }

  // Create audio capturemanager
  retInt = AudioCaptureManager::getInstance().init(audioDevices[audioSelectionNo]);
  if (retInt < 0)
  {
    if (audioDevices != nullptr)
    {
      releaseAllDevices(audioDevices, audioDeviceCount);
    }

    MessageBoxW(nullptr, L"Failed to create audio capture manager.", L"Error", MB_OK);
  }
  else
  {
    // Setup audio device
    int errCode = AudioOutputDeviceManager::getInstance().init(audioOutputSelectionNo);
    if (errCode == 0)
    {
      errCode = AudioOutputDeviceManager::getInstance().start();
      if (errCode != 0)
      {
        std::wcout << "Failed to start audio device." << std::endl;
      }
    }
    else
    {
      std::wcout << "Failed to initialize the audio device." << std::endl;
    }
    audioDevices[audioSelectionNo]->AddRef();
  }

  // Start message loop
  Win32MessageHandler::getInstance().run();

  // Release
  if (videoDevices != nullptr)
  {
    releaseAllDevices(videoDevices, videoDeviceCount);
  }

  if (audioDevices != nullptr)
  {
    releaseAllDevices(audioDevices, audioDeviceCount);
  }
  ThrowIfFailed(MFShutdown());
  CoUninitialize();

  return 0;
}
