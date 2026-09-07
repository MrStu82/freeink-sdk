#pragma once
#include <cstdint>
#include <cstring>

// Observational RAM only, called on the ordinary input/application loop.
// No bus, GPIO, serial, persistence, allocation or controller operations here.
namespace deck_trace {
struct Event {
  uint32_t ms=0; const char* kind=""; int irq=-1,status=-1,x=0,y=0,rawX=0,rawY=0;
  uint8_t raw[8]={}; char screen[32]={};
};
struct State {
  uint32_t started=0,polls=0,errors=0,pointErrors=0,ready=0,notReady=0,irqChanges=0,clears=0,clearErrors=0,ui=0;
  unsigned count=0,dropped=0; int lastIrq=-1,lastStatus=-1; bool armed=false;
  uint8_t address=0; bool enabled=false; Event events[100];
};
inline State state;
inline uint8_t initAddress=0;
inline bool initEnabled=false;
inline char screen[32]="unknown";
inline void setScreen(const char* name){std::strncpy(screen,name,sizeof(screen)-1);screen[31]=0;}
inline void start(uint32_t now) {
  std::memset(&state,0,sizeof(state)); state.lastIrq=-1;state.lastStatus=-1;
  state.started=now; state.armed=true;
  state.address=initAddress;state.enabled=initEnabled;
}
inline bool active(uint32_t now) {
  if(state.armed && uint32_t(now-state.started)>=60000)state.armed=false;
  return state.armed;
}
inline Event* event(uint32_t now,const char* kind) {
  if(!active(now))return nullptr;
  if(state.count==100){++state.dropped;return nullptr;}
  Event& e=state.events[state.count++];e.ms=now-state.started;e.kind=kind;
  std::strncpy(e.screen,screen,sizeof(e.screen)-1);return &e;
}
inline void poll(uint32_t now,int irq) {
  if(!active(now))return;
  ++state.polls;
  if(state.lastIrq!=irq){if(state.lastIrq!=-1)++state.irqChanges;
    if(auto* e=event(now,"irq"))e->irq=irq;}
  state.lastIrq=irq;
}
inline void status(uint32_t now,int value) {
  if(!active(now))return;
  state.lastStatus=value;
  if(value<0){++state.errors;if(auto*e=event(now,"read-error"))e->status=value;}
  else if(value&0x80){++state.ready;if(auto*e=event(now,"ready")){e->status=value;e->irq=state.lastIrq;}}
  else ++state.notReady;
}
inline void pointError(uint32_t now){if(active(now))++state.pointErrors;}
inline void point(uint32_t now,const uint8_t* raw,int rx,int ry,int x,int y) {
  if(auto*e=event(now,"point")){std::memcpy(e->raw,raw,8);e->rawX=rx;e->rawY=ry;e->x=x;e->y=y;}
}
inline void clear(uint32_t now,int result) {
  if(!active(now))return;++state.clears;if(result)++state.clearErrors;
}
inline void dispatch(uint32_t now,const char* kind,int x,int y) {
  if(!active(now))return;++state.ui;if(auto*e=event(now,kind)){e->x=x;e->y=y;}
}
}
