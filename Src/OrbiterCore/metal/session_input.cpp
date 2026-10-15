#include "session_input.h"

namespace core_metal {
unsigned directInputScancode(SDL_Scancode code) {
    // DirectInput values from Orbitersdk/include/OrbiterAPI.h OAPI_KEY_*.
    // Physical positions intentionally remain layout-independent, including keypad.
    switch (code) {
#define KEY(sdl, dik) case SDL_SCANCODE_##sdl: return dik
        KEY(ESCAPE,0x01); KEY(1,0x02); KEY(2,0x03); KEY(3,0x04); KEY(4,0x05);
        KEY(5,0x06); KEY(6,0x07); KEY(7,0x08); KEY(8,0x09); KEY(9,0x0a); KEY(0,0x0b);
        KEY(MINUS,0x0c); KEY(EQUALS,0x0d); KEY(BACKSPACE,0x0e); KEY(TAB,0x0f);
        KEY(Q,0x10); KEY(W,0x11); KEY(E,0x12); KEY(R,0x13); KEY(T,0x14);
        KEY(Y,0x15); KEY(U,0x16); KEY(I,0x17); KEY(O,0x18); KEY(P,0x19);
        KEY(LEFTBRACKET,0x1a); KEY(RIGHTBRACKET,0x1b); KEY(RETURN,0x1c); KEY(LCTRL,0x1d);
        KEY(A,0x1e); KEY(S,0x1f); KEY(D,0x20); KEY(F,0x21); KEY(G,0x22);
        KEY(H,0x23); KEY(J,0x24); KEY(K,0x25); KEY(L,0x26); KEY(SEMICOLON,0x27);
        KEY(APOSTROPHE,0x28); KEY(GRAVE,0x29); KEY(LSHIFT,0x2a); KEY(BACKSLASH,0x2b);
        KEY(Z,0x2c); KEY(X,0x2d); KEY(C,0x2e); KEY(V,0x2f); KEY(B,0x30);
        KEY(N,0x31); KEY(M,0x32); KEY(COMMA,0x33); KEY(PERIOD,0x34); KEY(SLASH,0x35);
        KEY(RSHIFT,0x36); KEY(KP_MULTIPLY,0x37); KEY(LALT,0x38); KEY(SPACE,0x39);
        KEY(CAPSLOCK,0x3a); KEY(F1,0x3b); KEY(F2,0x3c); KEY(F3,0x3d); KEY(F4,0x3e);
        KEY(F5,0x3f); KEY(F6,0x40); KEY(F7,0x41); KEY(F8,0x42); KEY(F9,0x43); KEY(F10,0x44);
        KEY(NUMLOCKCLEAR,0x45); KEY(SCROLLLOCK,0x46); KEY(KP_7,0x47); KEY(KP_8,0x48);
        KEY(KP_9,0x49); KEY(KP_MINUS,0x4a); KEY(KP_4,0x4b); KEY(KP_5,0x4c);
        KEY(KP_6,0x4d); KEY(KP_PLUS,0x4e); KEY(KP_1,0x4f); KEY(KP_2,0x50);
        KEY(KP_3,0x51); KEY(KP_0,0x52); KEY(KP_PERIOD,0x53); KEY(NONUSBACKSLASH,0x56);
        KEY(F11,0x57); KEY(F12,0x58); KEY(F13,0x64); KEY(F14,0x65); KEY(F15,0x66);
        KEY(KP_EQUALS,0x8d); KEY(KP_ENTER,0x9c); KEY(RCTRL,0x9d); KEY(KP_DIVIDE,0xb5);
        KEY(PRINTSCREEN,0xb7); KEY(RALT,0xb8); KEY(PAUSE,0xc5); KEY(HOME,0xc7);
        KEY(UP,0xc8); KEY(PAGEUP,0xc9); KEY(LEFT,0xcb); KEY(RIGHT,0xcd);
        KEY(END,0xcf); KEY(DOWN,0xd0); KEY(PAGEDOWN,0xd1); KEY(INSERT,0xd2);
        KEY(DELETE,0xd3); KEY(LGUI,0xdb); KEY(RGUI,0xdc); KEY(APPLICATION,0xdd);
#undef KEY
        default: return 0;
    }
}

void SessionInput::clear() {
    keys_.fill(false);
    // Clear the entire published device, including any previous session's state.
    for (unsigned i=0; i<keys_.size(); ++i) publish_(i, false);
}

void SessionInput::handle(const SDL_Event &e) {
    if (e.type == SDL_EVENT_QUIT) { quit_ = true; clear(); return; }
    if (e.type == SDL_EVENT_WINDOW_FOCUS_LOST || e.type == SDL_EVENT_WINDOW_HIDDEN ||
        e.type == SDL_EVENT_WINDOW_MINIMIZED || e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        if (e.window.windowID != window_) return;
        clear();
        if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) quit_ = true;
        return;
    }
    if (quit_ || (e.type != SDL_EVENT_KEY_DOWN && e.type != SDL_EVENT_KEY_UP) || e.key.windowID != window_) return;
    unsigned dik = directInputScancode(e.key.scancode);
    if (!dik) return;
    bool down = e.type == SDL_EVENT_KEY_DOWN;
    if (keys_[dik] == down) return; // repeats do not produce extra transitions
    keys_[dik] = down;
    publish_(dik, down);
    // Left/right modifier keys are independent physical scancodes; never infer
    // them from global SDL_GetModState or event.key.mod (which can be stale).
}

void SessionInput::pump() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) handle(event);
}
}
