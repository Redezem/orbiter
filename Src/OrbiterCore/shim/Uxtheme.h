// macOS port shim: UxTheme (visual styles) — compile-only placeholder.
#ifndef ORBITER_MACOS_SHIM_UXTHEME_H
#define ORBITER_MACOS_SHIM_UXTHEME_H

#include <windows.h>

inline HRESULT SetWindowTheme(HWND hwnd, LPCWSTR subAppName, LPCWSTR subClassId)
{ (void)hwnd; (void)subAppName; (void)subClassId; return 0; }

#endif // ORBITER_MACOS_SHIM_UXTHEME_H
