#include <windows.h>

#ifndef ORBITER_SHIM_INITCOMMONCONTROLS
#define ORBITER_SHIM_INITCOMMONCONTROLS
inline void InitCommonControls() {}
#endif

// macOS port shim: Win32 common-controls treeview API used by the legacy
// Launchpad tabs (TabExtra/TabModule). Compile-only placeholders — the
// macOS UI path uses ImGui panels, not Win32 treeviews.
#ifndef ORBITER_MACOS_SHIM_COMMCTRL_H2
#define ORBITER_MACOS_SHIM_COMMCTRL_H2

// ---- TreeView types ----
typedef void *HTREEITEM;
#ifndef TVI_ROOT
#define TVI_ROOT ((HTREEITEM)((ptrdiff_t)0xffff0000))
#endif
#ifndef TVI_FIRST
#define TVI_FIRST ((HTREEITEM)((ptrdiff_t)0xffff0001))
#endif
#ifndef TVI_LAST
#define TVI_LAST ((HTREEITEM)((ptrdiff_t)0xffff0002))
#endif
#ifndef TVI_SORT
#define TVI_SORT ((HTREEITEM)((ptrdiff_t)0xffff0003))
#endif

typedef struct TVITEMA {
    HTREEITEM hItem;
    UINT      mask;
    UINT      state;
    UINT      stateMask;
    LPSTR     pszText;
    int       cchTextMax;
    int       iImage;
    int       iSelectedImage;
    int       cChildren;
    LPARAM    lParam;
} TVITEMA, TVITEM, *LPTVITEMA, *LPTVITEM;
typedef TVITEMA TV_ITEM;

typedef struct TV_INSERTSTRUCTA {
    HTREEITEM hParent;
    HTREEITEM hInsertAfter;
    TVITEMA   item;
} TV_INSERTSTRUCTA, TV_INSERTSTRUCT;

// item flags
#ifndef TVIF_HANDLE
#define TVIF_HANDLE 0x0001
#endif
#ifndef TVIF_TEXT
#define TVIF_TEXT 0x0002
#endif
#ifndef TVIF_IMAGE
#define TVIF_IMAGE 0x0004
#endif
#ifndef TVIF_PARAM
#define TVIF_PARAM 0x0008
#endif
#ifndef TVIF_STATE
#define TVIF_STATE 0x0010
#endif
#ifndef TVIF_CHILDREN
#define TVIF_CHILDREN 0x0040
#endif

// treeview styles
#ifndef TVS_NOTOOLTIPS
#define TVS_NOTOOLTIPS 0x0080
#endif
#ifndef TVS_HASLINES
#define TVS_HASLINES 0x0002
#endif
#ifndef TVS_SHOWSELALWAYS
#define TVS_SHOWSELALWAYS 0x0020
#endif

// expand/collapse actions
#ifndef TVE_EXPAND
#define TVE_EXPAND 0x0002
#endif
#ifndef TVE_COLLAPSE
#define TVE_COLLAPSE 0x0001
#endif

// notifications
#ifndef NM_FIRST
#define NM_FIRST 0
#endif
#ifndef NM_LAST
#define NM_LAST 99
#endif
#ifndef TVN_FIRST
#define TVN_FIRST (0 - 400)
#endif
#ifndef TVN_SELCHANGEDW
#define TVN_SELCHANGEDW (TVN_FIRST - 4)
#endif
#ifndef TVN_SELCHANGEDA
#define TVN_SELCHANGEDA (TVN_FIRST - 14)
#endif
#ifndef TVN_SELCHANGED
#ifdef UNICODE
#define TVN_SELCHANGED TVN_SELCHANGEDW
#else
#define TVN_SELCHANGED TVN_SELCHANGEDA
#endif
#endif
#ifndef NM_DBLCLK
#define NM_DBLCLK (NM_FIRST + 3)
#endif

typedef struct TV_NOTIFY {
    NMHDR     hdr;
    HTREEITEM hItem;
    UINT      stateNew;
    UINT      stateOld;
    TVITEMA   itemNew;
    TVITEMA   itemOld;
    POINT     ptDrag;
} TV_NOTIFY, *LPTV_NOTIFY;
typedef TV_NOTIFY NM_TREEVIEW, *LPNM_TREEVIEW;

// TreeView_* helpers (macro form in the real Win32 SDK)
inline HTREEITEM TreeView_InsertItem(HWND tv, TV_INSERTSTRUCT *is)
{ (void)tv; (void)is; return nullptr; }
inline bool TreeView_DeleteItem(HWND tv, HTREEITEM item)
{ (void)tv; (void)item; return true; }
inline bool TreeView_DeleteAllItems(HWND tv) { (void)tv; return true; }
inline HTREEITEM TreeView_GetRoot(HWND tv) { (void)tv; return nullptr; }
inline HTREEITEM TreeView_GetChild(HWND tv, HTREEITEM item)
{ (void)tv; (void)item; return nullptr; }
inline HTREEITEM TreeView_GetSelection(HWND tv) { (void)tv; return nullptr; }
inline bool TreeView_SetItem(HWND tv, TVITEM *item) { (void)tv; (void)item; return true; }
inline bool TreeView_GetItem(HWND tv, TVITEM *item) { (void)tv; (void)item; return false; }
inline bool TreeView_Expand(HWND tv, HTREEITEM item, UINT action)
{ (void)tv; (void)item; (void)action; return true; }
inline HTREEITEM TreeView_SelectItem(HWND tv, HTREEITEM item)
{ (void)tv; (void)item; return nullptr; }
inline HTREEITEM TreeView_GetNextSibling(HWND tv, HTREEITEM item)
{ (void)tv; (void)item; return nullptr; }
inline HTREEITEM TreeView_GetParent(HWND tv, HTREEITEM item)
{ (void)tv; (void)item; return nullptr; }

#endif // ORBITER_MACOS_SHIM_COMMCTRL_H2
