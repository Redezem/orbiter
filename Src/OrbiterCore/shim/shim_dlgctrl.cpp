// macOS port fallback for the Win32 custom-control library (Src/DlgCtrl).
//
// oapiRegisterCustomControls / oapiSetGauge* back the gauge, switch and
// property-list controls used by the Windows dialog and scenario-editor UI.
// The native port has no Win32 dialogs (its UI layer is ImGui), so the
// module-facing entry points declared in Orbitersdk/include/DlgCtrl.h are
// provided here as inert fallbacks. Vessel modules that touch them only do
// so from Windows editor callbacks, which never run in the native session.

#include <windows.h>
#include "DlgCtrl.h"

void oapiRegisterCustomControls (HINSTANCE hInst)
{
	(void)hInst;
}

void oapiUnregisterCustomControls (HINSTANCE hInst)
{
	(void)hInst;
}

void oapiSetGaugeParams (HWND hCtrl, GAUGEPARAM *gp, bool redraw)
{
	(void)hCtrl; (void)gp; (void)redraw;
}

int oapiSetGaugePos (HWND hCtrl, int pos, bool redraw)
{
	(void)hCtrl; (void)pos; (void)redraw;
	return 0;
}

void oapiSetGaugeRange (HWND hCtrl, int rmin, int rmax, bool redraw)
{
	(void)hCtrl; (void)rmin; (void)rmax; (void)redraw;
}

int oapiIncGaugePos (HWND hCtrl, int dpos, bool redraw)
{
	(void)hCtrl; (void)dpos; (void)redraw;
	return 0;
}

int oapiGetGaugePos (HWND hCtrl)
{
	(void)hCtrl;
	return 0;
}

void oapiSetSwitchParams (HWND hCtrl, SWITCHPARAM *sp, bool redraw)
{
	(void)hCtrl; (void)sp; (void)redraw;
}

int oapiSetSwitchState (HWND hCtrl, int state, bool redraw)
{
	(void)hCtrl; (void)state; (void)redraw;
	return 0;
}

int oapiGetSwitchState (HWND hCtrl)
{
	(void)hCtrl;
	return 0;
}

// Legacy ABI placeholder declared in OrbiterAPI.h
void dummy() {}
