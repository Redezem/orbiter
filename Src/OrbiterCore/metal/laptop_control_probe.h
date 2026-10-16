#pragma once
// Laptop-keyboard flight-control probe. Test-only process-local SDL events;
// every observation is a real engine public API. Proves physically laptop-
// reachable keys, once bound in the run's keymap.cfg (fixtures/laptop-keymap-
// lines.txt staged ahead of the engine defaults), drive the real engine:
// Vessel::ConsumeBufferedKey -> SetAttMode/ToggleAttMode, polled RCS
// ctrlKeyboard -> ApplyUserAttitudeControls -> thruster group override level,
// and OverrideFullMainThrust. The probe never sets engine state directly and
// never substitutes simulation. Schedule slots are frames 4..15 (four
// untouched warmup frames for the real UI's transient keyboard capture).
#include <SDL3/SDL.h>
#include <cstdio>
#include <cmath>
#include <stdexcept>
namespace core_metal {
class LaptopControlProbe {
public:
    explicit LaptopControlProbe(SDL_WindowID window) : window_(window) {}
    void before(unsigned frame) {
        switch(frame<4 ? 100 : frame-4) {
            case 1: key(SDL_SCANCODE_LCTRL,true); key(SDL_SCANCODE_SLASH,true); break;
            case 2: key(SDL_SCANCODE_SLASH,false); key(SDL_SCANCODE_LCTRL,false); break;
            case 3: key(SDL_SCANCODE_LCTRL,true); key(SDL_SCANCODE_SLASH,true); break;
            case 4: key(SDL_SCANCODE_SLASH,false); key(SDL_SCANCODE_LCTRL,false); break;
            case 5: key(SDL_SCANCODE_SLASH,true); break;
            case 6: key(SDL_SCANCODE_SLASH,false); key(SDL_SCANCODE_DOWN,true); break;
            case 7: break; // DOWN held with NO repeat event
            case 8: key(SDL_SCANCODE_DOWN,false); break;
            case 9: key(SDL_SCANCODE_4,true); break;
            case 10: break; // 4 held with NO repeat event
            case 11: { SDL_Event e{}; e.type=SDL_EVENT_QUIT; push(e); break; }
        }
        SDL_Delay(15);
        attBefore_=oapiGetFocusInterface()->GetAttitudeMode();
        upBefore_=oapiGetFocusInterface()->GetThrusterGroupLevel(THGROUP_ATT_UP);
        mainBefore_=oapiGetFocusInterface()->GetThrusterGroupLevel(THGROUP_MAIN);
    }
    void after(unsigned frame, const char *keys) {
        int att=oapiGetFocusInterface()->GetAttitudeMode();
        double up=oapiGetFocusInterface()->GetThrusterGroupLevel(THGROUP_ATT_UP);
        double main=oapiGetFocusInterface()->GetThrusterGroupLevel(THGROUP_MAIN);
        // Scenario default attmode 1 (Vessel::Read). Ctrl+/ disables RCS
        // (1->0), again re-arms (0->1), then bare / toggles to translational
        // (1->2). Only keydown edges act (Vessel::ConsumeBufferedKey).
        bool attOK = (frame==5 ? att==0&&attBefore_==1 : frame==6 ? att==0 :
                      frame==7 ? att==1&&attBefore_==0 : frame==8 ? att==1 :
                      frame==9 ? att==2&&attBefore_==1 : att==attBefore_);
        // Bare arrow DOWN bound to RCSUp raises the real ATT_UP override level
        // while held in translational mode; the polled-state change lands one
        // engine frame later (press first seen in the frame-10 poll raises the
        // level during frame 11). The level returns to zero one frame after
        // the release poll.
        bool upOK = (frame==11 ? (up>1e-6 && upBefore_<=1e-6) :
                     frame==12 ? up>1e-6 :
                     frame>=13 ? up<=1e-6 : true);
        // Bare digit 4 bound to OverrideFullMainThrust sets the real main
        // level directly (not dt-scaled), one frame after the press poll;
        // throttle is a permanent level, so it holds through frame 15.
        bool mainOK = (frame==14 ? (main>1e-6 && mainBefore_<=1e-6) :
                       frame==15 ? main>=mainBefore_ && main>1e-6 : main<=1e-6);
        // Polled DIK state proves the physical transitions reached the engine
        // device: SLASH 0x35, DOWN 0xd0, digit 4 0x05, LCTRL 0x1d.
        unsigned slash=!!(keys[0x35]&0x80), down=!!(keys[0xd0]&0x80), digit=!!(keys[0x05]&0x80), ctrl=!!(keys[OAPI_KEY_LCONTROL]&0x80);
        static const unsigned want[16][4] = {
            {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
            {1,0,0,1},{0,0,0,0},{1,0,0,1},{0,0,0,0},{1,0,0,0},
            {0,1,0,0},{0,1,0,0},{0,0,0,0},{0,0,1,0},{0,0,1,0},{0,0,0,0}};
        bool pollOK = frame<16 && slash==want[frame][0] && down==want[frame][1] && digit==want[frame][2] && ctrl==want[frame][3];
        bool ok = attOK && upOK && mainOK && pollOK
               && std::isfinite(up) && std::isfinite(main) && up>=0 && up<=1 && main>=0 && main<=1;
        ok_=ok_ && ok; ++frames_;
        printf("[core-metal] laptop_json={\"frame\":%u,\"attmode_before\":%d,\"attmode\":%d,\"up_before\":%.17g,\"up\":%.17g,\"main_before\":%.17g,\"main\":%.17g,\"ok\":%s,\"polled_keys\":[%u,%u,%u,%u]}\n",
               frame,attBefore_,att,upBefore_,up,mainBefore_,main,ok?"true":"false",slash,down,digit,ctrl);
    }
    bool verified() const {
        bool ok=ok_ && frames_==16;
        printf("[core-metal] laptop input verified=%u frames=%u source=process-local-SDL_PushEvent observer=attmode+GetThrusterGroupLevel+polled-DIK\n",ok,frames_);
        return ok;
    }
private:
    static void push(SDL_Event &e) { if(!SDL_PushEvent(&e)) throw std::runtime_error(SDL_GetError()); }
    void key(SDL_Scancode code, bool down) {
        SDL_Event e{}; e.type=down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        e.key.windowID=window_; e.key.scancode=code; e.key.down=down; push(e);
    }
    SDL_WindowID window_;
    int attBefore_=-1;
    double upBefore_=0, mainBefore_=0;
    bool ok_=true;
    unsigned frames_=0;
};
} // namespace core_metal
