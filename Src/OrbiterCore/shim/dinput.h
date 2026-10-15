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
	long lRz;
	int rglSlider[2];
	DWORD rgdwPOV[4];
	unsigned char rgbButtons[32];
	int rgdwButtons[32];
	int rgdwButtons2[32];
	long lVx;
	long lVy;
	long lVz;
	unsigned char rvx;
	unsigned char rvy;
	unsigned char rvz;
	int rglVelSlider[2];
	DWORD rgdwVelPOV[4];
	int rgdwVelButtons[32];
	int rgdwVelButtons2[32];
	long lFx;
	long lFy;
	long lFz;
	unsigned char rfx;
	unsigned char rfy;
	unsigned char rfz;
	int rglForceSlider[2];
	DWORD rgdwForcePOV[4];
	int rgdwForceButtons[32];
	int rgdwForceButtons2[32];
};

struct DIDEVICEOBJECTDATA {
	DWORD dwOfs;
	DWORD dwData;
	DWORD dwTimeStamp;
	DWORD dwSequence;
	UINT_PTR uUser;
};

using LPCDIDEVICEINSTANCE = const DIDEVICEINSTANCE *;

#ifndef DIK_F3
#define DIK_F3 0x3D
#endif
#ifndef DIK_LSHIFT
#define DIK_LSHIFT 0x2A
#endif
#ifndef DIK_ESCAPE
#define DIK_ESCAPE 0x01
#endif

// DirectInput device object types referenced by Orbiter
#ifndef DIDEVTYPE_DEVICE
#define DIDEVTYPE_DEVICE 1
#endif
#ifndef DIDEVKEYBOARD
#define DIDEVKEYBOARD 2
#endif
#ifndef DIDEVJOYSTICK
#define DIDEVJOYSTICK 1
#endif
#ifndef DI_OK
#define DI_OK 0L
#endif
#ifndef DIERR_NOTACQUIRED
#define DIERR_NOTACQUIRED 0x08000002L
#endif
#ifndef DIERR_INPUTLOST
#define DIERR_INPUTLOST 0x80000001L
#endif

// Inert DirectInput device: reports no data. The macOS app feeds keyboard
// state through SDL events instead (see OrbiterMacApp.mm).
struct IDirectInputDevice8Shim {
	HRESULT GetDeviceState(DWORD size, void *out)
	{
		if (out && size > 0) memset(out, 0, size);
		return DIERR_NOTACQUIRED;
	}
	HRESULT Acquire() { return DIERR_NOTACQUIRED; }
	HRESULT GetDeviceData(DWORD cb, DIDEVICEOBJECTDATA *out, DWORD *items, DWORD flags)
	{
		(void)cb; (void)out; (void)flags;
		if (items) *items = 0;
		return DI_OK;
	}
	HRESULT SetDataFormat(const void *fmt) { (void)fmt; return DI_OK; }
	HRESULT SetProperty(const void *obj, const void *data, DWORD flags)
	{ (void)obj; (void)data; (void)flags; return DI_OK; }
};

using LPDIRECTINPUT8 = ComReleasable *;
using LPDIRECTINPUTDEVICE8 = IDirectInputDevice8Shim *;

#endif // ORBITER_MACOS_SHIM_DINPUT_H
