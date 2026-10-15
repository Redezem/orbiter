// macOS port shim: satisfies the Win32 includes the Orbiter engine core
// expects. Types come from OrbiterPlatform.h; this adds the handful of
// functions and legacy D3D7/DirectInput types the engine references.

#ifndef ORBITER_MACOS_SHIM_WINDOWS_H
#define ORBITER_MACOS_SHIM_WINDOWS_H

#include <OrbiterPlatform.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <cwchar>
#include <strings.h>
#ifndef __int64_defined
#define __int64_defined
using __int64 = long long;
#endif

#ifndef LONGLONG
using LONGLONG = long long;
#endif

#ifndef _stricmp
inline int _stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
#endif
#ifndef _strnicmp
inline int _strnicmp(const char *a, const char *b, size_t n) { return strncasecmp(a, b, n); }
#endif
#ifndef strnicmp
#define strnicmp strncasecmp
#endif

#ifndef MAX_PATH
#define MAX_PATH 1024
#endif

#ifndef INT
using INT = int;
#endif
#ifndef UINT
using UINT = unsigned int;
#endif
#ifndef UINT8
using UINT8 = std::uint8_t;
#endif
#ifndef UINT16
using UINT16 = std::uint16_t;
#endif
#ifndef UINT32
using UINT32 = std::uint32_t;
#endif

#ifndef MAKELANGID
#define MAKELANGID(p, s) ((((WORD)(s)) << 8) | (WORD)(p))
#endif
#ifndef LANG_NEUTRAL
#define LANG_NEUTRAL 0x00
#endif
#ifndef SUBLANG_DEFAULT
#define SUBLANG_DEFAULT 0x01
#endif

#ifndef FORMAT_MESSAGE_ALLOCATE_BUFFER
#define FORMAT_MESSAGE_ALLOCATE_BUFFER 0x00000100
#endif
#ifndef FORMAT_MESSAGE_IGNORE_INSERTS
#define FORMAT_MESSAGE_IGNORE_INSERTS 0x00000200
#endif
#ifndef FORMAT_MESSAGE_FROM_SYSTEM
#define FORMAT_MESSAGE_FROM_SYSTEM 0x00001000
#endif

#ifndef RGB
#define RGB(r, g, b) ((COLORREF)(((BYTE)(r) | ((WORD)((BYTE)(g)) << 8)) | (((DWORD)(BYTE)(b)) << 16)))
#endif

// Minimal COM-like object so legacy code can call ->Release() on opaque
// device pointers. The macOS renderer never creates these; engine code only
// nulls or releases them. Legacy D3D7 device calls compile to no-ops.
struct ComReleasable {
	void Release() {}
	void SetRenderState(int, int) {}
	void GetRenderState(int, void *) {}
	void SetTexture(int, void *) {}
	void DrawPrimitive(int, int, void *, int, void *) {}
	void DrawIndexedPrimitive(int, int, void *, int, void *, int, int) {}
	void SetTransform(int, const void *) {}
	void SetMaterial(int, const void *) {}
	void BeginScene() {}
	void EndScene() {}
	void Clear(int, int, int) {}
	void SetViewport(void *) {}
	void SetRenderMode(int) {}
	void SetColorKey(int, void *) {}
};

using LPDIRECTDRAWSURFACE7 = ComReleasable *;
using LPDIRECTDRAW7 = ComReleasable *;
using LPDIRECT3D7 = ComReleasable *;
using LPDIRECT3DDEVICE7 = ComReleasable *;
using LPDIRECT3DVERTEXBUFFER7 = ComReleasable *;

#ifndef E_INVALIDARG
#define E_INVALIDARG ((HRESULT)0x80070057L)
#endif
#ifndef E_NOINTERFACE
#define E_NOINTERFACE ((HRESULT)0x80004002L)
#endif
#ifndef E_FAIL
#define E_FAIL ((HRESULT)0x80004005L)
#endif

inline DWORD GetLastError() { return 0; }

// FormatMessage with ALLOCATE_BUFFER: allocate a small message buffer so
// LogOut_LastError has something to print and LocalFree can release.
inline DWORD FormatMessageA(DWORD flags, const void *src, DWORD msgID, DWORD langID,
                            char *out, DWORD size, void *args)
{
	(void)src; (void)langID; (void)args;
	if (flags & FORMAT_MESSAGE_ALLOCATE_BUFFER) {
		char **alloc = reinterpret_cast<char **>(out);
		char *buf = static_cast<char *>(malloc(128));
		if (!buf) return 0;
		snprintf(buf, 128, "system error %lu", (unsigned long)msgID);
		*alloc = buf;
		return static_cast<DWORD>(strlen(buf));
	}
	if (out && size > 0) snprintf(out, size, "system error %lu", (unsigned long)msgID);
	return static_cast<DWORD>(strlen(out));
}
inline DWORD FormatMessageW(DWORD flags, const void *src, DWORD msgID, DWORD langID,
                            WCHAR *out, DWORD size, void *args)
{
	(void)flags; (void)src; (void)msgID; (void)langID; (void)out; (void)size; (void)args;
	return 0;
}
inline void *LocalFree(void *p) { free(p); return nullptr; }

// Win32 maps the unsuffixed name to the ANSI variant via a macro.
#ifndef FormatMessage
#define FormatMessage FormatMessageA
#endif

#ifndef HBRUSH
using HBRUSH = void *;
#endif
#ifndef LPBOOL
using LPBOOL = bool *;
#endif
#ifndef HPEN
using HPEN = void *;
#endif
#ifndef HFONT
using HFONT = void *;
#endif

struct LOGBRUSH {
	UINT lbStyle;
	COLORREF lbColor;
	long lbLength;
};

// GDI object creation (Pane.cpp legacy pen/brush init; inert on macOS)
inline HPEN CreatePen(int style, int width, COLORREF color)
{ (void)style; (void)width; (void)color; return nullptr; }
inline HBRUSH CreateBrushIndirect(const LOGBRUSH *lb) { (void)lb; return nullptr; }
#ifndef NULL_BRUSH
#define NULL_BRUSH 5
#endif
#ifndef TRANSPARENT
#define TRANSPARENT 1
#endif
#ifndef OPAQUE
#define OPAQUE 2
#endif
#ifndef TA_CENTER
#define TA_CENTER 1
#endif
#ifndef TA_LEFT
#define TA_LEFT 0
#endif
#ifndef TA_RIGHT
#define TA_RIGHT 2
#endif
#ifndef TA_TOP
#define TA_TOP 0
#endif
#ifndef TA_BOTTOM
#define TA_BOTTOM 8
#endif
#ifndef TA_BASELINE
#define TA_BASELINE 24
#endif
using HDC = void *;
inline int SetTextAlign(void *hdc, uint32_t align) { (void)hdc; (void)align; return 0; }
inline bool MoveToEx(void *hdc, int x, int y, void *lp) { (void)hdc; (void)x; (void)y; (void)lp; return true; }
inline bool LineTo(void *hdc, int x, int y) { (void)hdc; (void)x; (void)y; return true; }
inline COLORREF SetTextColor(void *hdc, COLORREF color) { (void)hdc; return color; }
inline COLORREF SetBkColor(void *hdc, COLORREF color) { (void)hdc; return color; }
inline int SetBkMode(void *hdc, int mode) { (void)hdc; (void)mode; return 0; }
inline bool TextOutA(void *hdc, int x, int y, const char *text, int len)
{ (void)hdc; (void)x; (void)y; (void)text; (void)len; return true; }
#ifndef TextOut
#define TextOut TextOutA
#endif
inline bool Rectangle(void *hdc, int left, int top, int right, int bottom)
{ (void)hdc; (void)left; (void)top; (void)right; (void)bottom; return true; }

// Code page conversion (VectorMap.cpp)
inline int MultiByteToWideChar(UINT codepage, DWORD flags, LPCSTR mb, int mbLen, LPWSTR wide, int wideLen)
{
	(void)codepage; (void)flags;
	if (!mb || mbLen == -1) mbLen = (int)strlen(mb);
	int n = 0;
	while (n < mbLen && mb[n]) {
		if (wide && n < wideLen) wide[n] = (WCHAR)(unsigned char)mb[n];
		n++;
	}
	if (wide && n < wideLen) wide[n] = 0;
	return n;
}
inline int WideCharToMultiByte(UINT codepage, DWORD flags, LPCWSTR wide, int wideLen, LPSTR mb, int mbLen, LPCSTR def, LPBOOL used)
{
	(void)codepage; (void)flags; (void)def; (void)used;
	if (!wide || wideLen == -1) wideLen = (int)wcslen(wide);
	int n = 0;
	while (n < wideLen && wide[n]) {
		if (mb && n < mbLen) mb[n] = (char)wide[n];
		n++;
	}
	if (mb && n < mbLen) mb[n] = 0;
	return n;
}

inline HWND GetDesktopWindow() { return nullptr; }
inline DWORD GetCurrentDirectoryA(DWORD size, char *buffer)
{
	if (buffer && size > 0) buffer[0] = '\0';
	return 0;
}
inline bool SetCurrentDirectoryA(LPCSTR path) { (void)path; return true; }
#ifndef SetCurrentDirectory
#define SetCurrentDirectory SetCurrentDirectoryA
#endif

// Legacy GetVersion() call in WinMain (Windows version check). Orbiter's
// own build version comes from Orbiter::GetVersion(); provide the free
// function so the log line compiles.
inline int GetVersion() { return 0; }

// MSVC CRT env var setter
#ifndef _putenv
#define _putenv putenv
#endif
#ifndef _getcwd
#define _getcwd getcwd
#endif

// Legacy timing: milliseconds since an arbitrary epoch.
inline DWORD timeGetTime(){
	using namespace std::chrono;
	return static_cast<DWORD>(
		duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}
inline bool timeBeginPeriod(UINT p) { (void)p; return true; }
inline bool timeEndPeriod(UINT p) { (void)p; return true; }

#ifndef DWORDLONG
using DWORDLONG = unsigned long long;
#endif
#ifndef LPBYTE
using LPBYTE = unsigned char *;
#endif

#ifndef ULONG
using ULONG = unsigned long;
#endif
#ifndef LONG
using LONG = long;
#endif

#ifndef LARGE_INTEGER
union LARGE_INTEGER {
	struct { DWORD LowPart; LONG HighPart; };
	long long QuadPart;
};
#endif

// High-resolution performance counter backed by POSIX clock.
inline bool QueryPerformanceFrequency(LARGE_INTEGER *freq)
{
	if (freq) freq->QuadPart = 1000000000LL; // nanosecond resolution
	return true;
}
inline bool QueryPerformanceCounter(LARGE_INTEGER *cnt)
{
	if (cnt) {
		using namespace std::chrono;
		cnt->QuadPart = duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
	}
	return true;
}

#ifndef ZeroMemory
#define ZeroMemory(addr, len) memset((addr), 0, (len))
#endif

struct POINTS {
	short x;
	short y;
};
using LPPOINTS = POINTS *;

struct NMHDR {
	HWND hwndFrom;
	UINT idFrom;
	UINT code;
};
using LPNMHDR = NMHDR *;

struct tagMSG {
	HWND hwnd;
	UINT message;
	WPARAM wParam;
	LPARAM lParam;
	DWORD time;
	POINT pt;
};
using MSG = tagMSG;
using LPMSG = MSG *;

using HTREEITEM = void *;
using LONG_PTR = std::intptr_t;
using HGDIOBJ = void *;

#ifndef ATOM
using ATOM = WORD;
#endif
#ifndef HMENU
using HMENU = void *;
#endif

// COM GUID (shared by dinput.h / wincodec.h shims)
#ifndef _GUID_DEFINED
#define _GUID_DEFINED
struct GUID {
	unsigned long Data1;
	unsigned short Data2;
	unsigned short Data3;
	unsigned char Data4[8];
	inline bool operator==(const GUID &o) const {
		return Data1 == o.Data1 && Data2 == o.Data2 && Data3 == o.Data3 &&
		       memcmp(Data4, o.Data4, 8) == 0;
	}
	inline bool operator!=(const GUID &o) const { return !(*this == o); }
};
#endif
#ifndef IsEqualGUID
inline bool IsEqualGUID(const GUID &a, const GUID &b)
{
	return a.Data1 == b.Data1 && a.Data2 == b.Data2 && a.Data3 == b.Data3 &&
	       memcmp(a.Data4, b.Data4, 8) == 0;
}
#endif

// MSVC math exception trap (not used on macOS; inert)
#ifndef _EXCEPTION_DEFINED
#define _EXCEPTION_DEFINED
struct _exception {
	int type;
	char *name;
	double retval;
	double arg1;
	double arg2;
};
#endif

// Registry (WINE detection etc. — inert on macOS)
using HKEY = void *;
#ifndef HKEY_CURRENT_USER
#define HKEY_CURRENT_USER ((HKEY)(long)0x80000001)
#endif
#ifndef HKEY_LOCAL_MACHINE
#define HKEY_LOCAL_MACHINE ((HKEY)(long)0x80000002)
#endif
#ifndef KEY_QUERY_VALUE
#define KEY_QUERY_VALUE 1
#endif
#ifndef ERROR_SUCCESS
#define ERROR_SUCCESS 0L
#endif
#ifndef ERROR_FILE_NOT_FOUND
#define ERROR_FILE_NOT_FOUND 2L
#endif
inline long RegOpenKeyExA(HKEY root, LPCSTR sub, DWORD opts, DWORD access, HKEY *out)
{
	(void)root; (void)sub; (void)opts; (void)access;
	if (out) *out = nullptr;
	return 2; // ERROR_FILE_NOT_FOUND: no WINE registry on macOS
}
inline long RegCloseKey(HKEY key) { (void)key; return 0; }
#ifndef RegOpenKeyEx
#define RegOpenKeyEx RegOpenKeyExA
#endif

// GDI device context + DIB section: legacy image assembly path. The macOS
// graphics client does not use these; stubs keep the code compiling and
// return null handles.
inline HDC GetDC(HWND hwnd) { (void)hwnd; return nullptr; }
inline void ReleaseDC(HWND hwnd, HDC hdc) { (void)hwnd; (void)hdc; }

struct BITMAPINFOHEADER {
	DWORD biSize;
	LONG biWidth;
	LONG biHeight;
	WORD biPlanes;
	WORD biBitCount;
	DWORD biCompression;
	DWORD biSizeImage;
	LONG biXPelsPerMeter;
	LONG biYPelsPerMeter;
	DWORD biClrUsed;
	DWORD biClrImportant;
};
struct BITMAPINFO {
	BITMAPINFOHEADER bmiHeader;
	int bmiColors[1];
};

#ifndef BI_RGB
#define BI_RGB 0L
#endif
#ifndef DIB_RGB_COLORS
#define DIB_RGB_COLORS 0
#endif
#ifndef DIB_WIDTHBYTES
#define DIB_WIDTHBYTES(bpp) ((((bpp) + 31) & ~31) / 8)
#endif

inline HBITMAP CreateDIBSection(HDC hdc, const BITMAPINFO *info, UINT usage, void **bits, void *section, DWORD offset)
{
	(void)hdc; (void)info; (void)usage; (void)section; (void)offset;
	if (bits) *bits = nullptr;
	return nullptr;
}

struct BITMAP {
	LONG bmType;
	LONG bmWidth;
	LONG bmHeight;
	LONG bmWidthBytes;
	WORD bmPlanes;
	WORD bmBitsPixel;
	void *bmBits;
};

inline int GetObjectA(HGDIOBJ obj, int count, void *buf)
{
	(void)obj;
	if (buf && count == (int)sizeof(BITMAP)) {
		BITMAP *bm = static_cast<BITMAP *>(buf);
		bm->bmType = 0; bm->bmWidth = 0; bm->bmHeight = 0; bm->bmWidthBytes = 0;
		bm->bmPlanes = 1; bm->bmBitsPixel = 32; bm->bmBits = nullptr;
		return sizeof(BITMAP);
	}
	return 0;
}
#ifndef GetObject
#define GetObject GetObjectA
#endif

inline HDC CreateCompatibleDC(HDC hdc) { (void)hdc; return nullptr; }
inline bool DeleteDC(HDC hdc) { (void)hdc; return true; }
inline HGDIOBJ SelectObject(HDC hdc, HGDIOBJ obj) { (void)hdc; (void)obj; return nullptr; }
inline HBITMAP CreateCompatibleBitmap(HDC hdc, int w, int h) { (void)hdc; (void)w; (void)h; return nullptr; }
#ifndef SRCCOPY
#define SRCCOPY 0x00CC0020L
#endif
inline bool BitBlt(HDC dst, int x, int y, int w, int h, HDC src, int sx, int sy, DWORD rop)
{
	(void)dst; (void)x; (void)y; (void)w; (void)h; (void)src; (void)sx; (void)sy; (void)rop;
	return true;
}
inline bool StretchBlt(HDC dst, int x, int y, int w, int h, HDC src, int sx, int sy, int sw, int sh, DWORD rop)
{
	(void)dst; (void)x; (void)y; (void)w; (void)h; (void)src; (void)sx; (void)sy; (void)sw; (void)sh; (void)rop;
	return true;
}

inline void OutputDebugStringA(LPCSTR s) { (void)s; }
#ifndef OutputDebugString
#define OutputDebugString OutputDebugStringA
#endif
inline int GetWindowTextA(HWND hwnd, char *buf, int max)
{
	(void)hwnd;
	if (buf && max > 0) buf[0] = '\0';
	return 0;
}
#ifndef GetWindowText
#define GetWindowText GetWindowTextA
#endif

inline bool DeleteObject(HGDIOBJ obj) { (void)obj; return true; }
#ifndef FAR
#define FAR
#endif
#ifndef PASCAL
#define PASCAL
#endif
#ifndef WINAPI
#define WINAPI
#endif
#ifndef CALLBACK
#define CALLBACK
#endif
#ifndef WM_NOTIFY
#define WM_NOTIFY 0x004E
#endif
#ifndef WM_COMMAND
#define WM_COMMAND 0x0111
#endif
#ifndef WM_INITDIALOG
#define WM_INITDIALOG 0x0110
#endif
#ifndef DDCKEY_SRCBLT
#define DDCKEY_SRCBLT 0x0000000E
#endif

#ifndef WM_CLOSE
#define WM_CLOSE 0x0010
#endif
#ifndef WM_SIZE
#define WM_SIZE 0x0005
#endif
#ifndef WM_LBUTTONDOWN
#define WM_LBUTTONDOWN 0x0201
#endif
#ifndef WM_LBUTTONUP
#define WM_LBUTTONUP 0x0202
#endif
#ifndef WM_RBUTTONDOWN
#define WM_RBUTTONDOWN 0x0204
#endif
#ifndef WM_RBUTTONUP
#define WM_RBUTTONUP 0x0205
#endif
#ifndef WM_MBUTTONDOWN
#define WM_MBUTTONDOWN 0x0207
#endif
#ifndef WM_MBUTTONUP
#define WM_MBUTTONUP 0x0208
#endif
#ifndef WM_MOUSEMOVE
#define WM_MOUSEMOVE 0x0200
#endif
#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL 0x020A
#endif
#ifndef HIWORD
#define HIWORD(l) ((WORD)(((DWORD)(l) >> 16) & 0xFFFF))
#endif
#ifndef LOWORD
#define LOWORD(l) ((WORD)((DWORD)(l) & 0xFFFF))
#endif

// MSVC CRT aliases
#ifndef _snprintf
#define _snprintf snprintf
#endif
#ifndef _strdup
#define _strdup strdup
#endif
#ifndef strcpy_s
#define strcpy_s(dst, n, src) strncpy((dst), (src), (n))
#endif
#ifndef strncpy_s
#define strncpy_s(dst, n, src, m) strncpy((dst), (src), (m))
#endif
#ifndef lstrlen
#define lstrlen strlen
#endif
// 64-bit file seek: macOS fseeko is 64-bit by default.
#ifndef _fseeki64
#define _fseeki64 fseeko
#endif
#ifndef _ftelli64
#define _ftelli64 ftello
#endif

// Window messaging: the macOS host owns its own event loop; engine calls
// that post messages to the legacy render window are no-ops.
inline bool PostMessageA(HWND hwnd, UINT msg, WPARAM w, LPARAM l)
{
	(void)hwnd; (void)msg; (void)w; (void)l;
	return false;
}
inline bool PostMessage(HWND hwnd, UINT msg, WPARAM w, LPARAM l)
{
	(void)hwnd; (void)msg; (void)w; (void)l;
	return false;
}

// Cursor tracking: the macOS host feeds camera input through its own event
// loop; legacy screen-cursor calls are no-ops returning a stationary cursor.
inline bool GetCursorPos(POINT *pt)
{
	if (pt) { pt->x = 0; pt->y = 0; }
	return true;
}
inline bool SetCursorPos(int x, int y) { (void)x; (void)y; return true; }
inline void ScreenToClient(HWND hwnd, POINT *pt) { (void)hwnd; (void)pt; }

// Dynamic module loading: the macOS host loads modules via dlopen through
// the platform runtime; engine code paths that call the Win32 API directly
// get no-op stubs so the core compiles unchanged.
inline HMODULE LoadLibraryA(const char *name) { (void)name; return nullptr; }
inline HMODULE LoadLibraryW(const WCHAR *name) { (void)name; return nullptr; }
inline HMODULE LoadLibrary(const char *name) { (void)name; return nullptr; }
inline void *GetProcAddress(HMODULE hmod, const char *name) { (void)hmod; (void)name; return nullptr; }
inline bool FreeLibrary(HMODULE hmod) { (void)hmod; return false; }

// MSVC secure CRT: map to the portable equivalents.
inline int fopen_s(FILE **fp, const char *filename, const char *mode)
{
	*fp = fopen(filename, mode);
	return *fp ? 0 : 1;
}

inline DWORD GetModuleFileNameA(HMODULE hmod, char *buffer, DWORD size)
{
	(void)hmod;
	if (buffer && size > 0) buffer[0] = '\0';
	return 0;
}
inline DWORD GetModuleFileNameW(HMODULE hmod, WCHAR *buffer, DWORD size)
{
	(void)hmod;
	if (buffer && size > 0) buffer[0] = 0;
	return 0;
}
#ifndef GetModuleFileName
#define GetModuleFileName GetModuleFileNameA
#endif

// Win32 CreateWindow macro supplies exStyle=0 like the real SDK.
#ifndef CreateWindowEx
#define CreateWindowEx CreateWindowExA
#endif
#ifndef CreateWindow
#define CreateWindow(...) CreateWindowExA(0, __VA_ARGS__)
#endif
#ifndef LPVOID
using LPVOID = void *;
#endif
#ifndef GetCurrentDirectory
#define GetCurrentDirectory GetCurrentDirectoryA
#endif
#ifndef stricmp
#define stricmp _stricmp
#endif
#ifndef TEXT
#define TEXT(s) s
#endif
#ifndef E_HANDLE
#define E_HANDLE ((HRESULT)0x80070006L)
#endif
#ifndef E_PENDING
#define E_PENDING ((HRESULT)0x800700AAL)
#endif

// Console attachment: macOS app already owns its stdio; no-op.
#ifndef ATTACH_PARENT_PROCESS
#define ATTACH_PARENT_PROCESS ((DWORD)-1)
#endif
inline bool AttachConsole(DWORD pid) { (void)pid; return true; }
inline bool AllocConsole() { return true; }

// Module handle
inline HMODULE GetModuleHandleA(LPCSTR name) { (void)name; return nullptr; }
#ifndef GetModuleHandle
#define GetModuleHandle GetModuleHandleA
#endif

// Embedded resources: macOS binary has no Win32 resource table; fail cleanly.
using HRSRC = void *;
using HGLOBAL = void *;
inline HRSRC FindResourceA(HMODULE mod, LPCSTR name, LPCSTR type) { (void)mod; (void)name; (void)type; return nullptr; }
inline void *LoadResource(HMODULE mod, HRSRC res) { (void)mod; (void)res; return nullptr; }
inline void *LockResource(HGLOBAL glob) { (void)glob; return nullptr; }
inline DWORD SizeofResource(HMODULE mod, HRSRC res) { (void)mod; (void)res; return 0; }
#ifndef FindResource
#define FindResource FindResourceA
#endif

// Process/module/version enumeration (PrintModules diagnostics): inert.
#ifndef PROCESS_QUERY_INFORMATION
#define PROCESS_QUERY_INFORMATION 0x0400
#endif
#ifndef PROCESS_VM_READ
#define PROCESS_VM_READ 0x0010
#endif
inline HANDLE OpenProcess(DWORD access, bool inherit, DWORD pid) { (void)access; (void)inherit; (void)pid; return nullptr; }
inline bool CloseHandle(HANDLE h) { (void)h; return true; }
inline HANDLE GetCurrentProcess() { return nullptr; }
inline DWORD GetProcessId(HANDLE h) { (void)h; return 0; }
inline bool EnumProcessModules(HANDLE hProc, HMODULE *mods, DWORD cb, DWORD *needed)
{
	(void)hProc; (void)mods; (void)cb;
	if (needed) *needed = 0;
	return false;
}
inline DWORD GetModuleFileNameExA(HANDLE hProc, HMODULE mod, char *buf, DWORD size)
{
	(void)hProc; (void)mod;
	if (buf && size > 0) buf[0] = '\0';
	return 0;
}
struct MODULEINFO {
	LPVOID lpBaseOfDll;
	DWORD SizeOfImage;
	LPVOID EntryPoint;
};
inline bool GetModuleInformation(HANDLE hProc, HMODULE mod, MODULEINFO *mi, DWORD cb)
{
	(void)hProc; (void)mod; (void)cb;
	if (mi) { mi->lpBaseOfDll = nullptr; mi->SizeOfImage = 0; mi->EntryPoint = nullptr; }
	return false;
}
inline DWORD GetFileVersionInfoSizeA(LPCSTR name, DWORD *handle) { (void)name; if (handle) *handle = 0; return 0; }
inline bool GetFileVersionInfoA(LPCSTR name, DWORD handle, DWORD size, void *data) { (void)name; (void)handle; (void)size; (void)data; return false; }
#ifndef GetFileVersionInfoSize
#define GetFileVersionInfoSize GetFileVersionInfoSizeA
#endif
#ifndef GetFileVersionInfo
#define GetFileVersionInfo GetFileVersionInfoA
#endif
struct VS_FIXEDFILEINFO {
	DWORD dwSignature;
	DWORD dwStrucVersion;
	DWORD dwFileVersionMS;
	DWORD dwFileVersionLS;
	DWORD dwProductVersionMS;
	DWORD dwProductVersionLS;
	DWORD dwFileFlagsMask;
	DWORD dwFileFlags;
	DWORD dwFileOS;
	DWORD dwFileType;
	DWORD dwFileSubtype;
	DWORD dwFileDateMS;
	DWORD dwFileDateLS;
};
inline bool VerQueryValueA(const void *block, LPCSTR sub, void **val, UINT *len) { (void)block; (void)sub; if (val) *val = nullptr; if (len) *len = 0; return false; }

#include <winuser.h>

#endif // ORBITER_MACOS_SHIM_WINDOWS_H
