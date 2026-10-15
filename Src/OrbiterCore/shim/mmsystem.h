#include <windows.h>

// TIME_KILL_SYNCHRONOUS / timeGetTime etc. — timing already in windows.h.
#ifndef TIME_KILL_SYNCHRONOUS
#define TIME_KILL_SYNCHRONOUS 0x01
#endif
