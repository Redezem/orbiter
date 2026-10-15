// macOS port shim: legacy Direct3D 7 / DirectDraw types referenced by the
// Orbiter engine core headers. The Metal renderer does not use these; they
// exist so engine data structures compile unchanged.

#ifndef ORBITER_MACOS_SHIM_D3D_H
#define ORBITER_MACOS_SHIM_D3D_H

#include <windows.h>

#ifndef FLOAT
using FLOAT = float;
#endif
#ifndef VOID
using VOID = void;
#endif

using D3DVALUE = float;
#ifndef D3DVAL
#define D3DVAL(d) ((D3DVALUE)(d))
#endif
using D3DCOLOR = DWORD;

#ifndef D3DFVF_XYZ
#define D3DFVF_XYZ 0x002
#endif
#ifndef D3DFVF_TEX1
#define D3DFVF_TEX1 0x100
#endif
#ifndef D3DFVF_TEXCOORDSIZE2
#define D3DFVF_TEXCOORDSIZE2(c) (0x20000000UL << (c))
#endif
#ifndef D3DFVF_VERTEX
#define D3DFVF_VERTEX (D3DFVF_XYZ | 0x010)
#endif
#ifndef D3DPT_TRIANGLESTRIP
#define D3DPT_TRIANGLESTRIP 0x005
#endif
#ifndef D3DPT_TRIANGLELIST
#define D3DPT_TRIANGLELIST 0x006
#endif
#ifndef D3DRENDERSTATE_ALPHABLENDENABLE
#define D3DRENDERSTATE_ALPHABLENDENABLE 27
#endif
#ifndef D3DRENDERSTATE_AMBIENT
#define D3DRENDERSTATE_AMBIENT 13
#endif
#ifndef D3DRGBA
#define D3DRGBA(r, g, b, a) \
	((D3DCOLOR)((((int)((r) * 255.0f)) << 16) | (((int)((g) * 255.0f)) << 8) | ((int)((b) * 255.0f))))
#endif

struct D3DVECTOR {
	D3DVALUE x = 0, y = 0, z = 0;
};

struct D3DVERTEX {
	D3DVECTOR position;
	D3DVECTOR normal;
	float tu;
	float tv;
};

struct D3DMATRIX {
	union {
		struct {
			D3DVALUE _11, _12, _13, _14;
			D3DVALUE _21, _22, _23, _24;
			D3DVALUE _31, _32, _33, _34;
			D3DVALUE _41, _42, _43, _44;
		};
		D3DVALUE m[16];
	};
};

struct D3DCOLORVALUE {
	D3DVALUE r = 0, g = 0, b = 0, a = 0;
};

struct D3DMATERIAL7 {
	D3DCOLORVALUE diffuse;
	D3DCOLORVALUE ambient;
	D3DCOLORVALUE emissive;
	D3DCOLORVALUE specular;
	D3DVALUE power;
};

struct DDCOLORKEY {
	DWORD dwColorKeyLow;
	DWORD dwColorKeyHigh;
};

struct DDPIXELFORMAT {
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dwRGBBitCount;
	DWORD dwRBitMask;
	DWORD dwGBitMask;
	DWORD dwBBitMask;
	DWORD dwRGBAlphaBitMask;
};

#endif // ORBITER_MACOS_SHIM_D3D_H
