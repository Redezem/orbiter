#pragma once
#include <SDL3/SDL.h>
#include <array>

namespace core_metal {
// SDL USB physical scancodes are NOT DIK/PC set-1 indices. Zero = unsupported.
unsigned directInputScancode(SDL_Scancode code);

// One SDL window, one main-thread owner. No OS input polling or injection.
// Publishes physical transitions; the DI shim maintains held state and a bounded queue.
class SessionInput {
public:
    using Publish = void (*)(unsigned, bool);
    SessionInput(SDL_WindowID window, Publish publish) : window_(window), publish_(publish) { clear(); }
    ~SessionInput() { clear(); }
    SessionInput(const SessionInput&) = delete;
    SessionInput& operator=(const SessionInput&) = delete;
    void handle(const SDL_Event &event);
    void pump();
    void clear();
    bool quitRequested() const { return quit_; }
    bool held(unsigned dik) const { return dik < keys_.size() && keys_[dik]; }
private:
    SDL_WindowID window_;
    Publish publish_;
    std::array<bool, 256> keys_{};
    bool quit_ = false;
};
}
