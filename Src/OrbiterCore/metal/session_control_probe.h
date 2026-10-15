#pragma once
// Included by the real-core host after OrbiterAPI.h and session_input.h.
// A test schedule only: never sets engine throttle or substitutes simulation.
namespace core_metal {
class SessionControlProbe {
public:
    explicit SessionControlProbe(SDL_WindowID window) : window_(window) {}
    void before(unsigned frame) {
        // The real engine UI transiently captures keyboard during its first
        // frames. Allow four untouched warmup frames; NEVER override capture.
        switch(frame<4 ? 100 : frame-4) {
            case 1: key(SDL_SCANCODE_LCTRL,true); key(SDL_SCANCODE_KP_PLUS,true); break;
            case 2: break; // held with NO repeat event
            case 3: key(SDL_SCANCODE_KP_PLUS,false); break;
            case 4: key(SDL_SCANCODE_LCTRL,false); key(SDL_SCANCODE_RCTRL,true); key(SDL_SCANCODE_KP_PLUS,true); key(SDL_SCANCODE_KP_PLUS,true,true); break;
            case 5: windowEvent(SDL_EVENT_WINDOW_FOCUS_LOST); break;
            case 6: key(SDL_SCANCODE_LCTRL,true,false,window_+1); key(SDL_SCANCODE_KP_PLUS,true,false,window_+1); break;
            case 7: key(SDL_SCANCODE_LCTRL,true); key(SDL_SCANCODE_KP_PLUS,true); break;
            case 8: windowEvent(SDL_EVENT_WINDOW_HIDDEN); break;
            case 9: key(SDL_SCANCODE_LCTRL,true); key(SDL_SCANCODE_KP_MINUS,true); break;
            case 10: key(SDL_SCANCODE_LCTRL,false); key(SDL_SCANCODE_KP_MINUS,false); break;
            case 11: { SDL_Event e{}; e.type=SDL_EVENT_QUIT; push(e); break; }
        }
        // Real timing, bounded to 15ms/frame; no artificial engine dt or physics.
        SDL_Delay(15);
        before_=oapiGetFocusInterface()->GetThrusterGroupLevel(THGROUP_MAIN);
    }
    void after(unsigned frame, const char *polledKeys) {
        double after=oapiGetFocusInterface()->GetThrusterGroupLevel(THGROUP_MAIN);
        double delta=after-before_;
        const char *expected="stable";
        bool ok=std::abs(delta)<1e-12;
        if(frame==5 || frame==6 || frame==8 || frame==11) { expected="increase"; ok=delta>1e-10; }
        if(frame==13) { expected="decrease"; ok=delta < -1e-10; }
        ok_=ok_ && ok && std::isfinite(after) && after>=0 && after<=1;
        ++frames_;
        printf("[core-metal] session_json={\"frame\":%u,\"before\":%.17g,\"after\":%.17g,\"expected\":\"%s\",\"ok\":%s,\"polled_keys\":[%u,%u,%u,%u]}\n",frame,before_,after,expected,ok ? "true" : "false",
               !!(polledKeys[OAPI_KEY_ADD]&0x80),!!(polledKeys[OAPI_KEY_SUBTRACT]&0x80),!!(polledKeys[OAPI_KEY_LCONTROL]&0x80),!!(polledKeys[OAPI_KEY_RCONTROL]&0x80));
    }
    bool verified() const {
        bool ok=ok_ && frames_==16;
        printf("[core-metal] session input verified=%u frames=%u source=process-local-SDL_PushEvent observer=VESSEL-GetThrusterGroupLevel\n",ok,frames_);
        return ok;
    }
private:
    static void push(SDL_Event &e) { if(!SDL_PushEvent(&e)) throw std::runtime_error(SDL_GetError()); }
    void key(SDL_Scancode code, bool down, bool repeat=false, SDL_WindowID window=0) {
        SDL_Event e{}; e.type=down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        e.key.windowID=window ? window : window_; e.key.scancode=code; e.key.down=down; e.key.repeat=repeat; push(e);
    }
    void windowEvent(SDL_EventType type) { SDL_Event e{}; e.type=type; e.window.windowID=window_; push(e); }
    SDL_WindowID window_;
    double before_=0;
    bool ok_=true;
    unsigned frames_=0;
};
}
