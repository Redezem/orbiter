#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "imgui_platform.h"
#include "session_input.h"
#include <cstdio>
#include <stdexcept>
#include <cstring>
// The engine's ImGui fork deliberately leaves this global to its context owner.
ImGuiContext *GImGui = nullptr; // standalone test process only
static void require(bool value,const char *message) { if(!value) throw std::runtime_error(message); }
static void publish(unsigned,bool) {}
int main() {
 SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP,"1");
 SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER,"1");
 SDL_SetHint(SDL_HINT_MOUSE_AUTO_CAPTURE,"0");
 if(!SDL_Init(SDL_INIT_VIDEO)) return 1;
 SDL_Window *window=SDL_CreateWindow("Hidden platform unit test",640,480,SDL_WINDOW_HIDDEN);
 if(!window) return 1;
 ImGui::CreateContext();
 auto &io=ImGui::GetIO(); io.IniFilename=nullptr;
 unsigned char *pixels; int w,h; io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
 bool ready=false; int result=1;
 try {
  require(core_metal::initImGuiPlatform(window),"init"); ready=true;
  require(SDL_ScreenSaverEnabled(),"screensaver must remain enabled");
  require(!(io.ConfigFlags&ImGuiConfigFlags_ViewportsEnable),"viewports disabled");
  require(!io.ConfigNavMoveSetMousePos && (io.ConfigFlags&ImGuiConfigFlags_NoMouseCursorChange),"no cursor warp/change");
  auto id=SDL_GetWindowID(window);
  core_metal::SessionInput input(id,publish,core_metal::processImGuiEvent);
  input.pump();
  float wheelObserved=0; unsigned textCount=0,textCode=0;
  auto frame=[&] { input.pump(); ImGui_ImplSDL3_NewFrame(); ImGui::NewFrame(); wheelObserved=io.MouseWheel; textCount=io.InputQueueCharacters.Size; textCode=textCount ? io.InputQueueCharacters[0] : 0; ImGui::EndFrame(); require((SDL_GetWindowFlags(window)&SDL_WINDOW_HIDDEN) && !(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS),"hidden/unfocused"); };
  auto push=[&](SDL_Event &e) { require(SDL_PushEvent(&e),"push"); };
  auto key=[&](SDL_WindowID target,bool down) { SDL_Event e{}; e.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP; e.key.windowID=target; e.key.scancode=SDL_SCANCODE_A; e.key.key=SDLK_A; e.key.down=down; push(e); };
  key(id+1,true); frame(); require(!ImGui::IsKeyDown(ImGuiKey_A),"foreign key filtered");
  key(id,true); frame(); require(ImGui::IsKeyDown(ImGuiKey_A) && input.held(0x1e),"key delivered both consumers");
  key(id,false); frame(); require(!ImGui::IsKeyDown(ImGuiKey_A) && !input.held(0x1e),"keyup delivered");
  SDL_Event text{}; text.type=SDL_EVENT_TEXT_INPUT; text.text.windowID=id+1; text.text.text="é"; push(text); frame(); require(textCount==0,"foreign text filtered");
  text.text.windowID=id; push(text); frame(); require(textCount==1 && textCode==0xe9,"UTF8 text delivered");
  SDL_Event mouse{}; mouse.type=SDL_EVENT_MOUSE_MOTION; mouse.motion.windowID=id; mouse.motion.x=123; mouse.motion.y=234; push(mouse);
  frame(); require(io.MousePos.x==123 && io.MousePos.y==234,"mouse motion delivered");
  mouse.motion.windowID=id+1; mouse.motion.x=9; push(mouse); frame(); require(io.MousePos.x==123,"foreign mouse filtered");
  SDL_Event button{}; button.type=SDL_EVENT_MOUSE_BUTTON_DOWN; button.button.windowID=id; button.button.button=SDL_BUTTON_LEFT; button.button.down=true; push(button); frame(); require(io.MouseDown[0],"mouse button delivered");
  button.type=SDL_EVENT_MOUSE_BUTTON_UP; button.button.down=false; push(button); frame(); require(!io.MouseDown[0],"mouse release delivered");
  SDL_Event wheel{}; wheel.type=SDL_EVENT_MOUSE_WHEEL; wheel.wheel.windowID=id; wheel.wheel.y=2; push(wheel); frame(); require(wheelObserved==2,"wheel delivered");
  for(auto type:{SDL_EVENT_WINDOW_FOCUS_LOST,SDL_EVENT_WINDOW_HIDDEN,SDL_EVENT_WINDOW_MINIMIZED,SDL_EVENT_WINDOW_CLOSE_REQUESTED}) {
   key(id,true); button.type=SDL_EVENT_MOUSE_BUTTON_DOWN; button.button.down=true; push(button); frame(); require(ImGui::IsKeyDown(ImGuiKey_A) && io.MouseDown[0],"held before clear");
   SDL_Event e{}; e.type=type; e.window.windowID=id+1; push(e); frame(); require(ImGui::IsKeyDown(ImGuiKey_A),"foreign loss filtered");
   e.window.windowID=id; push(e); frame(); require(!ImGui::IsKeyDown(ImGuiKey_A) && !input.held(0x1e) && !io.MouseDown[0],"loss clears both plus mouse");
  }
  result=0; printf("PASS: hidden SDL backend keys/mouse/wheel/window isolation, release and loss clearing; no OS input injection\n");
 } catch(const std::exception &e) { fprintf(stderr,"FAIL: %s\n",e.what()); }
 if(ready) ImGui_ImplSDL3_Shutdown();
 require(io.BackendPlatformUserData==nullptr,"shutdown before context/window");
 ImGui::DestroyContext(); SDL_DestroyWindow(window); SDL_Quit(); return result;
}
