// macOS port shim: DirectInput types referenced by Di7frame.h. The macOS
// host supplies input via SDL; these types only need to exist for the
// legacy header to parse.

#ifndef ORBITER_MACOS_SHIM_DINPUT_H
#define ORBITER_MACOS_SHIM_DINPUT_H

#include <windows.h>

// GUID comes from windows.h shim.

struct DIDATAFORMAT {
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dwNumFields;
	void *rgDataFormat; // unused on macOS
};

struct DIMOUSESTATE {
	long lX;
	long lY;
	long lZ;
	unsigned char rgbButtons[4];
};

struct DIJOYSTATE {
	long lX;
	long lY;
	long lZ;
	unsigned char rx;
	unsigned char ry;
	unsigned char rz;
	int rglRz;
	int rglSlider[2];
	DWORD rgdwButtons;
	DWORD rgdwButtons2;
};

struct DIDEVICEINSTANCE {
	DWORD dwSize;
	GUID guidInstance;
	GUID guidProduct;
	DWORD dwDevType;
	char tszInstanceName[32];
	char tszProductName[32];
	GUID guidFFEffect;
	DWORD dwFFEffectSize;
	DWORD dwFFDriverVersion;
};

struct DIJOYSTATE2 {
	long lX;
	long lY;
	long lZ;
	unsigned char rx;
	unsigned char ry;
	unsigned char rz;
	int rglRz[4];
	int rgdwSlider[2];
	DWORD rgdwButtons;
	DWORD rgdwButtons2;
};

struct DIDEVICEOBJECTDATA {
	DWORD dwOfs;
	DWORD dwData;
	DWORD dwTimeStamp;
	DWORD dwSequence;
	UINT_PTR uUser;
};

using LPCDIDEVICEINSTANCE = const DIDEVICEINSTANCE *;

using LPDIRECTINPUT8 = ComReleasable *;
using LPDIRECTINPUTDEVICE8 = ComReleasable *;

#endif // ORBITER_MACOS_SHIM_DINPUT_H
