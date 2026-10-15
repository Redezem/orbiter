// macOS port shim: MSVC <direct.h> — directory helpers.
#ifndef ORBITER_MACOS_SHIM_DIRECT_H
#define ORBITER_MACOS_SHIM_DIRECT_H

#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

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

// MSVC _fullpath: normalise a path to absolute form. The engine callers use
// it with ".\\" purely to fetch the current directory.
#ifndef _fullpath
static inline char *_fullpath (char *absPath, const char *relPath, size_t maxLength)
{
	if (!relPath || !*relPath || !strcmp (relPath, ".") || !strcmp (relPath, ".\\") || !strcmp (relPath, "./"))
		return getcwd (absPath, maxLength);
	if (relPath[0] == '/')
		return strncpy (absPath, relPath, maxLength) ? absPath : nullptr;
	char base[1024];
	if (!getcwd (base, sizeof base)) return nullptr;
	snprintf (absPath, maxLength, "%s/%s", base, relPath);
	return absPath;
}
#endif

#endif // ORBITER_MACOS_SHIM_DIRECT_H
