// macOS port: upstream LuaInterpreter resolves the gcCore custom-camera
// entry points via the D3D9Client import library, so loading LuaInline on
// Windows requires a graphics client. The native preview has none yet, so
// the port defines its own gcCore custom-camera behaviour inside
// LuaInterpreter: calls fail like a run without the client (SetupCustomCamera
// returns NULL — Lua sees nil; DeleteCustomCamera returns -1 — Lua sees
// false; On/Overlay are no-ops). Replace by binding the real gcCore
// interface when a Metal graphics client exists.
#include "gcCoreAPI.h"

int gcCore::DeleteCustomCamera(CAMERAHANDLE hCam)
{
	return -1;
}

void gcCore::CustomCameraOnOff(CAMERAHANDLE hCam, bool bOn)
{
}

void gcCore::CustomCameraOverlay(CAMERAHANDLE hCam, __gcRenderProc clbk, void* pUser)
{
}

CAMERAHANDLE gcCore::SetupCustomCamera(CAMERAHANDLE hCam, OBJHANDLE hVessel, VECTOR3& vPos, VECTOR3& vDir, VECTOR3& vUp, double dFov, SURFHANDLE hSurf, DWORD dwFlags)
{
	return NULL;
}
