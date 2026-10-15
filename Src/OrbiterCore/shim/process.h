// macOS port shim: MSVC <process.h>.
#ifndef ORBITER_MACOS_SHIM_PROCESS_H
#define ORBITER_MACOS_SHIM_PROCESS_H

#include <unistd.h>
#include <process.h>

#ifndef _getpid
#define _getpid getpid
#endif

// Orbiter relaunches itself after graphics-client shutdown on Windows.
// On macOS the app lifecycle is managed by the host; relaunch is disabled.
inline int _execl(const char *path, ...) { (void)path; return -1; }

#endif // ORBITER_MACOS_SHIM_PROCESS_H
