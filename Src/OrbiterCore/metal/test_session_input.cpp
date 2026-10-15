#include <windows.h>
#include <dinput.h>
#include "OrbiterAPI.h"
#include "session_input.h"
#include <cstdio>
#include <cstdlib>

static unsigned transitions;
static void publish(unsigned key, bool down) { ++transitions; shim_set_key(key, down); }
static void require(bool value, const char *why) { if (!value) { fprintf(stderr,"FAIL: %s\n",why); std::exit(1); } }
static SDL_Event key(SDL_EventType type, SDL_Scancode code, SDL_WindowID window=17) {
    SDL_Event e{}; e.type=type; e.key.windowID=window; e.key.scancode=code; return e;
}
int main() {
    using namespace core_metal;
    require(directInputScancode(SDL_SCANCODE_KP_PLUS)==OAPI_KEY_ADD,"keypad add DIK");
    require(directInputScancode(SDL_SCANCODE_EQUALS)==OAPI_KEY_EQUALS,"main equals distinct from keypad");
    require(directInputScancode(SDL_SCANCODE_KP_ENTER)==OAPI_KEY_NUMPADENTER,"keypad enter extended DIK");
    require(directInputScancode(SDL_SCANCODE_UP)==OAPI_KEY_UP,"arrow extended DIK");
    require(directInputScancode(SDL_SCANCODE_A)==OAPI_KEY_A,"letter DIK, not SDL index");
    require(directInputScancode(SDL_SCANCODE_LCTRL)==OAPI_KEY_LCONTROL && directInputScancode(SDL_SCANCODE_RCTRL)==OAPI_KEY_RCONTROL,"independent controls");
    require(directInputScancode(SDL_SCANCODE_LSHIFT)==OAPI_KEY_LSHIFT && directInputScancode(SDL_SCANCODE_RSHIFT)==OAPI_KEY_RSHIFT,"independent shifts");
    require(directInputScancode(SDL_SCANCODE_LALT)==OAPI_KEY_LALT && directInputScancode(SDL_SCANCODE_RALT)==OAPI_KEY_RALT,"independent alts");
    require(directInputScancode(SDL_SCANCODE_UNKNOWN)==0 && directInputScancode(SDL_SCANCODE_F24)==0,"unknown and valid unsupported scancodes");
    bool mapped[256]{};
    for(int code=0; code<SDL_SCANCODE_COUNT; ++code) {
        unsigned dik=directInputScancode(static_cast<SDL_Scancode>(code));
        if(!dik) continue;
        require(dik<256 && !mapped[dik],"mapped physical keys cannot alias or exceed keyboard array");
        mapped[dik]=true;
    }
    shim_set_key(OAPI_KEY_A,true);
    {
        SessionInput input(17,publish);
        require(!shim_kbd_state[OAPI_KEY_A],"new session clears stale state");
        auto staleMod=key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_A); staleMod.key.mod=SDL_KMOD_CTRL;
        input.handle(staleMod);
        require(!input.held(OAPI_KEY_LCONTROL) && !input.held(OAPI_KEY_RCONTROL),"stale SDL modifier mask must not invent physical keys");
        input.handle(key(SDL_EVENT_KEY_UP,SDL_SCANCODE_A));
        input.handle(key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_LCTRL));
        input.handle(key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_RCTRL));
        input.handle(key(SDL_EVENT_KEY_UP,SDL_SCANCODE_LCTRL));
        require(!input.held(OAPI_KEY_LCONTROL) && input.held(OAPI_KEY_RCONTROL),"left release keeps right held");
        input.handle(key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_KP_PLUS));
        unsigned before=transitions;
        auto repeat=key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_KP_PLUS); repeat.key.repeat=true; input.handle(repeat);
        require(transitions==before,"repeat idempotent");
        IDirectInputDevice8Shim device;
        char state[256]{};
        require(device.GetDeviceState(256,state)==DI_OK && (state[OAPI_KEY_ADD]&0x80) && (state[OAPI_KEY_RCONTROL]&0x80),"actual DInput device polls published held states");
        input.handle(key(SDL_EVENT_KEY_UP,SDL_SCANCODE_KP_PLUS,18));
        require(input.held(OAPI_KEY_ADD),"foreign-window release ignored");
        input.handle(key(SDL_EVENT_KEY_UP,SDL_SCANCODE_KP_PLUS));
        device.GetDeviceState(256,state);
        require(!state[OAPI_KEY_ADD],"release reaches actual device");
        for(auto type : {SDL_EVENT_WINDOW_FOCUS_LOST,SDL_EVENT_WINDOW_HIDDEN,SDL_EVENT_WINDOW_MINIMIZED}) {
            input.handle(key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_KP_PLUS));
            SDL_Event e{}; e.type=type; e.window.windowID=18; input.handle(e);
            require(input.held(OAPI_KEY_ADD),"foreign focus loss ignored");
            e.window.windowID=17; input.handle(e); device.GetDeviceState(256,state);
            for(char value:state) require(value==0,"focus/hide/minimize clears all device states");
        }
        input.handle(key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_A));
        SDL_Event close{}; close.type=SDL_EVENT_WINDOW_CLOSE_REQUESTED; close.window.windowID=17; input.handle(close);
        require(input.quitRequested() && !input.held(OAPI_KEY_A),"close requests quit and clears");
        input.handle(key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_A));
        require(!input.held(OAPI_KEY_A),"closed session ignores keys");
    }
    IDirectInputDevice8Shim device;
    char state[256]{};
    {
        SessionInput input(17,publish);
        input.handle(key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_A));
        input.handle(key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_RCTRL));
        device.GetDeviceState(256,state);
        require((state[OAPI_KEY_A]&0x80) && (state[OAPI_KEY_RCONTROL]&0x80),"destructor test starts with actual held device state");
    }
    device.GetDeviceState(256,state);
    for(char value:state) require(value==0,"destructor clears held device state");
    {
        SessionInput input(17,publish);
        input.handle(key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_KP_PLUS));
        input.handle(key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_LCTRL));
        device.GetDeviceState(256,state);
        require((state[OAPI_KEY_ADD]&0x80) && (state[OAPI_KEY_LCONTROL]&0x80),"quit test starts with actual held device state");
        SDL_Event quit{}; quit.type=SDL_EVENT_QUIT; input.handle(quit);
        require(input.quitRequested(),"SDL quit");
        device.GetDeviceState(256,state);
        for(char value:state) require(value==0,"SDL quit clears held device state before destruction");
        require(!input.held(OAPI_KEY_ADD) && !input.held(OAPI_KEY_LCONTROL),"SDL quit clears adapter held state");
    }
    puts("PASS: physical DIK mapping; held/up/repeat/left-right modifiers; actual shim device polling; foreign-window isolation; focus/hide/minimize/close/quit/teardown clearing");
}
