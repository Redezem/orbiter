#include <windows.h>

// Mouse wheel (VX mouse) — legacy VirtualMouse types referenced by Camera.
#ifndef MOUSEFLAGS
#define MOUSEFLAGS int
#endif

#ifndef MOUSE_ZOOM
#define MOUSE_ZOOM 0x0001
#endif
#ifndef MOUSE_XROTATE
#define MOUSE_XROTATE 0x0002
#endif
#ifndef MOUSE_YROTATE
#define MOUSE_YROTATE 0x0004
#endif
#ifndef MOUSE_PAN
#define MOUSE_PAN 0x0008
#endif
#ifndef MOUSE_XPAN
#define MOUSE_XPAN 0x0010
#endif
#ifndef MOUSE_YPAN
#define MOUSE_YPAN 0x0020
#endif
#ifndef MOUSE_TRACKBALL
#define MOUSE_TRACKBALL 0x0040
#endif
