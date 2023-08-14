
#ifndef STRING_HELPER_H_
#define STRING_HELPER_H_

#ifdef _WIN32
// Win
#include <Windows.h>
#include <string>
#else
// Linux
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <errno.h>
#endif

namespace
{
#ifdef _WIN32
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
    delete[] pUnicode;
    return rt;
  }
#else
  template <class T>
  std::string wstringToString(T ws)
  {
    std::mbstate_t state = std::mbstate_t();
    auto temp = ws.c_str();
    size_t len = std::wcsrtombs(NULL, &temp, 0, &state);
    char* cpMultiByte = new char[len + 1];
    std::memset(cpMultiByte, 0, (len + 1) * sizeof(char));
    std::wcsrtombs(cpMultiByte, &temp, len, &state);
    std::string oRet;
    oRet = (char*)cpMultiByte;
    delete[] cpMultiByte;
    return oRet;
  }

  template <class T>
  std::wstring UTF8ToUnicode(T str)
  {
    std::mbstate_t state = std::mbstate_t();
    auto temp = str.c_str();
    size_t len = std::mbsrtowcs(NULL, &temp, 0, &state);
    wchar_t* pUnicode = new wchar_t[len + 1];
    memset(pUnicode, 0, (len + 1) * sizeof(wchar_t));
    std::mbsrtowcs(pUnicode, &temp, len, &state);
    std::wstring rt;
    rt = (wchar_t*)pUnicode;
    delete[] pUnicode;
    return rt;
  }
#endif
}

#endif // STRING_HELPER_H_
