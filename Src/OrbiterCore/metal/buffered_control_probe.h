#pragma once
// Test-only process-local events; observations use real engine public APIs.
namespace core_metal {
class BufferedControlProbe {
public:
 explicit BufferedControlProbe(SDL_WindowID window):window_(window) {}
 void before(unsigned frame) {
  switch(frame) {
   case 5: key(SDL_SCANCODE_P,true); break; // unmodified P is NOT Pause
   case 6: key(SDL_SCANCODE_P,false); break;
   case 7: key(SDL_SCANCODE_LCTRL,true); key(SDL_SCANCODE_P,true); break;
   case 8: key(SDL_SCANCODE_P,true,true); break;
   case 10: key(SDL_SCANCODE_P,false); break;
   case 11: key(SDL_SCANCODE_P,true); break;
   case 13: focus(); break;
   case 14: key(SDL_SCANCODE_RCTRL,true); key(SDL_SCANCODE_P,true); break;
   case 15: focus(); break;
   case 16: key(SDL_SCANCODE_RCTRL,true); key(SDL_SCANCODE_P,true); break;
   case 17: key(SDL_SCANCODE_P,false); key(SDL_SCANCODE_RCTRL,false); break;
   case 18: { SDL_Event e{}; e.type=SDL_EVENT_QUIT; push(e); break; }
  }
  SDL_Delay(15);
  beforeTime_=oapiGetSimTime(); beforePause_=oapiGetPause();
 }
 void after(unsigned frame, const char *keys) {
  bool paused=oapiGetPause(); double time=oapiGetSimTime(), delta=time-beforeTime_;
  bool expected=(frame>=8 && frame<=11) || (frame>=15 && frame<=16);
  // Pause requests are applied by NEXT BeginTimeStep, before world/time update.
  bool ok=paused==expected && std::isfinite(time) &&
      (frame==0 || (paused ? delta==0 : delta>0));
  ok_=ok_ && ok; ++frames_;
  printf("[core-metal] buffered_json={\"frame\":%u,\"before_pause\":%s,\"pause\":%s,\"sim_before\":%.17g,\"sim_after\":%.17g,\"ok\":%s,\"polled_keys\":[%u,%u,%u]}\n",
    frame,beforePause_?"true":"false",paused?"true":"false",beforeTime_,time,ok?"true":"false",!!(keys[OAPI_KEY_P]&0x80),!!(keys[OAPI_KEY_LCONTROL]&0x80),!!(keys[OAPI_KEY_RCONTROL]&0x80));
 }
 bool verified() const {
  bool ok=ok_ && frames_==19;
  printf("[core-metal] buffered input verified=%u frames=%u source=process-local-SDL_PushEvent observer=oapiGetPause+oapiGetSimTime\n",ok,frames_);
  return ok;
 }
private:
 static void push(SDL_Event &e) { if(!SDL_PushEvent(&e)) throw std::runtime_error(SDL_GetError()); }
 void key(SDL_Scancode code,bool down,bool repeat=false) {
  SDL_Event e{}; e.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;
  e.key.windowID=window_; e.key.scancode=code; e.key.down=down; e.key.repeat=repeat; push(e);
 }
 void focus() { SDL_Event e{}; e.type=SDL_EVENT_WINDOW_FOCUS_LOST; e.window.windowID=window_; push(e); }
 SDL_WindowID window_; double beforeTime_=0; bool beforePause_=false,ok_=true; unsigned frames_=0;
};
}
