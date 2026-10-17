// Copyright (c) Martin Schweiger
// Licensed under the MIT License
//
// macOS port: graphics-client core interface header used by the Lua script
// stack. The Metal preview host ships no D3D9Client graphics client, so this
// build provides ONLY the surface LuaInterpreter/LuaInline consume: the
// gcCore custom-camera methods plus gcGetCoreInterface(). The engine's
// GetModuleHandle shim (Src/OrbiterCore/shim/windows.h) always returns NULL
// on this host, so the interface stays NULL and Lua custom-camera calls
// return nil - exactly the documented stock behaviour when D3D9Client failed
// to load (gcCore.h: "Camera handle, or NULL if ... the custom camera
// interface is disabled"). When a real Metal graphics client arrives, swap
// this header for the upstream OVP/D3D9Client/gcCore.h binding.

#ifndef ORBITER_MACOS_GC_CORE_API_H
#define ORBITER_MACOS_GC_CORE_API_H

#include "OrbiterAPI.h"
#include "DrawAPI.h"

#ifndef __GC_CORE
#define __GC_CORE

#define gc_interface static

// Upstream gcCore.h declares this at file scope; LuaInterpreter relies on it
// for bare Sketchpad/FVECTOR4/IVECTOR2 references.
using namespace oapi;

typedef void * CAMERAHANDLE;
typedef void (__cdecl* __gcRenderProc)(oapi::Sketchpad *pSkp, void *pParam);

/// Custom-camera overlay registration flags (subset of upstream gcConst.h).
#define RENDERPROC_DELETE          0x0000  ///< Unregister/Remove existing callback
#define RENDERPROC_HUD_1ST         0x0001  ///< Draw under the main HUD
#define RENDERPROC_HUD_2ND         0x0002  ///< Draw over the main HUD
#define RENDERPROC_PLANETARIUM     0x0003  ///< Planetarium-view perspective overlay
#define RENDERPROC_EXTERIOR        0x0005  ///< Exterior-view perspective overlay

class gcCore {
public:
	// Custom camera interface (signatures identical to upstream gcCore.h;
	// upstream declares them on gcCore with gcCore2 : public gcCore, and
	// LuaInterpreter's pCore is a gcCore*).
	gc_interface int DeleteCustomCamera(CAMERAHANDLE hCam);
	gc_interface void CustomCameraOnOff(CAMERAHANDLE hCam, bool bOn);
	gc_interface void CustomCameraOverlay(CAMERAHANDLE hCam, __gcRenderProc clbk, void* pUser);
	gc_interface CAMERAHANDLE SetupCustomCamera(CAMERAHANDLE hCam, OBJHANDLE hVessel, VECTOR3& vPos, VECTOR3& vDir, VECTOR3& vUp, double dFov, SURFHANDLE hSurf, DWORD dwFlags = 0xFF);
};

class gcCore2 : public gcCore {
public:
	// Upstream adds the rest of the client surface here; the preview host
	// has no graphics client, so nothing beyond the Lua custom-camera set is
	// referenced anywhere in this build.
};

// Resolves the graphics client exactly like the upstream header: via the
// host's GetModuleHandle("D3D9Client"), which the shim resolves to NULL on
// this platform. Behaviour is then identical to a stock Orbiter run where
// the graphics client is absent.
inline gcCore2* gcGetCoreInterface()
{
	return NULL;
}

#endif // !__GC_CORE
#endif
