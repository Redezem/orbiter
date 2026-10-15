#include <windows.h>
#include <dinput.h>
#include "OrbiterAPI.h"
#include "session_input.h"
#include <cstdio>
#include <cstdlib>
static void check(bool b,const char *s) { if(!b) { fprintf(stderr,"FAIL: %s\n",s); exit(1); } }
static void event(core_metal::SessionInput &input, SDL_EventType type, SDL_Scancode code) {
 SDL_Event e{}; e.type=type; e.key.windowID=17; e.key.scancode=code; input.handle(e);
}
int main() {
 IDirectInputDevice8Shim device;
 DIPROPDWORD prop{{sizeof(DIPROPDWORD),sizeof(DIPROPHEADER),0,DIPH_DEVICE},4};
 check(device.SetProperty(DIPROP_BUFFERSIZE,&prop)==DI_OK,"configure buffer");
 device.Acquire();
 core_metal::SessionInput input(17,shim_set_key);
 DIDEVICEOBJECTDATA data[8]{}; DWORD n=8;
 device.GetDeviceData(sizeof(data[0]),data,&n,0); check(n==0,"initialization must not enqueue 256 releases");
 event(input,SDL_EVENT_KEY_DOWN,SDL_SCANCODE_LCTRL);
 event(input,SDL_EVENT_KEY_DOWN,SDL_SCANCODE_P);
 event(input,SDL_EVENT_KEY_DOWN,SDL_SCANCODE_P);
 n=8; device.GetDeviceData(sizeof(data[0]),data,&n,0);
 check(n==2,"two genuine key transitions, no held repeats");
 check(data[0].dwOfs==OAPI_KEY_LCONTROL && data[1].dwOfs==OAPI_KEY_P && data[1].dwData==0x80,"ordered DIK event payload");
 check(data[1].dwSequence==data[0].dwSequence+1 && data[1].dwTimeStamp>=data[0].dwTimeStamp,"sequence and timestamp");
 event(input,SDL_EVENT_KEY_UP,SDL_SCANCODE_P);
 n=8; device.GetDeviceData(sizeof(data[0]),data,&n,1);
 check(n==1 && data[0].dwData==0,"peek release");
 DWORD seq=data[0].dwSequence;
 n=8; device.GetDeviceData(sizeof(data[0]),nullptr,&n,1); check(n==1,"null peek count");
 n=1; device.GetDeviceData(sizeof(data[0]),data,&n,0); check(n==1 && data[0].dwSequence==seq,"peek did not consume");
 n=8; device.GetDeviceData(sizeof(data[0]),data,&n,0); check(n==0,"drain consumes once");
 // Partial/null reads and zero requests must not accidentally flush the queue.
 shim_set_key(OAPI_KEY_A,true); shim_set_key(OAPI_KEY_A,false);
 n=0; device.GetDeviceData(sizeof(data[0]),nullptr,&n,0); check(n==0,"zero request");
 n=1; device.GetDeviceData(sizeof(data[0]),data,&n,0); check(n==1 && data[0].dwData==0x80,"partial read oldest");
 device.Acquire(); // idempotent Acquire must preserve unread events
 n=8; device.GetDeviceData(sizeof(data[0]),data,&n,0); check(n==1 && data[0].dwOfs==OAPI_KEY_A && data[0].dwData==0,"partial drain and idempotent Acquire");
 char state[256]{}; device.GetDeviceState(256,state); check((state[OAPI_KEY_LCONTROL]&0x80) && !state[OAPI_KEY_P],"held independent of buffered consumption");
 event(input,SDL_EVENT_KEY_DOWN,SDL_SCANCODE_RCTRL);
 event(input,SDL_EVENT_KEY_UP,SDL_SCANCODE_LCTRL);
 SDL_Event focus{}; focus.type=SDL_EVENT_WINDOW_FOCUS_LOST; focus.window.windowID=17; input.handle(focus);
 n=8; device.GetDeviceData(sizeof(data[0]),data,&n,0);
 check(n==3 && data[2].dwOfs==OAPI_KEY_RCONTROL && data[2].dwData==0,"focus clear emits only actual releases");
 device.GetDeviceState(256,state); for(char v:state) check(v==0,"focus clears held");
 for(int i=0;i<6;++i) shim_set_key(OAPI_KEY_P,i%2==0);
 n=8; check(device.GetDeviceData(sizeof(data[0]),data,&n,1)==1 && n==4,"bounded ring reports overflow on peek");
 n=8; check(device.GetDeviceData(sizeof(data[0]),nullptr,&n,0)==1 && n==4,"null drain reports overflow and flushes");
 n=8; check(device.GetDeviceData(sizeof(data[0]),data,&n,0)==DI_OK && n==0,"overflow acknowledged by nonpeek");
 shim_set_key(OAPI_KEY_P,true); device.Unacquire(); device.Acquire();
 n=8; device.GetDeviceData(sizeof(data[0]),data,&n,0); check(n==0,"reacquire clears stale commands");
 shim_set_key(OAPI_KEY_P,false); n=8; device.GetDeviceData(sizeof(data[0]),data,&n,0); check(n==1,"reacquire resumes transitions");
 check(device.GetDeviceData(1,data,&n,0)!=DI_OK,"reject unsupported object stride");
 check(device.GetDeviceData(sizeof(data[0]),data,nullptr,0)!=DI_OK,"reject missing count");
 check(device.GetDeviceData(sizeof(data[0]),data,&n,2)!=DI_OK,"reject unknown flags");
 device.Unacquire();
 n=8; check(device.GetDeviceData(sizeof(data[0]),data,&n,0)==DIERR_NOTACQUIRED && n==0,"unacquired data rejected");
 shim_set_key(OAPI_KEY_A,true); shim_set_key(OAPI_KEY_A,false); device.Acquire();
 n=8; device.GetDeviceData(sizeof(data[0]),data,&n,0); check(n==0,"unacquired transitions not replayed");
 check(device.SetProperty(DIPROP_BUFFERSIZE,&prop)!=DI_OK,"acquired configuration rejected");
 device.Unacquire(); prop.dwData=257;
 check(device.SetProperty(DIPROP_BUFFERSIZE,&prop)!=DI_OK,"oversized buffer rejected, never silently clamped");
 prop.dwData=0; check(device.SetProperty(DIPROP_BUFFERSIZE,&prop)==DI_OK,"disable buffer"); device.Acquire();
 shim_set_key(OAPI_KEY_P,true); n=8; check(device.GetDeviceData(sizeof(data[0]),data,&n,0)!=DI_OK,"unbuffered read explicitly unsupported");
 device.Release(); prop.dwData=4;
 check(device.SetProperty(DIPROP_BUFFERSIZE,&prop)==DI_OK,"release permits clean reconfiguration"); device.Acquire();
 {
  core_metal::SessionInput session(18,shim_set_key);
  SDL_Event e{}; e.type=SDL_EVENT_KEY_DOWN; e.key.windowID=18; e.key.scancode=SDL_SCANCODE_P; session.handle(e);
  n=8; device.GetDeviceData(sizeof(data[0]),data,&n,0); check(n==1 && data[0].dwData==0x80,"fresh session has no stale commands");
 }
 n=8; device.GetDeviceData(sizeof(data[0]),data,&n,0); check(n==1 && data[0].dwOfs==OAPI_KEY_P && data[0].dwData==0,"session destructor emits genuine release only");
 device.Release(); device.GetDeviceState(256,state); for(char v:state) check(v==0,"device release clears held state");
 puts("PASS: native buffered transitions/payload/held/modifiers/focus/peek/count/drain/overflow/reacquire/validation");
}
