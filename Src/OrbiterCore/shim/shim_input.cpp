// macOS input bridge backing for the DirectInput shim (dinput.h).
// The SDL/Metal host calls shim_set_key() with DirectInput scancodes
// (DIK_*); Orbiter::UserInput() then reads the array via GetDeviceState.
#include <windows.h>
#include <dinput.h>

char shim_kbd_state[256] = {0};

void shim_set_key(unsigned int scancode, bool down)
{
	if (scancode < 256) shim_kbd_state[scancode] = down ? (char)0x80 : 0;
}

DIDATAFORMAT shim_dfDIKeyboard  = { sizeof(DIDATAFORMAT), 0, 0, nullptr };
DIDATAFORMAT shim_dfDIMouse     = { sizeof(DIDATAFORMAT), 0, 0, nullptr };
DIDATAFORMAT shim_dfDIJoystick2 = { sizeof(DIDATAFORMAT), 0, 0, nullptr };
