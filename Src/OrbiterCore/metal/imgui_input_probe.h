#pragma once
#include "imgui_internal.h"
namespace core_metal {
// Explicit diagnostic widget inserted into the engine-owned frame. No context,
// NewFrame/Render calls, capture overrides or engine control setters of its own.
class ImGuiInputProbe {
public:
 explicit ImGuiInputProbe(SDL_WindowID window):window_(window) {
  ImGuiContextHook hook{}; hook.Type=ImGuiContextHookType_EndFramePre;
  hook.UserData=this; hook.Callback=[](ImGuiContext*, ImGuiContextHook *h) { static_cast<ImGuiInputProbe*>(h->UserData)->widget(); };
  hook_=ImGui::AddContextHook(ImGui::GetCurrentContext(),&hook);
 }
 ~ImGuiInputProbe() { ImGui::RemoveContextHook(ImGui::GetCurrentContext(),hook_); }
 void widget() {
  if(frame_<6 || frame_>13) return;
  ImGui::SetNextWindowPos(ImVec2(20,80),ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(300,100),ImGuiCond_Always);
  ImGui::Begin("Process-local input diagnostic",nullptr,ImGuiWindowFlags_NoSavedSettings);
  if(frame_==6) ImGui::SetKeyboardFocusHere();
  ImGui::InputText("Text",text_,sizeof(text_));
  active_=ImGui::IsItemActive();
  ImGui::End();
 }
 void before(unsigned frame) {
  frame_=frame; active_=false;
  switch(frame) {
   case 5: key(SDL_SCANCODE_LCTRL,true); key(SDL_SCANCODE_KP_PLUS,true); break;
   case 7: key(SDL_SCANCODE_KP_PLUS,false); key(SDL_SCANCODE_LCTRL,false); break;
   case 8: text("orbit"); break;
   case 9: key(SDL_SCANCODE_BACKSPACE,true); break;
   case 10: key(SDL_SCANCODE_BACKSPACE,false); break;
   case 11: text("t"); break;
   case 12: key(SDL_SCANCODE_LCTRL,true); key(SDL_SCANCODE_KP_PLUS,true); key(SDL_SCANCODE_P,true); break;
   case 14: focus(); break;
   case 17: key(SDL_SCANCODE_LCTRL,true); key(SDL_SCANCODE_KP_MINUS,true); key(SDL_SCANCODE_P,true); break;
   case 18: key(SDL_SCANCODE_KP_MINUS,false); key(SDL_SCANCODE_P,false); break;
   case 19: key(SDL_SCANCODE_P,true); break;
   case 20: focus(); break;
   case 21: { SDL_Event e{}; e.type=SDL_EVENT_QUIT; push(e); break; }
  }
  SDL_Delay(15);
  throttle_=oapiGetFocusInterface()->GetThrusterGroupLevel(THGROUP_MAIN);
  capture_=ImGui::GetIO().WantCaptureKeyboard;
 }
 void beforeEngine() {
  capture_=ImGui::GetIO().WantCaptureKeyboard;
  std::copy(shim_kbd_state,shim_kbd_state+256,raw_);
 }
 void after(unsigned frame,const char *keys) {
  double after=oapiGetFocusInterface()->GetThrusterGroupLevel(THGROUP_MAIN),delta=after-throttle_;
  bool ok=std::isfinite(after);
  if(frame==5 || frame==6) ok=ok && !capture_ && delta>1e-10;
  else if(frame==17) ok=ok && !capture_ && delta < -1e-10;
  else ok=ok && std::abs(delta)<1e-12;
  if(frame==12 || frame==13) ok=ok && capture_ && !(keys[OAPI_KEY_ADD]&0x80) && !(keys[OAPI_KEY_P]&0x80);
  if(frame==10) ok=ok && std::string(text_)=="orbi";
  if(frame>=11) ok=ok && std::string(text_)=="orbit";
  bool pause=oapiGetPause();
  ok=ok && pause==(frame>=18 && frame<=19);
  if(frame>=14 && frame<=16) ok=ok && !ImGui::IsKeyDown(ImGuiKey_KeypadAdd) && !ImGui::IsKeyDown(ImGuiKey_P);
  if(frame==12 || frame==13) ok=ok && (raw_[OAPI_KEY_ADD]&0x80) && (raw_[OAPI_KEY_P]&0x80);
  if(frame==14 || frame==15 || frame==16 || frame==20 || frame==21)
   for(unsigned char key:raw_) ok=ok && key==0;
  if(frame==10) ok=ok && !ImGui::IsKeyDown(ImGuiKey_Backspace);
  ok_=ok_ && ok; ++frames_;
  printf("[core-metal] imgui_keys_json={\"frame\":%u,\"raw_add\":%u,\"raw_p\":%u,\"engine_add\":%u,\"engine_p\":%u,\"gui_add\":%s,\"gui_p\":%s}\n",frame,!!(raw_[OAPI_KEY_ADD]&0x80),!!(raw_[OAPI_KEY_P]&0x80),!!(keys[OAPI_KEY_ADD]&0x80),!!(keys[OAPI_KEY_P]&0x80),ImGui::IsKeyDown(ImGuiKey_KeypadAdd)?"true":"false",ImGui::IsKeyDown(ImGuiKey_P)?"true":"false");
  printf("[core-metal] imgui_input_json={\"frame\":%u,\"text\":\"%s\",\"active\":%s,\"capture_before\":%s,\"capture_after\":%s,\"before\":%.17g,\"after\":%.17g,\"pause\":%s,\"ok\":%s}\n",frame,text_,active_?"true":"false",capture_?"true":"false",ImGui::GetIO().WantCaptureKeyboard?"true":"false",throttle_,after,pause?"true":"false",ok?"true":"false");
 }
 bool verified() const { bool ok=ok_ && frames_==22; printf("[core-metal] imgui input verified=%u frames=%u\n",ok,frames_); return ok; }
private:
 static void push(SDL_Event &e) { if(!SDL_PushEvent(&e)) throw std::runtime_error(SDL_GetError()); }
 void key(SDL_Scancode code,bool down) { SDL_Event e{}; e.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP; e.key.windowID=window_; e.key.scancode=code; e.key.key=SDL_GetKeyFromScancode(code,SDL_KMOD_NONE,false); e.key.down=down; e.key.mod=(frame_==12 || frame_==17 || frame_==19)?SDL_KMOD_LCTRL:SDL_KMOD_NONE; push(e); }
 void text(const char *s) { SDL_Event e{}; e.type=SDL_EVENT_TEXT_INPUT; e.text.windowID=window_; e.text.text=s; push(e); }
 void focus() { SDL_Event e{}; e.type=SDL_EVENT_WINDOW_FOCUS_LOST; e.window.windowID=window_; push(e); }
 SDL_WindowID window_; ImGuiID hook_=0; unsigned frame_=0,frames_=0; char text_[64]{};
 unsigned char raw_[256]{};
 bool active_=false,capture_=false,ok_=true; double throttle_=0;
};
}
