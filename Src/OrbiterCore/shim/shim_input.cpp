// One main-thread native keyboard, matching the current host ownership model.
#include <windows.h>
#include <dinput.h>
#include <array>
#include <chrono>
#include <algorithm>

char shim_kbd_state[256] = {0};
namespace {
constexpr DWORD maxEvents = 256;
std::array<DIDEVICEOBJECTDATA, maxEvents> events{};
DWORD capacity=0, head=0, count=0, sequence=0;
bool acquired=false, overflow=false;
void flush() { head=count=0; overflow=false; }
}
void shim_set_key_state_only(unsigned scancode, bool down) {
    if(scancode<256) shim_kbd_state[scancode]=down ? (char)0x80 : 0;
}
void shim_set_key(unsigned scancode, bool down) {
    if(scancode>=256 || !!(shim_kbd_state[scancode]&0x80)==down) return;
    shim_set_key_state_only(scancode,down);
    if(!acquired || !capacity) return;
    const DWORD serial=++sequence;
    if(count==capacity) { overflow=true; return; } // preserve oldest unread events
    auto millis=std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    events[(head+count)%capacity]={scancode,down ? 0x80u : 0u,static_cast<DWORD>(millis),serial,0};
    ++count;
}
HRESULT IDirectInputDevice8Shim::Acquire() { if(keyboard_) acquired=true; return DI_OK; }
HRESULT IDirectInputDevice8Shim::Unacquire() {
    if(keyboard_) { acquired=false; flush(); }
    return DI_OK;
}
void IDirectInputDevice8Shim::Release() {
    if(keyboard_) { Unacquire(); capacity=0; memset(shim_kbd_state,0,256); }
}
HRESULT IDirectInputDevice8Shim::SetProperty(DWORD obj,const void *data) {
    if(!keyboard_ || obj!=DIPROP_BUFFERSIZE || !data) return DIERR_INVALIDPARAM;
    auto *p=static_cast<const DIPROPDWORD*>(data);
    if(acquired || p->diph.dwSize!=sizeof(DIPROPDWORD) || p->diph.dwHeaderSize!=sizeof(DIPROPHEADER) ||
       p->diph.dwObj!=0 || p->diph.dwHow!=DIPH_DEVICE || p->dwData>maxEvents) return DIERR_INVALIDPARAM;
    capacity=p->dwData; flush(); return DI_OK;
}
HRESULT IDirectInputDevice8Shim::GetDeviceData(DWORD cb,DIDEVICEOBJECTDATA *out,DWORD *items,DWORD flags) {
    if(!items || cb!=sizeof(DIDEVICEOBJECTDATA) || (flags & ~DIGDD_PEEK)) return DIERR_INVALIDPARAM;
    DWORD requested=*items; *items=0;
    if(!keyboard_ || !capacity) return DIERR_INVALIDPARAM; // unbuffered device
    if(!acquired) return DIERR_NOTACQUIRED;
    DWORD n=std::min(requested,count);
    for(DWORD i=0;out && i<n;++i) out[i]=events[(head+i)%capacity];
    *items=n;
    HRESULT status=overflow ? DI_BUFFEROVERFLOW : DI_OK; // DirectInput DI_BUFFEROVERFLOW = S_FALSE
    if(!(flags&DIGDD_PEEK)) { head=(head+n)%capacity; count-=n; overflow=false; }
    return status;
}
DIDATAFORMAT shim_dfDIKeyboard  = { sizeof(DIDATAFORMAT), 0, 0, nullptr };
DIDATAFORMAT shim_dfDIMouse     = { sizeof(DIDATAFORMAT), 0, 0, nullptr };
DIDATAFORMAT shim_dfDIJoystick2 = { sizeof(DIDATAFORMAT), 0, 0, nullptr };
