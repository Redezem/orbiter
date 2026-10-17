#pragma once
// Test-only process-local events; observations use real engine public APIs.
// Reproduces the preview lifecycle boundedly: the session starts paused
// (the host applies Pause(true) like the preview does), a real Ctrl+P
// buffered command unpauses it, and the sim then runs unpaused for the
// remaining frames. Observables: real oapiGetPause() transitions and real
// sim-time advance only while unpaused. The companion regression separately
// asserts the engine's own log stays free of module-load failures during the
// unpaused window.
namespace core_metal {
class UnpauseControlProbe {
public:
    explicit UnpauseControlProbe(SDL_WindowID window) : window_(window) {}
    void before(unsigned frame) {
        // Warm up four untouched frames like the other probes, then a real
        // Ctrl+P buffered pause-exit command. The engine applies the request
        // at the NEXT BeginTimeStep.
        switch (frame < 4 ? 100 : frame - 4) {
            case 1: key(SDL_SCANCODE_LCTRL, true); key(SDL_SCANCODE_P, true); break;
            case 3: key(SDL_SCANCODE_P, false); key(SDL_SCANCODE_LCTRL, false); break;
            case 25: { SDL_Event e{}; e.type = SDL_EVENT_QUIT; push(e); break; }
        }
        SDL_Delay(15);
        beforePause_ = oapiGetPause();
        beforeTime_ = oapiGetSimTime();
    }
    void after(unsigned frame, const char *keys) {
        bool paused = oapiGetPause();
        double time = oapiGetSimTime(), delta = time - beforeTime_;
        // Request issued at schedule step 1 (frame 5); applied by the next
        // BeginTimeStep, so frame 6 onward must be running.
        bool wantPaused = frame < 6;
        bool ok = paused == wantPaused && std::isfinite(time) &&
                  (frame == 0 || (paused ? delta == 0 : delta > 0));
        if (frame == 5) ok = ok && beforePause_; // still paused entering the toggle frame
        ok_ = ok_ && ok;
        ++frames_;
        printf("[core-metal] unpause_json={\"frame\":%u,\"pause\":%s,\"sim\":%.17g,\"advance\":%.17g,\"ok\":%s,\"polled_keys\":[%u,%u]}\n",
               frame, paused ? "true" : "false", time, delta, ok ? "true" : "false",
               !!(keys[OAPI_KEY_P] & 0x80), !!(keys[OAPI_KEY_LCONTROL] & 0x80));
    }
    bool verified() const {
        bool ok = ok_ && frames_ == FRAMES && !oapiGetPause();
        printf("[core-metal] unpause verified=%u frames=%u final_pause=%u source=process-local-SDL_PushEvent observer=oapiGetPause+oapiGetSimTime\n",
               ok, frames_, oapiGetPause() ? 1u : 0u);
        return ok;
    }
    static constexpr unsigned FRAMES = 30;

private:
    static void push(SDL_Event &e) { if (!SDL_PushEvent(&e)) throw std::runtime_error(SDL_GetError()); }
    void key(SDL_Scancode code, bool down) {
        SDL_Event e{}; e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        e.key.windowID = window_; e.key.scancode = code; e.key.down = down; push(e);
    }
    SDL_WindowID window_;
    double beforeTime_ = 0, unpausedTime_ = 0;
    bool beforePause_ = false, ok_ = true;
    unsigned frames_ = 0;
};
}
