// macOS port shim: shell helpers referenced by Util.cpp. Path creation on
// macOS is handled by std::filesystem in the platform layer; these stubs
// keep the legacy Win32 code compiling.

#ifndef ORBITER_MACOS_SHIM_SHLOBJ_H
#define ORBITER_MACOS_SHIM_SHLOBJ_H

#include <windows.h>

#ifndef ERROR_SUCCESS
#define ERROR_SUCCESS 0L
#endif

inline int SHCreateDirectoryExA(HWND hwnd, const char *path, void *sa)
{
	(void)hwnd; (void)path; (void)sa;
	return ERROR_SUCCESS;
}
inline int SHCreateDirectoryEx(HWND hwnd, const char *path, void *sa)
{
	(void)hwnd; (void)path; (void)sa;
	return ERROR_SUCCESS;
}

#endif // ORBITER_MACOS_SHIM_SHLOBJ_H
