// macOS DirectInput compatibility subset. The main-thread SDL host publishes
// physical keyboard transitions; held state and bounded buffered reads feed
// the unmodified engine input path. Other device capabilities remain limited.

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

struct DIPROPHEADER {
	DWORD dwSize;
	DWORD dwHeaderSize;
	DWORD dwObj;
	DWORD dwHow;
};
struct DIPROPRANGE {
	DIPROPHEADER diph;
	long lMin;
	long lMax;
};
struct DIPROPDWORD {
	DIPROPHEADER diph;
	DWORD dwData;
};
#ifndef DIPH_BYOFFSET
#define DIPH_BYOFFSET 0
#endif
#ifndef DIPH_BYID
#define DIPH_BYID 1
#endif
#ifndef DIPROP_RANGE
#define DIPROP_RANGE 0x00000001L
#endif
#ifndef DIPROP_DEADZONE
#define DIPROP_DEADZONE 0x00000002L
#endif
#ifndef DIPROP_SATURATION
#define DIPROP_SATURATION 0x00000004L
#endif
#ifndef DIPROP_AXISMODE
#define DIPROP_AXISMODE 0x00000010L
#endif
#ifndef DIJOFS_X
#define DIJOFS_X 0L
#endif
#ifndef DIJOFS_Y
#define DIJOFS_Y 1L
#endif
#ifndef DIJOFS_Z
#define DIJOFS_Z 2L
#endif
#ifndef DIJOFS_RX
#define DIJOFS_RX 3L
#endif
#ifndef DIJOFS_RY
#define DIJOFS_RY 4L
#endif
#ifndef DIJOFS_RZ
#define DIJOFS_RZ 5L
#endif
#ifndef DIJOFS_SLIDER0
#define DIJOFS_SLIDER0 6L
#endif
#ifndef DIJOFS_SLIDER
#define DIJOFS_SLIDER(n) (6L + (n))
#endif
#ifndef DIJOFS_BUTTON0
#define DIJOFS_BUTTON0 8L
#endif
#ifndef DIERR_OTHERAPPHASPRIO
#define DIERR_OTHERAPPHASPRIO 0x80000003L
#endif
#ifndef DIERR_NOTINITIALIZED
#define DIERR_NOTINITIALIZED 0x80000006L
#endif

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
#ifndef DIERR_INVALIDPARAM
#define DIERR_INVALIDPARAM 0x80004005L
#endif
#ifndef DIERR_INPUTLOST
#define DIERR_INPUTLOST 0x80000001L
#endif

// macOS input bridge: the SDL host publishes the keyboard state as a
// DirectInput-style scancode array (DIK_* indices). GetDeviceState below
// copies it, so Orbiter::UserInput() works unchanged.
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif

extern char shim_kbd_state[256];      // published by the macOS host
void shim_set_key(unsigned int scancode, bool down);
// Diagnostic negative control: held state only, deliberately no queued transition.
void shim_set_key_state_only(unsigned int scancode, bool down);

// Supported buffered-keyboard subset; same flag/status values as DirectInput.
#ifndef DIGDD_PEEK
#define DIGDD_PEEK 1u
#endif
#ifndef DI_BUFFEROVERFLOW
#define DI_BUFFEROVERFLOW 1L
#endif

struct IDirectInputDevice8Shim {
	HRESULT GetDeviceState(DWORD size, void *out)
	{
		if (!out || size == 0) return DIERR_INVALIDPARAM;
		if (size > 256) size = 256;
		memcpy(out, shim_kbd_state, size);
		return DI_OK;
	}
	// Single main-thread keyboard backend. Mouse remains explicitly unbuffered.
	explicit IDirectInputDevice8Shim(bool keyboard = true) : keyboard_(keyboard) {}
	HRESULT Acquire();
	HRESULT SetCooperativeLevel(HWND hwnd, DWORD flags) { (void)hwnd; (void)flags; return DI_OK; }
	HRESULT GetDeviceData(DWORD cb, DIDEVICEOBJECTDATA *out, DWORD *items, DWORD flags);
	HRESULT Poll() { return DI_OK; }
	HRESULT Unacquire();
	void Release();
	HRESULT SetDataFormat(const void *fmt) { (void)fmt; return DI_OK; }
	HRESULT SetProperty(DWORD obj, const void *data);
private:
	bool keyboard_;
};

#ifndef DIERR_NOINTERFACE
#define DIERR_NOINTERFACE 0x80004002L
#endif
#ifndef DI8DEVCLASS_GAMECTRL
#define DI8DEVCLASS_GAMECTRL 4
#endif
#ifndef DIEDFL_ATTACHEDONLY
#define DIEDFL_ATTACHEDONLY 0x00000001L
#endif
#ifndef DIENUM_CONTINUE
#define DIENUM_CONTINUE 1
#endif
#ifndef DIENUM_STOP
#define DIENUM_STOP 0
#endif
#ifndef DIPH_DEVICE
#define DIPH_DEVICE 0
#endif
#ifndef DIPROP_BUFFERSIZE
#define DIPROP_BUFFERSIZE 0x00000005L
#endif
#ifndef DISCL_EXCLUSIVE
#define DISCL_EXCLUSIVE 0x00000001L
#endif
#ifndef DISCL_NONEXCLUSIVE
#define DISCL_NONEXCLUSIVE 0x00000002L
#endif
#ifndef DISCL_FOREGROUND
#define DISCL_FOREGROUND 0x00000004L
#endif
#ifndef DISCL_BACKGROUND
#define DISCL_BACKGROUND 0x00000008L
#endif

// Well-known GUIDs (values match the real DInput SDK GUIDs)
#define GUID_SHIM_INIT(a) { (unsigned long)(a), 0, 0, {0,0,0,0,0,0,0,0} }
#ifndef GUID_SysKeyboard
static const GUID GUID_SysKeyboard = GUID_SHIM_INIT(0x05445361);
#endif
#ifndef GUID_SysMouse
static const GUID GUID_SysMouse = GUID_SHIM_INIT(0x05445362);
#endif
#ifndef IID_IDirectInput8
static const GUID IID_IDirectInput8 = GUID_SHIM_INIT(0x05445363);
#endif

// Data formats (inert contents; only pointers are compared)
#ifndef c_dfDIKeyboard
#define c_dfDIKeyboard shim_dfDIKeyboard
extern DIDATAFORMAT shim_dfDIKeyboard;
#endif
#ifndef c_dfDIMouse
#define c_dfDIMouse shim_dfDIMouse
extern DIDATAFORMAT shim_dfDIMouse;
#endif
#ifndef c_dfDIJoystick2
#define c_dfDIJoystick2 shim_dfDIJoystick2
extern DIDATAFORMAT shim_dfDIJoystick2;
#endif
#ifndef DIPROPAXISMODE_ABS
#define DIPROPAXISMODE_ABS 0x00000000L
#endif
#ifndef DIPROPAXISMODE_REL
#define DIPROPAXISMODE_REL 0x00000001L
#endif

typedef int (*DIENUMDEVICESCALLBACK)(LPCDIDEVICEINSTANCE, void *);

// One keyboard plus a distinct limited mouse token; no joystick enumeration.
struct IDirectInputDevice8Shim;
using LPDIRECTINPUTDEVICE8 = IDirectInputDevice8Shim *;
struct DirectInput8Shim : ComReleasable {
	HRESULT CreateDevice(GUID guid, LPDIRECTINPUTDEVICE8 *out, void *outer)
	{ (void)outer; if (!out) return DIERR_INVALIDPARAM;
	  *out = nullptr;
	  static IDirectInputDevice8Shim keyboard(true), mouse(false);
	  if (guid == GUID_SysKeyboard) *out = &keyboard;
	  else if (guid == GUID_SysMouse) *out = &mouse;
	  else return DIERR_NOINTERFACE;
	  return DI_OK; }
	HRESULT EnumDevices(DWORD classMask, DIENUMDEVICESCALLBACK cb, void *ctx, DWORD flags)
	{ (void)classMask; (void)cb; (void)ctx; (void)flags; return DI_OK; }
	HRESULT SetProperty(DWORD obj, const void *data) { (void)obj; (void)data; return DI_OK; }
};

using LPDIRECTINPUT8 = DirectInput8Shim *;
using LPDIRECTINPUTDEVICE8 = IDirectInputDevice8Shim *;

// DirectInput8Create entry point (Di7frame.cpp). Returns the native subset.
inline HRESULT DirectInput8Create(void *hinst, DWORD version, const GUID &iid, void **out, void *outer)
{
	(void)hinst; (void)version; (void)iid; (void)outer;
	static DirectInput8Shim di8;
	*out = &di8;
	return DI_OK;
}

#endif // ORBITER_MACOS_SHIM_DINPUT_H
