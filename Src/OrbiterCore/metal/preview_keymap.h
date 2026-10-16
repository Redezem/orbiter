#pragma once
// Preview-only keymap composition. The engine keymap parser (Keymap::Read with
// its shipped keyname table) remains the only authority for binding syntax and
// semantics; this header just names the laptop-reachable physical keys and the
// shared seed fixture so a keyboard without a numeric keypad can fly the
// vessel through the real engine. No engine key handling is replaced here.
//
// Included after session_input.h (SDL scancodes) by main.mm.
#include <string>
#include <vector>

namespace core_metal {

// Base keys used by fixtures/laptop-keymap-lines.txt, as SDL physical
// scancodes that SessionInput::directInputScancode publishes. The preview
// publisher allows exactly this set plus keypad keys and modifiers, so no
// binding line can name a key the input pipeline refuses to deliver. The
// fixture and this list are asserted equal by test_preview_keymap.cpp against
// the engine Keymap name table.
inline std::vector<SDL_Scancode> previewLaptopPhysicalKeys() {
    return {
        SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4,
        SDL_SCANCODE_6, SDL_SCANCODE_7, SDL_SCANCODE_9,
        SDL_SCANCODE_H, SDL_SCANCODE_J, SDL_SCANCODE_K, SDL_SCANCODE_L,
        SDL_SCANCODE_SLASH,
        SDL_SCANCODE_HOME, SDL_SCANCODE_UP, SDL_SCANCODE_LEFT,
        SDL_SCANCODE_RIGHT, SDL_SCANCODE_DOWN,
        SDL_SCANCODE_INSERT,
    };
}

// Seed file (staged ahead of the generated engine defaults in keymap.cfg)
// shared by the preview bundle and the hidden laptop-control regression.
inline std::string previewLaptopKeymapFixture() {
    return "fixtures/laptop-keymap-lines.txt";
}

} // namespace core_metal
