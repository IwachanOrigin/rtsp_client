
#ifndef UTILS_H_
#define UTILS_H_

#include "stdafx.h"
#include "dxhelper.h"

using Microsoft::WRL::ComPtr;

namespace helper
{

  // Gets an interface pointer from a Media Foundation collection.
template <class IFACE>
HRESULT utilGetCollectionObject(IMFCollection* collection, DWORD index, IFACE** ppObject)
{
  IUnknown* pUnk = nullptr;
  HRESULT hr = collection->GetElement(index, &pUnk);
  if (SUCCEEDED(hr))
  {
    hr = pUnk->QueryInterface(IID_PPV_ARGS(ppObject));
    pUnk->Release();
  }
  return hr;
}

static inline HRESULT utilCopyAttribute(IMFAttributes* srcAttribute, IMFAttributes* dstAttribute, const GUID& key)
{
  PROPVARIANT var;
  PropVariantInit(&var);
  HRESULT hr = srcAttribute->GetItem(key, &var);
  if (SUCCEEDED(hr))
  {
    hr = dstAttribute->SetItem(key, var);
    PropVariantClear(&var);
  }
  return hr;
}

// Create a compatible format all items
static inline HRESULT utilCloneAllItems(IMFMediaType* srcMediaType, IMFMediaType** ppNewMediaType)
{
  ComPtr<IMFMediaType> newMediaType = nullptr;

  HRESULT hr = MFCreateMediaType(newMediaType.GetAddressOf());
  if (FAILED(hr))
  {
    return hr;
  }

  hr = newMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  if (FAILED(hr))
  {
    return hr;
  }

  hr = utilCopyAttribute(srcMediaType, newMediaType.Get(), MF_MT_SUBTYPE);
  if (FAILED(hr))
  {
    return hr;
  }

  hr = utilCopyAttribute(srcMediaType, newMediaType.Get(), MF_MT_FRAME_SIZE);
  if (FAILED(hr))
  {
    return hr;
  }

  hr = utilCopyAttribute(srcMediaType, newMediaType.Get(), MF_MT_FRAME_RATE);
  if (FAILED(hr))
  {
    return hr;
  }

  hr = utilCopyAttribute(srcMediaType, newMediaType.Get(), MF_MT_PIXEL_ASPECT_RATIO);
  if (FAILED(hr))
  {
    return hr;
  }

  hr = utilCopyAttribute(srcMediaType, newMediaType.Get(), MF_MT_INTERLACE_MODE);
  if (FAILED(hr))
  {
    return hr;
  }

  *ppNewMediaType = newMediaType.Get();
  (*ppNewMediaType)->AddRef();

  return hr;
}

// Create a compatible video format with a different subtype.
static inline HRESULT utilCloneVideomediaType(IMFMediaType* srcMediaType, REFGUID guidSubType, IMFMediaType** ppNewMediaType)
{
  ComPtr<IMFMediaType> newMediaType = nullptr;

  HRESULT hr = MFCreateMediaType(newMediaType.GetAddressOf());
  if (FAILED(hr))
  {
    return hr;
  }

  hr = newMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  if (FAILED(hr))
  {
    return hr;
  }

  hr = newMediaType->SetGUID(MF_MT_SUBTYPE, guidSubType);
  if (FAILED(hr))
  {
    return hr;
  }

  hr = utilCopyAttribute(srcMediaType, newMediaType.Get(), MF_MT_FRAME_SIZE);
  if (FAILED(hr))
  {
    return hr;
  }

  hr = utilCopyAttribute(srcMediaType, newMediaType.Get(), MF_MT_FRAME_RATE);
  if (FAILED(hr))
  {
    return hr;
  }

  hr = utilCopyAttribute(srcMediaType, newMediaType.Get(), MF_MT_PIXEL_ASPECT_RATIO);
  if (FAILED(hr))
  {
    return hr;
  }

  hr = utilCopyAttribute(srcMediaType, newMediaType.Get(), MF_MT_INTERLACE_MODE);
  if (FAILED(hr))
  {
    return hr;
  }

  *ppNewMediaType = newMediaType.Get();
  (*ppNewMediaType)->AddRef();

  return hr;
}

// Create a compatible audio format with a different subtype.
static inline HRESULT utilCloneAudiomediaType(IMFMediaType* srcMediaType, REFGUID guidSubType, IMFMediaType** ppNewMediaType)
{
  ComPtr<IMFMediaType> newMediaType = nullptr;

  HRESULT hr = MFCreateMediaType(newMediaType.GetAddressOf());
  if (FAILED(hr))
  {
    return hr;
  }

  hr = newMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
  if (FAILED(hr))
  {
    return hr;
  }

  hr = newMediaType->SetGUID(MF_MT_SUBTYPE, guidSubType);
  if (FAILED(hr))
  {
    return hr;
  }

  hr = utilCopyAttribute(srcMediaType, newMediaType.Get(), MF_MT_AUDIO_NUM_CHANNELS);
  if (FAILED(hr))
  {
    return hr;
  }

  hr = utilCopyAttribute(srcMediaType, newMediaType.Get(), MF_MT_AUDIO_SAMPLES_PER_SECOND);
  if (FAILED(hr))
  {
    return hr;
  }

  hr = utilCopyAttribute(srcMediaType, newMediaType.Get(), MF_MT_AUDIO_BLOCK_ALIGNMENT);
  if (FAILED(hr))
  {
    return hr;
  }

  hr = utilCopyAttribute(srcMediaType, newMediaType.Get(), MF_MT_AUDIO_AVG_BYTES_PER_SECOND);
  if (FAILED(hr))
  {
    return hr;
  }

  hr = utilCopyAttribute(srcMediaType, newMediaType.Get(), MF_MT_AUDIO_BITS_PER_SAMPLE);
  if (FAILED(hr))
  {
    return hr;
  }

  *ppNewMediaType = newMediaType.Get();
  (*ppNewMediaType)->AddRef();

  return hr;
}


// Helper function to get the frame size from a video media type.
static inline HRESULT utilGetFrameSize(IMFMediaType* type, UINT32* width, UINT32* height)
{
  return MFGetAttributeSize(type, MF_MT_FRAME_SIZE, width, height);
}

// Helper function to get the frame rate from a video media type.
static inline HRESULT utilGetFrameRate(IMFMediaType* type, UINT32* numerator, UINT32* denominator)
{
  return MFGetAttributeRatio(type, MF_MT_FRAME_RATE, numerator, denominator);
}

static inline HRESULT utilGetEncodingBitrate(IMFMediaType* mediaType, UINT32* encodingBitrate)
{
  UINT32 width = 0;
  UINT32 height = 0;
  float bitrate = 0.0f;
  UINT32 framerateNum = 0;
  UINT32 framerateDenom = 0;

  HRESULT hr = utilGetFrameSize(mediaType, &width, &height);
  if (FAILED(hr))
  {
    return hr;
  }

  hr = utilGetFrameRate(mediaType, &framerateNum, &framerateDenom);
  if (FAILED(hr))
  {
    return hr;
  }

  bitrate = width / 3.0f * height * framerateNum / framerateDenom;
  *encodingBitrate = static_cast<UINT32>(bitrate);

  return hr;
}

#ifndef IF_EQUAL_RETURN
#define IF_EQUAL_RETURN(param, val) if(val == param) return #val
#endif

static inline LPCSTR getGUIDNameConst(const GUID& guid)
{
  IF_EQUAL_RETURN(guid, MF_MT_MAJOR_TYPE);
  IF_EQUAL_RETURN(guid, MF_MT_MAJOR_TYPE);
  IF_EQUAL_RETURN(guid, MF_MT_SUBTYPE);
  IF_EQUAL_RETURN(guid, MF_MT_ALL_SAMPLES_INDEPENDENT);
  IF_EQUAL_RETURN(guid, MF_MT_FIXED_SIZE_SAMPLES);
  IF_EQUAL_RETURN(guid, MF_MT_COMPRESSED);
  IF_EQUAL_RETURN(guid, MF_MT_SAMPLE_SIZE);
  IF_EQUAL_RETURN(guid, MF_MT_WRAPPED_TYPE);
  IF_EQUAL_RETURN(guid, MF_MT_AUDIO_NUM_CHANNELS);
  IF_EQUAL_RETURN(guid, MF_MT_AUDIO_SAMPLES_PER_SECOND);
  IF_EQUAL_RETURN(guid, MF_MT_AUDIO_FLOAT_SAMPLES_PER_SECOND);
  IF_EQUAL_RETURN(guid, MF_MT_AUDIO_AVG_BYTES_PER_SECOND);
  IF_EQUAL_RETURN(guid, MF_MT_AUDIO_BLOCK_ALIGNMENT);
  IF_EQUAL_RETURN(guid, MF_MT_AUDIO_BITS_PER_SAMPLE);
  IF_EQUAL_RETURN(guid, MF_MT_AUDIO_VALID_BITS_PER_SAMPLE);
  IF_EQUAL_RETURN(guid, MF_MT_AUDIO_SAMPLES_PER_BLOCK);
  IF_EQUAL_RETURN(guid, MF_MT_AUDIO_CHANNEL_MASK);
  IF_EQUAL_RETURN(guid, MF_MT_AUDIO_FOLDDOWN_MATRIX);
  IF_EQUAL_RETURN(guid, MF_MT_AUDIO_WMADRC_PEAKREF);
  IF_EQUAL_RETURN(guid, MF_MT_AUDIO_WMADRC_PEAKTARGET);
  IF_EQUAL_RETURN(guid, MF_MT_AUDIO_WMADRC_AVGREF);
  IF_EQUAL_RETURN(guid, MF_MT_AUDIO_WMADRC_AVGTARGET);
  IF_EQUAL_RETURN(guid, MF_MT_AUDIO_PREFER_WAVEFORMATEX);
  IF_EQUAL_RETURN(guid, MF_MT_AAC_PAYLOAD_TYPE);
  IF_EQUAL_RETURN(guid, MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION);
  IF_EQUAL_RETURN(guid, MF_MT_FRAME_SIZE);
  IF_EQUAL_RETURN(guid, MF_MT_FRAME_RATE);
  IF_EQUAL_RETURN(guid, MF_MT_FRAME_RATE_RANGE_MAX);
  IF_EQUAL_RETURN(guid, MF_MT_FRAME_RATE_RANGE_MIN);
  IF_EQUAL_RETURN(guid, MF_MT_PIXEL_ASPECT_RATIO);
  IF_EQUAL_RETURN(guid, MF_MT_DRM_FLAGS);
  IF_EQUAL_RETURN(guid, MF_MT_PAD_CONTROL_FLAGS);
  IF_EQUAL_RETURN(guid, MF_MT_SOURCE_CONTENT_HINT);
  IF_EQUAL_RETURN(guid, MF_MT_VIDEO_CHROMA_SITING);
  IF_EQUAL_RETURN(guid, MF_MT_INTERLACE_MODE);
  IF_EQUAL_RETURN(guid, MF_MT_TRANSFER_FUNCTION);
  IF_EQUAL_RETURN(guid, MF_MT_VIDEO_PRIMARIES);
  IF_EQUAL_RETURN(guid, MF_MT_CUSTOM_VIDEO_PRIMARIES);
  IF_EQUAL_RETURN(guid, MF_MT_YUV_MATRIX);
  IF_EQUAL_RETURN(guid, MF_MT_VIDEO_LIGHTING);
  IF_EQUAL_RETURN(guid, MF_MT_VIDEO_NOMINAL_RANGE);
  IF_EQUAL_RETURN(guid, MF_MT_GEOMETRIC_APERTURE);
  IF_EQUAL_RETURN(guid, MF_MT_MINIMUM_DISPLAY_APERTURE);
  IF_EQUAL_RETURN(guid, MF_MT_PAN_SCAN_APERTURE);
  IF_EQUAL_RETURN(guid, MF_MT_PAN_SCAN_ENABLED);
  IF_EQUAL_RETURN(guid, MF_MT_AVG_BITRATE);
  IF_EQUAL_RETURN(guid, MF_MT_AVG_BIT_ERROR_RATE);
  IF_EQUAL_RETURN(guid, MF_MT_MAX_KEYFRAME_SPACING);
  IF_EQUAL_RETURN(guid, MF_MT_DEFAULT_STRIDE);
  IF_EQUAL_RETURN(guid, MF_MT_PALETTE);
  IF_EQUAL_RETURN(guid, MF_MT_USER_DATA);
  IF_EQUAL_RETURN(guid, MF_MT_AM_FORMAT_TYPE);
  IF_EQUAL_RETURN(guid, MF_MT_MPEG_START_TIME_CODE);
  IF_EQUAL_RETURN(guid, MF_MT_MPEG2_PROFILE);
  IF_EQUAL_RETURN(guid, MF_MT_MPEG2_LEVEL);
  IF_EQUAL_RETURN(guid, MF_MT_MPEG2_FLAGS);
  IF_EQUAL_RETURN(guid, MF_MT_MPEG_SEQUENCE_HEADER);
  IF_EQUAL_RETURN(guid, MF_MT_DV_AAUX_SRC_PACK_0);
  IF_EQUAL_RETURN(guid, MF_MT_DV_AAUX_CTRL_PACK_0);
  IF_EQUAL_RETURN(guid, MF_MT_DV_AAUX_SRC_PACK_1);
  IF_EQUAL_RETURN(guid, MF_MT_DV_AAUX_CTRL_PACK_1);
  IF_EQUAL_RETURN(guid, MF_MT_DV_VAUX_SRC_PACK);
  IF_EQUAL_RETURN(guid, MF_MT_DV_VAUX_CTRL_PACK);
  IF_EQUAL_RETURN(guid, MF_MT_ARBITRARY_HEADER);
  IF_EQUAL_RETURN(guid, MF_MT_ARBITRARY_FORMAT);
  IF_EQUAL_RETURN(guid, MF_MT_IMAGE_LOSS_TOLERANT);
  IF_EQUAL_RETURN(guid, MF_MT_MPEG4_SAMPLE_DESCRIPTION);
  IF_EQUAL_RETURN(guid, MF_MT_MPEG4_CURRENT_SAMPLE_ENTRY);
  IF_EQUAL_RETURN(guid, MF_MT_ORIGINAL_4CC);
  IF_EQUAL_RETURN(guid, MF_MT_ORIGINAL_WAVE_FORMAT_TAG);

  // Media types

  IF_EQUAL_RETURN(guid, MFMediaType_Audio);
  IF_EQUAL_RETURN(guid, MFMediaType_Video);
  IF_EQUAL_RETURN(guid, MFMediaType_Protected);
  IF_EQUAL_RETURN(guid, MFMediaType_SAMI);
  IF_EQUAL_RETURN(guid, MFMediaType_Script);
  IF_EQUAL_RETURN(guid, MFMediaType_Image);
  IF_EQUAL_RETURN(guid, MFMediaType_HTML);
  IF_EQUAL_RETURN(guid, MFMediaType_Binary);
  IF_EQUAL_RETURN(guid, MFMediaType_FileTransfer);

  IF_EQUAL_RETURN(guid, MFVideoFormat_AI44); //     FCC('AI44')
  IF_EQUAL_RETURN(guid, MFVideoFormat_ARGB32); //   D3DFMT_A8R8G8B8 
  IF_EQUAL_RETURN(guid, MFVideoFormat_AYUV); //     FCC('AYUV')
  IF_EQUAL_RETURN(guid, MFVideoFormat_DV25); //     FCC('dv25')
  IF_EQUAL_RETURN(guid, MFVideoFormat_DV50); //     FCC('dv50')
  IF_EQUAL_RETURN(guid, MFVideoFormat_DVH1); //     FCC('dvh1')
  IF_EQUAL_RETURN(guid, MFVideoFormat_DVSD); //     FCC('dvsd')
  IF_EQUAL_RETURN(guid, MFVideoFormat_DVSL); //     FCC('dvsl')
  IF_EQUAL_RETURN(guid, MFVideoFormat_H264); //     FCC('H264')
  IF_EQUAL_RETURN(guid, MFVideoFormat_I420); //     FCC('I420')
  IF_EQUAL_RETURN(guid, MFVideoFormat_IYUV); //     FCC('IYUV')
  IF_EQUAL_RETURN(guid, MFVideoFormat_M4S2); //     FCC('M4S2')
  IF_EQUAL_RETURN(guid, MFVideoFormat_MJPG);
  IF_EQUAL_RETURN(guid, MFVideoFormat_MP43); //     FCC('MP43')
  IF_EQUAL_RETURN(guid, MFVideoFormat_MP4S); //     FCC('MP4S')
  IF_EQUAL_RETURN(guid, MFVideoFormat_MP4V); //     FCC('MP4V')
  IF_EQUAL_RETURN(guid, MFVideoFormat_MPG1); //     FCC('MPG1')
  IF_EQUAL_RETURN(guid, MFVideoFormat_MSS1); //     FCC('MSS1')
  IF_EQUAL_RETURN(guid, MFVideoFormat_MSS2); //     FCC('MSS2')
  IF_EQUAL_RETURN(guid, MFVideoFormat_NV11); //     FCC('NV11')
  IF_EQUAL_RETURN(guid, MFVideoFormat_NV12); //     FCC('NV12')
  IF_EQUAL_RETURN(guid, MFVideoFormat_P010); //     FCC('P010')
  IF_EQUAL_RETURN(guid, MFVideoFormat_P016); //     FCC('P016')
  IF_EQUAL_RETURN(guid, MFVideoFormat_P210); //     FCC('P210')
  IF_EQUAL_RETURN(guid, MFVideoFormat_P216); //     FCC('P216')
  IF_EQUAL_RETURN(guid, MFVideoFormat_RGB24); //    D3DFMT_R8G8B8 
  IF_EQUAL_RETURN(guid, MFVideoFormat_RGB32); //    D3DFMT_X8R8G8B8 
  IF_EQUAL_RETURN(guid, MFVideoFormat_RGB555); //   D3DFMT_X1R5G5B5 
  IF_EQUAL_RETURN(guid, MFVideoFormat_RGB565); //   D3DFMT_R5G6B5 
  IF_EQUAL_RETURN(guid, MFVideoFormat_RGB8);
  IF_EQUAL_RETURN(guid, MFVideoFormat_UYVY); //     FCC('UYVY')
  IF_EQUAL_RETURN(guid, MFVideoFormat_v210); //     FCC('v210')
  IF_EQUAL_RETURN(guid, MFVideoFormat_v410); //     FCC('v410')
  IF_EQUAL_RETURN(guid, MFVideoFormat_WMV1); //     FCC('WMV1')
  IF_EQUAL_RETURN(guid, MFVideoFormat_WMV2); //     FCC('WMV2')
  IF_EQUAL_RETURN(guid, MFVideoFormat_WMV3); //     FCC('WMV3')
  IF_EQUAL_RETURN(guid, MFVideoFormat_WVC1); //     FCC('WVC1')
  IF_EQUAL_RETURN(guid, MFVideoFormat_Y210); //     FCC('Y210')
  IF_EQUAL_RETURN(guid, MFVideoFormat_Y216); //     FCC('Y216')
  IF_EQUAL_RETURN(guid, MFVideoFormat_Y410); //     FCC('Y410')
  IF_EQUAL_RETURN(guid, MFVideoFormat_Y416); //     FCC('Y416')
  IF_EQUAL_RETURN(guid, MFVideoFormat_Y41P);
  IF_EQUAL_RETURN(guid, MFVideoFormat_Y41T);
  IF_EQUAL_RETURN(guid, MFVideoFormat_YUY2); //     FCC('YUY2')
  IF_EQUAL_RETURN(guid, MFVideoFormat_YV12); //     FCC('YV12')
  IF_EQUAL_RETURN(guid, MFVideoFormat_YVYU);

  IF_EQUAL_RETURN(guid, MFAudioFormat_PCM); //              WAVE_FORMAT_PCM 
  IF_EQUAL_RETURN(guid, MFAudioFormat_Float); //            WAVE_FORMAT_IEEE_FLOAT 
  IF_EQUAL_RETURN(guid, MFAudioFormat_DTS); //              WAVE_FORMAT_DTS 
  IF_EQUAL_RETURN(guid, MFAudioFormat_Dolby_AC3_SPDIF); //  WAVE_FORMAT_DOLBY_AC3_SPDIF 
  IF_EQUAL_RETURN(guid, MFAudioFormat_DRM); //              WAVE_FORMAT_DRM 
  IF_EQUAL_RETURN(guid, MFAudioFormat_WMAudioV8); //        WAVE_FORMAT_WMAUDIO2 
  IF_EQUAL_RETURN(guid, MFAudioFormat_WMAudioV9); //        WAVE_FORMAT_WMAUDIO3 
  IF_EQUAL_RETURN(guid, MFAudioFormat_WMAudio_Lossless); // WAVE_FORMAT_WMAUDIO_LOSSLESS 
  IF_EQUAL_RETURN(guid, MFAudioFormat_WMASPDIF); //         WAVE_FORMAT_WMASPDIF 
  IF_EQUAL_RETURN(guid, MFAudioFormat_MSP1); //             WAVE_FORMAT_WMAVOICE9 
  IF_EQUAL_RETURN(guid, MFAudioFormat_MP3); //              WAVE_FORMAT_MPEGLAYER3 
  IF_EQUAL_RETURN(guid, MFAudioFormat_MPEG); //             WAVE_FORMAT_MPEG 
  IF_EQUAL_RETURN(guid, MFAudioFormat_AAC); //              WAVE_FORMAT_MPEG_HEAAC 
  IF_EQUAL_RETURN(guid, MFAudioFormat_ADTS); //             WAVE_FORMAT_MPEG_ADTS_AAC 

  return NULL;
}

static inline HRESULT CreateSingleBufferIMFSample(DWORD bufferSize, IMFSample** pSample)
{
  IMFMediaBuffer* pBuffer = NULL;

  HRESULT hr = S_OK;

  hr = MFCreateSample(pSample);
  if (FAILED(hr))
  {
    std::wcerr << "Failed to create MF sample." << std::endl;
    goto done;
  }

  // Adds a ref count to the pBuffer object.
  hr = MFCreateMemoryBuffer(bufferSize, &pBuffer);
  if (FAILED(hr))
  {
    std::wcerr << "Failed to create memory buffer." << std::endl;
    goto done;
  }

  // Adds another ref count to the pBuffer object.
  hr = (*pSample)->AddBuffer(pBuffer);
  if (FAILED(hr))
  {
    std::wcerr << "Failed to add sample to buffer." << std::endl;
    goto done;
  }

done:
  // Leave the single ref count that will be removed when the pSample is released.
  SAFE_RELEASE(pBuffer);
  return hr;
}

static inline HRESULT CreateAndCopySingleBufferIMFSample(IMFSample* pSrcSample, IMFSample** pDstSample)
{
  IMFMediaBuffer* pDstBuffer = NULL;
  DWORD srcBufLength;

  HRESULT hr = S_OK;

  // Gets total length of ALL media buffer samples. We can use here because it's only a
  // single buffer sample copy.
  hr = pSrcSample->GetTotalLength(&srcBufLength);
  if (FAILED(hr))
  {
    std::wcerr << "Failed to get total length from source buffer." << std::endl;
    goto done;
  }

  hr = CreateSingleBufferIMFSample(srcBufLength, pDstSample);
  if (FAILED(hr))
  {
    std::wcerr << "Failed to create new single buffer IMF sample." << std::endl;
    goto done;
  }

  hr = pSrcSample->CopyAllItems(*pDstSample);
  if (FAILED(hr))
  {
    std::wcerr << "Failed to copy IMFSample items from src to dst." << std::endl;
    goto done;
  }

  hr = (*pDstSample)->GetBufferByIndex(0, &pDstBuffer);
  if (FAILED(hr))
  {
    std::wcerr << "Failed to get buffer from sample." << std::endl;
    goto done;
  }

  hr = pSrcSample->CopyToBuffer(pDstBuffer);
  if (FAILED(hr))
  {
    std::wcerr << "Failed to copy IMF media buffer." << std::endl;
    goto done;
  }

done:
  SAFE_RELEASE(pDstBuffer);
  return hr;
}

static inline std::string GetMediaTypeDescription(IMFMediaType* pMediaType)
{
  HRESULT hr = S_OK;
  GUID MajorType{ 0 };
  UINT32 cAttrCount = 0;
  LPCSTR pszGuidStr{ 0 };
  std::string description = "";
  WCHAR TempBuf[200]{ 0 };

  if (pMediaType == NULL)
  {
    description = "<NULL>";
    goto done;
  }

  hr = pMediaType->GetMajorType(&MajorType);
  if (FAILED(hr))
  {
    goto done;
  }

  //pszGuidStr = STRING_FROM_GUID(MajorType);
  pszGuidStr = getGUIDNameConst(MajorType);
  if (pszGuidStr != NULL)
  {
    description += pszGuidStr;
    description += ": ";
  }
  else
  {
    description += "Other: ";
  }

  hr = pMediaType->GetCount(&cAttrCount);
  if (FAILED(hr))
  {
    goto done;
  }

  for (UINT32 i = 0; i < cAttrCount; i++)
  {
    GUID guidId;
    MF_ATTRIBUTE_TYPE attrType;

    hr = pMediaType->GetItemByIndex(i, &guidId, NULL);
    if (FAILED(hr))
    {
      goto done;
    }

    hr = pMediaType->GetItemType(guidId, &attrType);
    if (FAILED(hr))
    {
      goto done;
    }

    //pszGuidStr = STRING_FROM_GUID(guidId);
    pszGuidStr = getGUIDNameConst(guidId);
    if (pszGuidStr != NULL)
    {
      description += pszGuidStr;
    }
    else
    {
      LPOLESTR guidStr = NULL;

      hr = StringFromCLSID(guidId, &guidStr);
      if (FAILED(hr))
      {
        goto done;
      }
      auto wGuidStr = std::wstring(guidStr);
      description += std::string(wGuidStr.begin(), wGuidStr.end()); // GUID's won't have wide chars.

      CoTaskMemFree(guidStr);
    }

    description += "=";

    switch (attrType)
    {
    case MF_ATTRIBUTE_UINT32:
    {
      UINT32 Val;
      hr = pMediaType->GetUINT32(guidId, &Val);
      if (FAILED(hr))
      {
        goto done;
      }

      description += std::to_string(Val);
      break;
    }
    case MF_ATTRIBUTE_UINT64:
    {
      UINT64 Val;
      hr = pMediaType->GetUINT64(guidId, &Val);
      if (FAILED(hr))
      {
        goto done;
      }

      if (guidId == MF_MT_FRAME_SIZE)
      {
        description += "W:" + std::to_string(HI32(Val)) + " H: " + std::to_string(LO32(Val));
      }
      else if (guidId == MF_MT_FRAME_RATE)
      {
        // Frame rate is numerator/denominator.
        description += std::to_string(HI32(Val)) + "/" + std::to_string(LO32(Val));
      }
      else if (guidId == MF_MT_PIXEL_ASPECT_RATIO)
      {
        description += std::to_string(HI32(Val)) + ":" + std::to_string(LO32(Val));
      }
      else
      {
        //tempStr.Format("%ld", Val);
        description += std::to_string(Val);
      }

      //description += tempStr;

      break;
    }
    case MF_ATTRIBUTE_DOUBLE:
    {
      DOUBLE Val;
      hr = pMediaType->GetDouble(guidId, &Val);
      if (FAILED(hr))
      {
        goto done;
      }

      //tempStr.Format("%f", Val);
      description += std::to_string(Val);
      break;
    }
    case MF_ATTRIBUTE_GUID:
    {
      GUID Val;
      const char* pValStr;

      hr = pMediaType->GetGUID(guidId, &Val);
      if (FAILED(hr))
      {
        goto done;
      }

      //pValStr = STRING_FROM_GUID(Val);
      pValStr = getGUIDNameConst(Val);
      if (pValStr != NULL)
      {
        description += pValStr;
      }
      else
      {
        LPOLESTR guidStr = NULL;
        hr = StringFromCLSID(Val, &guidStr);
        if (FAILED(hr))
        {
          goto done;
        }
        auto wGuidStr = std::wstring(guidStr);
        description += std::string(wGuidStr.begin(), wGuidStr.end()); // GUID's won't have wide chars.

        CoTaskMemFree(guidStr);
      }

      break;
    }
    case MF_ATTRIBUTE_STRING:
    {
      hr = pMediaType->GetString(guidId, TempBuf, sizeof(TempBuf) / sizeof(TempBuf[0]), NULL);
      if (hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER))
      {
        description += "<Too Long>";
        break;
      }
      if (FAILED(hr))
      {
        goto done;
      }
      auto wstr = std::wstring(TempBuf);
      description += std::string(wstr.begin(), wstr.end()); // It's unlikely the attribute descriptions will contain multi byte chars.

      break;
    }
    case MF_ATTRIBUTE_BLOB:
    {
      description += "<BLOB>";
      break;
    }
    case MF_ATTRIBUTE_IUNKNOWN:
    {
      description += "<UNK>";
      break;
    }
    }

    description += ", ";
  }

done:

  return description;
}


static inline HRESULT GetTransformOutput(IMFTransform* pTransform, IMFSample** pOutSample, BOOL* transformFlushed)
{
  MFT_OUTPUT_STREAM_INFO StreamInfo = { 0 };
  MFT_OUTPUT_DATA_BUFFER outputDataBuffer = { 0 };
  DWORD processOutputStatus = 0;
  ComPtr<IMFMediaType> pChangedOutMediaType = nullptr;

  HRESULT hr = S_OK;
  *transformFlushed = FALSE;

  hr = pTransform->GetOutputStreamInfo(0, &StreamInfo);
  if (FAILED(hr))
  {
    std::cerr << "Failed to get output stream info from MFT." << std::endl;
    return hr;
  }

  outputDataBuffer.dwStreamID = 0;
  outputDataBuffer.dwStatus = 0;
  outputDataBuffer.pEvents = NULL;

  if ((StreamInfo.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) == 0)
  {
    hr = CreateSingleBufferIMFSample(StreamInfo.cbSize, pOutSample);
    if (FAILED(hr))
    {
      std::cerr << "Failed to create new single buffer IMF sample." << std::endl;
      return hr;
    }
    outputDataBuffer.pSample = *pOutSample;
  }

  auto mftProcessOutput = pTransform->ProcessOutput(0, 1, &outputDataBuffer, &processOutputStatus);

  printf("Process output result %.2X, MFT status %.2X.\n", mftProcessOutput, processOutputStatus);

  if (mftProcessOutput == S_OK)
  {
    // Sample is ready and allocated on the transform output buffer.
    *pOutSample = outputDataBuffer.pSample;
  }
  else if (mftProcessOutput == MF_E_TRANSFORM_STREAM_CHANGE)
  {
    // Format of the input stream has changed. https://docs.microsoft.com/en-us/windows/win32/medfound/handling-stream-changes
    if (outputDataBuffer.dwStatus == MFT_OUTPUT_DATA_BUFFER_FORMAT_CHANGE)
    {
      printf("MFT stream changed.\n");

      hr = pTransform->GetOutputAvailableType(0, 0, pChangedOutMediaType.GetAddressOf());
      if (FAILED(hr))
      {
        std::cerr << "Failed to get the MFT output media type after a stream change." << std::endl;
        return hr;
      }

      std::cout << "MFT output media type: " << GetMediaTypeDescription(pChangedOutMediaType.Get()) << std::endl << std::endl;

      hr = pChangedOutMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_IYUV);
      if (FAILED(hr))
      {
        std::cerr << "Failed to set media sub type." << std::endl;
        return hr;
      }

      hr = pTransform->SetOutputType(0, pChangedOutMediaType.Get(), 0);
      if (FAILED(hr))
      {
        std::cerr << "Failed to set new output media type on MFT." << std::endl;
        return hr;
      }

      hr = pTransform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, NULL);
      if (FAILED(hr))
      {
        std::cerr << "Failed to process FLUSH command on MFT." << std::endl;
        return hr;
      }

      *transformFlushed = TRUE;
    }
    else
    {
      printf("MFT stream changed but didn't have the data format change flag set. Don't know what to do.\n");
      hr = E_NOTIMPL;
    }
  }
  else if (mftProcessOutput == MF_E_TRANSFORM_NEED_MORE_INPUT)
  {
    // More input is not an error condition but it means the allocated output sample is empty.
    hr = MF_E_TRANSFORM_NEED_MORE_INPUT;
  }
  else
  {
    printf("MFT ProcessOutput error result %.2X, MFT status %.2X.\n", mftProcessOutput, processOutputStatus);
    hr = mftProcessOutput;
  }

  return hr;
}



} // helper

#endif // UTILS_H_
