
#ifndef STRING_HELPER_H_
#define STRING_HELPER_H_

#include <string>
#include <windows.h>

namespace
{
  template <class T>
  std::string wstringToString(T ws)
  {
    // wstring to UTF8
    int iBufferSize = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, (char*)NULL, 0, NULL, NULL);
    char* cpMultiByte = new char[iBufferSize];
    // wstring to UTF8
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, cpMultiByte, iBufferSize, NULL, NULL);
    std::string oRet(cpMultiByte, cpMultiByte + iBufferSize - 1);
    delete[] cpMultiByte;
    return oRet;
  }

  template <class T>
  std::wstring UTF8ToUnicode(T str)
  {
    int unicodeLen = ::MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    wchar_t* pUnicode;
    pUnicode = new wchar_t[unicodeLen + 1];
    memset(pUnicode, 0, (unicodeLen + 1) * sizeof(wchar_t));
    ::MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, (LPWSTR)pUnicode, unicodeLen);
    std::wstring rt;
    rt = (wchar_t*)pUnicode;
    delete pUnicode;
    return rt;
  }
}


#endif // STRING_HELPER_H_
