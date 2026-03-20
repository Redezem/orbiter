// Copyright (c) Martin Schweiger
// Licensed under the MIT License

#ifndef __ORBITERPLATFORM_H
#define __ORBITERPLATFORM_H

#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
#include <windows.h>
#else

#ifndef CALLBACK
#define CALLBACK
#endif

#ifndef WINAPI
#define WINAPI
#endif

#ifndef PASCAL
#define PASCAL
#endif

#ifndef FAR
#define FAR
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

using BOOL = int;
using BYTE = std::uint8_t;
using WORD = std::uint16_t;
using DWORD = std::uint32_t;
using UINT = std::uint32_t;
using INT16 = std::int16_t;
using INT32 = std::int32_t;
using WCHAR = wchar_t;
using ULONG_PTR = std::uintptr_t;
using DWORD_PTR = std::uintptr_t;
using LONG_PTR = std::intptr_t;
using UINT_PTR = std::uintptr_t;
using WPARAM = std::uintptr_t;
using LPARAM = std::intptr_t;
using LRESULT = std::intptr_t;
using INT_PTR = std::intptr_t;
using COLORREF = DWORD;
using HRESULT = long;
using HANDLE = void *;
using HINSTANCE = void *;
using HMODULE = void *;
using HWND = void *;
using HDC = void *;
using HGDIOBJ = void *;
using HBITMAP = void *;
using HFONT = void *;
using HPEN = void *;
using HBRUSH = void *;
using HICON = void *;
using HCURSOR = void *;
using TCHAR = char;
using PSTR = char *;
using PCSTR = const char *;
using LPSTR = char *;
using LPCSTR = const char *;
using LPTSTR = char *;
using LPWSTR = wchar_t *;
using LPCWSTR = const wchar_t *;

struct POINT {
	long x;
	long y;
};

struct RECT {
	long left;
	long top;
	long right;
	long bottom;
};

struct SIZE {
	long cx;
	long cy;
};

using LPPOINT = POINT *;
using LPCPOINT = const POINT *;
using LPRECT = RECT *;
using LPCRECT = const RECT *;
using LPSIZE = SIZE *;
using LPCSIZE = const SIZE *;
using DLGPROC = INT_PTR (CALLBACK *)(HWND, UINT, WPARAM, LPARAM);

#ifndef S_OK
#define S_OK ((HRESULT)0L)
#endif

#ifndef E_FAIL
#define E_FAIL ((HRESULT)0x80004005L)
#endif

#ifndef E_OUTOFMEMORY
#define E_OUTOFMEMORY ((HRESULT)0x8007000EL)
#endif

#ifndef SUCCEEDED
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#endif

#ifndef FAILED
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#endif

#endif

#if defined(_WIN32)
#define OAPI_PLATFORM_WINDOWS 1
#define OAPI_PLATFORM_MACOS 0
#if defined(ORBITER_BUILDING_SHARED)
#define OAPI_EXPORT __declspec(dllexport)
#define OAPI_IMPORT __declspec(dllimport)
#else
#define OAPI_EXPORT __declspec(dllexport)
#define OAPI_IMPORT __declspec(dllimport)
#endif
#else
#define OAPI_PLATFORM_WINDOWS 0
#if defined(__APPLE__)
#define OAPI_PLATFORM_MACOS 1
#else
#define OAPI_PLATFORM_MACOS 0
#endif
#define OAPI_EXPORT __attribute__((visibility("default")))
#define OAPI_IMPORT __attribute__((visibility("default")))
#endif

#define DLLEXPORT OAPI_EXPORT
#define DLLIMPORT OAPI_IMPORT
#define DLLCLBK extern "C" OAPI_EXPORT

using OAPIInstanceHandle = HINSTANCE;
using OAPIModuleHandle = HMODULE;
using OAPIWindowHandle = HWND;
using OAPIDeviceContextHandle = HDC;

#endif // !__ORBITERPLATFORM_H
