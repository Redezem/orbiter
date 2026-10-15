// macOS port shim: Windows Imaging Component (WIC) types used by
// GraphicsAPI.cpp for legacy image encode/decode. On macOS the Metal
// graphics client implements texture loading and screenshot saving itself
// (clbkLoadTexture / clbkSaveScreenshot); these stubs compile the legacy
// path and fail gracefully at runtime (null interfaces, E_FAIL).

#ifndef ORBITER_MACOS_SHIM_WINCODEC_H
#define ORBITER_MACOS_SHIM_WINCODEC_H

#include <windows.h>

// GUID is defined in windows.h (shared with dinput.h).
#ifndef WICPixelFormatGUID
using WICPixelFormatGUID = GUID;
#endif

inline GUID MakeWicGuid(unsigned long v)
{
	GUID g;
	g.Data1 = v;
	g.Data2 = 0;
	g.Data3 = 0;
	for (int i = 0; i < 8; i++) g.Data4[i] = 0;
	return g;
}

struct WICStub {
	void Release() {}
};

struct IWICImagingFactory : WICStub {
	template <class T> HRESULT CreateFormatConverter(T **pp) { if (pp) *pp = nullptr; return E_FAIL; }
	template <class T> HRESULT CreateBitmapScaler(T **pp) { if (pp) *pp = nullptr; return E_FAIL; }
	template <class T> HRESULT CreateStream(T **pp) { if (pp) *pp = nullptr; return E_FAIL; }
	template <class T> HRESULT CreateDecoderFromStream(void *stream, const void *vendor, int cache, T **pp)
	{ (void)stream; (void)vendor; (void)cache; if (pp) *pp = nullptr; return E_FAIL; }
	template <class T> HRESULT CreateDecoderFromFilename(const void *name, const void *vendor, DWORD access, int cache, T **pp)
	{ (void)name; (void)vendor; (void)access; (void)cache; if (pp) *pp = nullptr; return E_FAIL; }
	template <class T> HRESULT CreateEncoder(const GUID &fmt, const GUID *vendor, T **pp)
	{ (void)fmt; (void)vendor; if (pp) *pp = nullptr; return E_FAIL; }
};

struct IWICBitmapDecoder : WICStub {
	void GetFrameCount(UINT *n) { if (n) *n = 0; }
	template <class T> HRESULT GetFrame(UINT idx, T **pp) { (void)idx; if (pp) *pp = nullptr; return E_FAIL; }
};

struct IWICBitmapFrameDecode : WICStub {
	void GetSize(UINT *w, UINT *h) { if (w) *w = 0; if (h) *h = 0; }
};

struct IWICFormatConverter : WICStub {
	template <class T> HRESULT Initialize(T *src, const GUID &fmt, int dither, void *palette, float alpha, int paletteType)
	{ (void)src; (void)fmt; (void)dither; (void)palette; (void)alpha; (void)paletteType; return E_FAIL; }
};

struct IWICBitmapScaler : WICStub {
	template <class T> HRESULT Initialize(T *src, UINT w, UINT h, int mode)
	{ (void)src; (void)w; (void)h; (void)mode; return E_FAIL; }
	HRESULT CopyPixels(const void *rect, UINT stride, UINT size, void *bits)
	{ (void)rect; (void)stride; (void)size; (void)bits; return E_FAIL; }
};

struct IWICStream : WICStub {
	HRESULT InitializeFromMemory(void *buf, DWORD size) { (void)buf; (void)size; return E_FAIL; }
	HRESULT InitializeFromFilename(const void *name, DWORD access) { (void)name; (void)access; return E_FAIL; }
};

struct IWICBitmapFrameEncode : WICStub {
	template <class T> HRESULT Initialize(T *propBag) { (void)propBag; return E_FAIL; }
	HRESULT SetSize(UINT w, UINT h) { (void)w; (void)h; return E_FAIL; }
	HRESULT SetPixelFormat(GUID *fmt) { (void)fmt; return E_FAIL; }
	HRESULT WritePixels(UINT lineCount, UINT stride, UINT imageSize, const void *imageData)
	{ (void)lineCount; (void)stride; (void)imageSize; (void)imageData; return E_FAIL; }
	HRESULT Commit() { return E_FAIL; }
};

struct PROPBAG2 {
	DWORD dwType;
	DWORD dwItemType;
	DWORD cbName;
	wchar_t *pstrName;
};

struct tagVARIANT {
	unsigned short vt;
	unsigned short wReserved1;
	unsigned short wReserved2;
	unsigned short wReserved3;
	float fltVal;
};
using VARIANT = tagVARIANT;
#ifndef VT_R4
#define VT_R4 4
#endif
inline void VariantInit(VARIANT *v) { if (v) { v->vt = 0; v->fltVal = 0.f; } }

struct IPropertyBag2 : WICStub {
	HRESULT Write(ULONG count, const PROPBAG2 *options, const VARIANT *values)
	{ (void)count; (void)options; (void)values; return E_FAIL; }
};

struct IWICBitmapEncoder : WICStub {
	HRESULT Initialize(IWICStream *stream, int cacheOption) { (void)stream; (void)cacheOption; return E_FAIL; }
	template <class F, class P> HRESULT CreateNewFrame(F **frame, P **propBag)
	{ (void)frame; (void)propBag; return E_FAIL; }
	HRESULT Commit() { return E_FAIL; }
};

struct IWICBitmap : WICStub {};

#ifndef WICDecodeMetadataCacheOnDemand
#define WICDecodeMetadataCacheOnDemand 0
#endif
#ifndef WICBitmapDitherTypeNone
#define WICBitmapDitherTypeNone 0
#endif
#ifndef WICBitmapPaletteTypeCustom
#define WICBitmapPaletteTypeCustom 0
#endif
#ifndef WICBitmapInterpolationModeFant
#define WICBitmapInterpolationModeFant 0
#endif
#ifndef WICBitmapEncoderNoCache
#define WICBitmapEncoderNoCache 0
#endif
#ifndef GENERIC_WRITE
#define GENERIC_WRITE 0x40000000L
#endif
#ifndef ERROR_PATH_NOT_FOUND
#define ERROR_PATH_NOT_FOUND 3L
#endif

#define GUID_WICPixelFormat32bppBGR MakeWicGuid(0x11)
#define GUID_WICPixelFormat24bppBGR MakeWicGuid(0x12)
#define GUID_ContainerFormatBmp MakeWicGuid(0x21)
#define GUID_ContainerFormatPng MakeWicGuid(0x22)
#define GUID_ContainerFormatJpeg MakeWicGuid(0x23)
#define GUID_ContainerFormatTiff MakeWicGuid(0x24)

// COM object creation: unavailable on macOS; report "not available".
#define CLSID_WICImagingFactory MakeWicGuid(0x31)
#ifndef IID_PPV_ARGS
// Expands to two arguments (iid, ppv) like the real Windows macro.
#define IID_PPV_ARGS(pp) static_cast<void *>(0), reinterpret_cast<void **>(pp)
#endif
#ifndef CLSCTX_INPROC_SERVER
#define CLSCTX_INPROC_SERVER 1
#endif
#ifndef GENERIC_READ
#define GENERIC_READ 0x80000000L
#endif

inline HRESULT CoCreateInstance(const GUID &clsid, void *outer, DWORD ctx, void *iid, void **obj)
{
	(void)clsid; (void)outer; (void)ctx; (void)iid;
	if (obj) *obj = nullptr;
	return E_FAIL;
}

#endif // ORBITER_MACOS_SHIM_WINCODEC_H
