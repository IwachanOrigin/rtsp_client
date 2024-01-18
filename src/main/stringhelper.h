
#ifndef STRING_HELPER_H_
#define STRING_HELPER_H_

#ifdef _WIN32
#include <windows.h>
#endif // _WIN32

#include <string>
#include <vector>

namespace stringHelper
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
std::wstring stringToWstring(T str)
{
  int unicodeLen = ::MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
  wchar_t* pUnicode = new wchar_t[unicodeLen + 1];
  std::memset(pUnicode, 0, (unicodeLen + 1) * sizeof(wchar_t));
  ::MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, (LPWSTR)pUnicode, unicodeLen);
  std::wstring rt;
  rt = (wchar_t*)pUnicode;
  delete[] pUnicode;
  return rt;
}

#else

// over c++20
// ref : https://gist.github.com/danzek/d6a0e4a48a5439e7f808ed1497f6268e?permalink_comment_id=4289664#gistcomment-4289664

template <class T>
std::string wstringToString(T ws)
{
  // encoding is UTF8
  std::vector<char> buf(ws.size());
  std::use_facet<std::ctype<wchar_t>>(std::locale()).narrow(ws.data(), ws.data() + ws.size(), '?', buf.data());
  return std::string(buf.data(), buf.size());
}

template <class T>
std::wstring stringToWstring(T str)
{
  std::vector<wchar_t> buf(str.size());
  std::use_facet<std::ctype<wchar_t>>(std::locale()).widen(str.data(), str.data() + str.size(), buf.data());
  return std::wstring(buf.data(), buf.size());
}

#endif // _WIN32

} // namespace

#endif // STRING_HELPER_H_
