// macOS port shim: htmlhelp.h (Windows HTML Help API). Inert on macOS.
#ifndef ORBITER_MACOS_SHIM_HTMLHELP_H
#define ORBITER_MACOS_SHIM_HTMLHELP_H

#include <windows.h>

typedef void *HHASH;
typedef void *HHEVENT;

#ifndef HH_DISPLAY_TOPIC
#define HH_DISPLAY_TOPIC 0x0000
#endif
#ifndef HH_HELP_FINDER
#define HH_HELP_FINDER 0x0000
#endif
#ifndef HH_DISPLAY_TOC
#define HH_DISPLAY_TOC 0x0001
#endif
#ifndef HH_DISPLAY_INDEX
#define HH_DISPLAY_INDEX 0x0002
#endif
#ifndef HH_DISPLAY_SEARCH
#define HH_DISPLAY_SEARCH 0x0003
#endif
#ifndef HH_DISPLAY_TEXT
#define HH_DISPLAY_TEXT 0x0012
#endif

inline HHASH HtmlHelpA(HWND hwnd, LPCSTR file, UINT command, DWORD data)
{
	(void)hwnd; (void)file; (void)command; (void)data;
	return nullptr; // no HTML help viewer on macOS
}
#ifndef HtmlHelp
#define HtmlHelp HtmlHelpA
#endif

#endif // ORBITER_MACOS_SHIM_HTMLHELP_H
