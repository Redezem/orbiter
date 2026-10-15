// macOS port shim: MSVC <io.h> — file access helpers.
#ifndef ORBITER_MACOS_SHIM_IO_H
#define ORBITER_MACOS_SHIM_IO_H

#include <unistd.h>

#ifndef _access
#define _access(p, m) access((p), (m))
#endif
#ifndef _access_s
inline int _access_s(const char *p, int m) { return access(p, m); }
#endif

#endif // ORBITER_MACOS_SHIM_IO_H
