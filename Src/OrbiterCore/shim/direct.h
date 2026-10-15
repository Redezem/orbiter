// macOS port shim: MSVC <direct.h> — directory helpers.
#ifndef ORBITER_MACOS_SHIM_DIRECT_H
#define ORBITER_MACOS_SHIM_DIRECT_H

#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#ifndef _mkdir
#define _mkdir(p) mkdir((p), 0755)
#endif
#ifndef _rmdir
#define _rmdir(p) rmdir(p)
#endif
#ifndef _getcwd
#define _getcwd getcwd
#endif
#ifndef _chdir
#define _chdir chdir
#endif

#endif // ORBITER_MACOS_SHIM_DIRECT_H
