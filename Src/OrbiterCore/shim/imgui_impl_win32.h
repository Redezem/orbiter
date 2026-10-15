// macOS port shim: imgui_impl_win32.h. The macOS app uses its own ImGui
// backend (SDL/Metal); this shim only satisfies the legacy compile-time
// reference and the WndProc message hook, which is inert on macOS.

#ifndef ORBITER_MACOS_SHIM_IMGUI_IMPL_WIN32_H
#define ORBITER_MACOS_SHIM_IMGUI_IMPL_WIN32_H

#include <imgui.h>
#include <windows.h>

#ifndef IMGUI_IMPL_API
#define IMGUI_IMPL_API
#endif

inline bool ImGui_ImplWin32_Init(void *hwnd) { (void)hwnd; return true; }
inline void ImGui_ImplWin32_Shutdown() {}
inline bool ImGui_ImplWin32_NewFrame() { return true; }
inline LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	(void)hWnd; (void)msg; (void)wParam; (void)lParam;
	return 0;
}

#endif // ORBITER_MACOS_SHIM_IMGUI_IMPL_WIN32_H
