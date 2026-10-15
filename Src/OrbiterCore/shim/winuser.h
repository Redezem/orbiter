// macOS port shim: Win32 user32/gdi32 window, dialog and GDI surface used by
// the legacy Orbiter main loop (Orbiter.cpp, GraphicsAPI.cpp, Config.cpp,
// Launchpad, Pane, Mfd). On macOS the native app owns the window and input;
// these stubs compile the legacy code paths and are inert at runtime.

#ifndef ORBITER_MACOS_SHIM_WINUSER_H
#define ORBITER_MACOS_SHIM_WINUSER_H

#include <windows.h>

// ---- window style / class constants ----
#ifndef WS_OVERLAPPED
#define WS_OVERLAPPED 0x00000000L
#endif
#ifndef WS_POPUP
#define WS_POPUP 0x80000000L
#endif
#ifndef WS_VISIBLE
#define WS_VISIBLE 0x10000000L
#endif
#ifndef WS_CAPTION
#define WS_CAPTION 0x00C00000L
#endif
#ifndef WS_SYSMENU
#define WS_SYSMENU 0x00080000L
#endif
#ifndef WS_THICKFRAME
#define WS_THICKFRAME 0x00040000L
#endif
#ifndef WS_MINIMIZEBOX
#define WS_MINIMIZEBOX 0x00020000L
#endif
#ifndef WS_MAXIMIZEBOX
#define WS_MAXIMIZEBOX 0x00010000L
#endif
#ifndef WS_EX_TOPMOST
#define WS_EX_TOPMOST 0x00000008L
#endif
#ifndef WS_OVERLAPPEDWINDOW
#define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
#endif
#ifndef CW_USEDEFAULT
#define CW_USEDEFAULT ((int)0x80000000)
#endif
#ifndef SW_SHOW
#define SW_SHOW 5
#endif
#ifndef SW_HIDE
#define SW_HIDE 0
#endif
#ifndef SW_SHOWNORMAL
#define SW_SHOWNORMAL 1
#endif
#ifndef SW_SHOWDEFAULT
#define SW_SHOWDEFAULT 10
#endif
#ifndef SWP_NOMOVE
#define SWP_NOMOVE 0x0002
#endif
#ifndef SWP_NOSIZE
#define SWP_NOSIZE 0x0001
#endif
#ifndef SWP_NOZORDER
#define SWP_NOZORDER 0x0004
#endif
#ifndef SWP_SHOWWINDOW
#define SWP_SHOWWINDOW 0x0040
#endif
#ifndef HWND_TOP
#define HWND_TOP ((HWND)0)
#endif
#ifndef GWL_USERDATA
#define GWL_USERDATA (-21)
#endif
#ifndef GWLP_USERDATA
#define GWLP_USERDATA GWL_USERDATA
#endif
#ifndef GWL_STYLE
#define GWL_STYLE (-16)
#endif
#ifndef GWLP_WNDPROC
#define GWLP_WNDPROC (-4)
#endif
#ifndef GWLP_HINSTANCE
#define GWLP_HINSTANCE (-6)
#endif
#ifndef SM_CXSCREEN
#define SM_CXSCREEN 0
#endif
#ifndef SM_CYSCREEN
#define SM_CYSCREEN 1
#endif
#ifndef WM_QUIT
#define WM_QUIT 0x0012
#endif
#ifndef WM_KEYDOWN
#define WM_KEYDOWN 0x0100
#endif
#ifndef WM_KEYUP
#define WM_KEYUP 0x0101
#endif
#ifndef WM_SYSKEYDOWN
#define WM_SYSKEYDOWN 0x0104
#endif
#ifndef WM_SYSKEYUP
#define WM_SYSKEYUP 0x0105
#endif
#ifndef WM_CHAR
#define WM_CHAR 0x0102
#endif
#ifndef WM_ACTIVATE
#define WM_ACTIVATE 0x0006
#endif
#ifndef WM_SETFOCUS
#define WM_SETFOCUS 0x0007
#endif
#ifndef WM_KILLFOCUS
#define WM_KILLFOCUS 0x0008
#endif
#ifndef WM_MOVE
#define WM_MOVE 0x0003
#endif
#ifndef WM_ENTERSIZEMOVE
#define WM_ENTERSIZEMOVE 0x0231
#endif
#ifndef WM_EXITSIZEMOVE
#define WM_EXITSIZEMOVE 0x0232
#endif
#ifndef WM_TIMER
#define WM_TIMER 0x0113
#endif
#ifndef WM_DESTROY
#define WM_DESTROY 0x0002
#endif
#ifndef WM_ERASEBKGND
#define WM_ERASEBKGND 0x0014
#endif
#ifndef WM_PAINT
#define WM_PAINT 0x000F
#endif
#ifndef WM_GETMINMAXINFO
#define WM_GETMINMAXINFO 0x0024
#endif
#ifndef WM_NCCREATE
#define WM_NCCREATE 0x0081
#endif
#ifndef WM_CREATE
#define WM_CREATE 0x0001
#endif
#ifndef WM_LBUTTONDBLCLK
#define WM_LBUTTONDBLCLK 0x0203
#endif
#ifndef WM_RBUTTONDBLCLK
#define WM_RBUTTONDBLCLK 0x0206
#endif
#ifndef WM_CAPTURECHANGED
#define WM_CAPTURECHANGED 0x0215
#endif
#ifndef WM_CTLCOLORSTATIC
#define WM_CTLCOLORSTATIC 0x0138
#endif
#ifndef WM_VSCROLL
#define WM_VSCROLL 0x0115
#endif
#ifndef WM_HSCROLL
#define WM_HSCROLL 0x0114
#endif
#ifndef WM_USER
#define WM_USER 0x0400
#endif
#ifndef BM_SETCHECK
#define BM_SETCHECK 0x00F1
#endif
#ifndef BM_GETCHECK
#define BM_GETCHECK 0x00F0
#endif
#ifndef BST_CHECKED
#define BST_CHECKED 1
#endif
#ifndef BST_UNCHECKED
#define BST_UNCHECKED 0
#endif
#ifndef VK_SHIFT
#define VK_SHIFT 0x10
#endif
#ifndef VK_CONTROL
#define VK_CONTROL 0x11
#endif
#ifndef VK_MENU
#define VK_MENU 0x12
#endif
#ifndef VK_RETURN
#define VK_RETURN 0x0D
#endif
#ifndef VK_ESCAPE
#define VK_ESCAPE 0x1B
#endif
#ifndef VK_SPACE
#define VK_SPACE 0x20
#endif
#ifndef VK_TAB
#define VK_TAB 0x09
#endif
#ifndef MB_OK
#define MB_OK 0x00000000L
#endif
#ifndef MB_ICONERROR
#define MB_ICONERROR 0x00000010L
#endif
#ifndef IDOK
#define IDOK 1
#endif
#ifndef IDCANCEL
#define IDCANCEL 2
#endif
#ifndef SW_SHOWMINIMIZED
#define SW_SHOWMINIMIZED 2
#endif

#ifndef MAKEINTRESOURCE
#define MAKEINTRESOURCE(i) (reinterpret_cast<LPCSTR>((uintptr_t)(i)))
#endif
#ifndef MAKEINTRESOURCEW
#define MAKEINTRESOURCEW(i) (reinterpret_cast<LPCWSTR>((uintptr_t)(i)))
#endif

// Stock objects / cursors
#ifndef WHITE_BRUSH
#define WHITE_BRUSH 0
#endif
#ifndef BLACK_BRUSH
#define BLACK_BRUSH 1
#endif
#ifndef NULL_PEN
#define NULL_PEN 8
#endif
#ifndef SYSTEM_FONT
#define SYSTEM_FONT 14
#endif
#ifndef IDC_ARROW
#define IDC_ARROW MAKEINTRESOURCE(32512)
#endif
#ifndef IDC_IBEAM
#define IDC_IBEAM MAKEINTRESOURCE(32513)
#endif

typedef LRESULT (CALLBACK *WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef INT_PTR (CALLBACK *DLGPROC2)(HWND, UINT, WPARAM, LPARAM);

struct WNDCLASSA {
	UINT style;
	WNDPROC lpfnWndProc;
	int cbClsExtra;
	int cbWndExtra;
	HINSTANCE hInstance;
	HICON hIcon;
	HCURSOR hCursor;
	HBRUSH hbrBackground;
	LPCSTR lpszMenuName;
	LPCSTR lpszClassName;
};
using WNDCLASS = WNDCLASSA;

struct WNDCLASSEXA {
	UINT cbSize;
	UINT style;
	WNDPROC lpfnWndProc;
	int cbClsExtra;
	int cbWndExtra;
	HINSTANCE hInstance;
	HICON hIcon;
	HCURSOR hCursor;
	HBRUSH hbrBackground;
	LPCSTR lpszMenuName;
	LPCSTR lpszClassName;
	HICON hIconSm;
};
using WNDCLASSEX = WNDCLASSEXA;

// ---- window functions (inert on macOS; the native app owns the window) ----
inline ATOM RegisterClassA(const WNDCLASSA *wc) { (void)wc; return 1; }
inline ATOM RegisterClassExA(const WNDCLASSEXA *wc) { (void)wc; return 1; }
inline bool UnregisterClassA(LPCSTR cls, HINSTANCE h) { (void)cls; (void)h; return true; }

inline HWND CreateWindowExA(DWORD exStyle, LPCSTR cls, LPCSTR title, DWORD style,
                            int x, int y, int w, int h, HWND parent, HMENU menu,
                            HINSTANCE inst, void *param)
{
	(void)exStyle; (void)cls; (void)title; (void)style; (void)x; (void)y;
	(void)w; (void)h; (void)parent; (void)menu; (void)inst; (void)param;
	return nullptr;
}

inline bool DestroyWindow(HWND hwnd) { (void)hwnd; return true; }
inline void PostQuitMessage(int code) { (void)code; }
inline LRESULT DefWindowProcA(HWND hwnd, UINT msg, WPARAM w, LPARAM l)
{
	(void)hwnd; (void)msg; (void)w; (void)l;
	return 0;
}
inline bool ShowWindow(HWND hwnd, int cmd) { (void)hwnd; (void)cmd; return true; }
inline bool UpdateWindow(HWND hwnd) { (void)hwnd; return true; }
inline bool IsWindow(HWND hwnd) { return hwnd != nullptr; }
inline bool SetForegroundWindow(HWND hwnd) { (void)hwnd; return true; }
inline HWND GetForegroundWindow() { return nullptr; }
inline bool BringWindowToTop(HWND hwnd) { (void)hwnd; return true; }
inline bool SetWindowTextA(HWND hwnd, LPCSTR text) { (void)hwnd; (void)text; return true; }

inline LONG_PTR GetWindowLongPtrA(HWND hwnd, int idx) { (void)hwnd; (void)idx; return 0; }
inline LONG_PTR SetWindowLongPtrA(HWND hwnd, int idx, LONG_PTR val) { (void)hwnd; (void)idx; (void)val; return 0; }
inline LONG GetWindowLongA(HWND hwnd, int idx) { (void)hwnd; (void)idx; return 0; }
inline LONG SetWindowLongA(HWND hwnd, int idx, LONG val) { (void)hwnd; (void)idx; (void)val; return 0; }

inline bool GetWindowRect(HWND hwnd, RECT *r)
{
	if (r) { r->left = 0; r->top = 0; r->right = 1920; r->bottom = 1080; }
	return true;
}
inline bool GetClientRect(HWND hwnd, RECT *r)
{
	if (r) { r->left = 0; r->top = 0; r->right = 1920; r->bottom = 1080; }
	return true;
}
inline bool ClientToScreen(HWND hwnd, POINT *pt) { (void)hwnd; (void)pt; return true; }
inline bool MoveWindow(HWND hwnd, int x, int y, int w, int h, bool repaint)
{
	(void)hwnd; (void)x; (void)y; (void)w; (void)h; (void)repaint;
	return true;
}
inline bool SetWindowPos(HWND hwnd, HWND after, int x, int y, int w, int h, UINT flags)
{
	(void)hwnd; (void)after; (void)x; (void)y; (void)w; (void)h; (void)flags;
	return true;
}
inline void InvalidateRect(HWND hwnd, const RECT *r, bool erase) { (void)hwnd; (void)r; (void)erase; }
inline void ValidateRect(HWND hwnd, const RECT *r) { (void)hwnd; (void)r; }

inline HWND GetFocus() { return nullptr; }
inline HWND SetFocus(HWND hwnd) { (void)hwnd; return nullptr; }
inline HCURSOR SetCursor(HCURSOR c) { (void)c; return nullptr; }
inline int ShowCursor(bool show) { (void)show; return 1; }
inline int GetSystemMetrics(int idx) { (void)idx; return idx == SM_CXSCREEN ? 1920 : 1080; }
inline DWORD GetWindowThreadProcessId(HWND hwnd, DWORD *pid)
{
	(void)hwnd;
	if (pid) *pid = 0;
	return 0;
}

// ---- message loop ----
inline bool PeekMessageA(LPMSG msg, HWND hwnd, UINT min, UINT max, UINT remove)
{
	(void)hwnd; (void)min; (void)max; (void)remove;
	if (msg) { msg->hwnd = nullptr; msg->message = WM_QUIT; msg->wParam = 0; msg->lParam = 0; msg->time = 0; msg->pt.x = 0; msg->pt.y = 0; }
	return true; // one synthetic WM_QUIT, then caller loop exits cleanly
}
inline bool GetMessageA(LPMSG msg, HWND hwnd, UINT min, UINT max)
{
	(void)hwnd; (void)min; (void)max;
	if (msg) { msg->hwnd = nullptr; msg->message = WM_QUIT; msg->wParam = 0; msg->lParam = 0; msg->time = 0; msg->pt.x = 0; msg->pt.y = 0; }
	return false; // WM_QUIT value 0x0012 != 0 so caller treats as quit via return value
}
inline bool TranslateMessage(const MSG *msg) { (void)msg; return false; }
inline LRESULT DispatchMessageA(const MSG *msg) { (void)msg; return 0; }

// ---- dialogs / controls ----
inline HWND CreateDialogParamA(HINSTANCE inst, LPCSTR tmpl, HWND parent, DLGPROC proc, LPARAM init)
{
	(void)inst; (void)tmpl; (void)parent; (void)proc; (void)init;
	return nullptr;
}
inline INT_PTR DialogBoxParamA(HINSTANCE inst, LPCSTR tmpl, HWND parent, DLGPROC proc, LPARAM init)
{
	(void)inst; (void)tmpl; (void)parent; (void)proc; (void)init;
	return 0;
}
inline bool EndDialog(HWND hDlg, INT_PTR result) { (void)hDlg; (void)result; return true; }
inline HWND GetDlgItem(HWND hDlg, int item) { (void)hDlg; (void)item; return nullptr; }
inline bool DestroyWindow2(HWND h) { (void)h; return true; }
inline int MessageBoxA(HWND hwnd, LPCSTR text, LPCSTR caption, UINT type)
{
	(void)hwnd; (void)text; (void)caption; (void)type;
	return IDOK;
}
inline int LoadStringA(HINSTANCE inst, UINT id, char *buf, int max)
{
	(void)inst; (void)id;
	if (buf && max > 0) buf[0] = '\0';
	return 0;
}
inline HICON LoadIconA(HINSTANCE inst, LPCSTR name) { (void)inst; (void)name; return nullptr; }
inline HCURSOR LoadCursorA(HINSTANCE inst, LPCSTR name) { (void)inst; (void)name; return nullptr; }
inline HBRUSH GetSysColorBrush(int idx) { (void)idx; return nullptr; }
inline HGDIOBJ GetStockObject(int idx) { (void)idx; return nullptr; }

// unsuffixed Win32 aliases (ANSI variants)
#ifndef RegisterClass
#define RegisterClass RegisterClassA
#endif
#ifndef RegisterClassEx
#define RegisterClassEx RegisterClassExA
#endif
#ifndef UnregisterClass
#define UnregisterClass UnregisterClassA
#endif
#ifndef CreateWindowEx
#define CreateWindowEx CreateWindowExA
#endif
#ifndef CreateWindow
#define CreateWindow CreateWindowExA
#endif
#ifndef DefWindowProc
#define DefWindowProc DefWindowProcA
#endif
#ifndef GetWindowLongPtr
#define GetWindowLongPtr GetWindowLongPtrA
#endif
#ifndef SetWindowLongPtr
#define SetWindowLongPtr SetWindowLongPtrA
#endif
#ifndef GetWindowLong
#define GetWindowLong GetWindowLongA
#endif
#ifndef SetWindowLong
#define SetWindowLong SetWindowLongA
#endif
#ifndef SetWindowText
#define SetWindowText SetWindowTextA
#endif
#ifndef PeekMessage
#define PeekMessage PeekMessageA
#endif
#ifndef GetMessage
#define GetMessage GetMessageA
#endif
#ifndef DispatchMessage
#define DispatchMessage DispatchMessageA
#endif
#ifndef CreateDialogParam
#define CreateDialogParam CreateDialogParamA
#endif
#ifndef CreateDialog
#define CreateDialog CreateDialogParamA
#endif
#ifndef DialogBoxParam
#define DialogBoxParam DialogBoxParamA
#endif
#ifndef MessageBox
#define MessageBox MessageBoxA
#endif
#ifndef LoadString
#define LoadString LoadStringA
#endif
#ifndef LoadIcon
#define LoadIcon LoadIconA
#endif
#ifndef LoadCursor
#define LoadCursor LoadCursorA
#endif
#ifndef GetModuleFileName
#define GetModuleFileName GetModuleFileNameA
#endif

// wsprintf
inline int wsprintfA(char *buf, const char *fmt, ...)
{
	(void)buf; (void)fmt;
	return 0;
}
#ifndef wsprintf
#define wsprintf wsprintfA
#endif

#endif // ORBITER_MACOS_SHIM_WINUSER_H
