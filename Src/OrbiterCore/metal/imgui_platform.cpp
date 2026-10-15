// Compile exactly the engine's fetched backend, not another ImGui library/context.
// Private capability access is deliberately isolated here: upstream currently has
// no public switch for global mouse polling/capture. A backend upgrade must build
// and exercise this adapter, rather than silently restoring OS side effects.
#include "backends/imgui_impl_sdl3.cpp"
#include "imgui_platform.h"

namespace core_metal {
bool initImGuiPlatform(SDL_Window *window) {
    if (!ImGui_ImplSDL3_InitForMetal(window)) return false;
    auto *bd = ImGui_ImplSDL3_GetBackendData();
    bd->MouseCanUseGlobalState = false;
    bd->MouseCanUseCapture = false;
    bd->MouseCanReportHoveredViewport = false;
    auto &io = ImGui::GetIO();
    io.ConfigFlags &= ~(ImGuiConfigFlags_ViewportsEnable | ImGuiConfigFlags_NavEnableGamepad);
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.ConfigNavMoveSetMousePos = false;
    io.BackendFlags &= ~(ImGuiBackendFlags_PlatformHasViewports | ImGuiBackendFlags_HasParentViewport | ImGuiBackendFlags_HasSetMousePos);
    ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_Manual, nullptr, 0);
    auto &platform = ImGui::GetPlatformIO();
    platform.Platform_GetClipboardTextFn = [](ImGuiContext*) { return ""; };
    platform.Platform_SetClipboardTextFn = [](ImGuiContext*, const char*) {};
    platform.Platform_SetImeDataFn = nullptr; // process-local text events only; no OS IME activation
    platform.Platform_OpenInShellFn = nullptr;
    return true;
}
void processImGuiEvent(const SDL_Event &event) {
    auto *bd = ImGui_ImplSDL3_GetBackendData();
    if (!bd) return;
    // Upstream filters key/text/mouse/window IDs. Clear both consumers on every
    // session loss, including hide/minimize/quit (not handled by upstream).
    bool clear = event.type == SDL_EVENT_QUIT ||
        ((event.type == SDL_EVENT_WINDOW_HIDDEN || event.type == SDL_EVENT_WINDOW_MINIMIZED ||
          event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED || event.type == SDL_EVENT_WINDOW_FOCUS_LOST) && event.window.windowID == bd->WindowID);
    ImGui_ImplSDL3_ProcessEvent(&event);
    if (clear) {
        auto &io=ImGui::GetIO();
        io.AddFocusEvent(false);
        for(int button=0; button<ImGuiMouseButton_COUNT; ++button) io.AddMouseButtonEvent(button,false);
        bd->MouseButtonsDown = 0;
    }
}
}
