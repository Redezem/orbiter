// Actual production GearVisual with a minimal CPU engine seam; not a live engine
// mutation test. Only this fixture's owned arrays are modified for rejection tests.
#include <array>
#include <vector>
#include <memory>
#include <string>
#include <cstdio>
#include <cassert>
#include <cstdint>
#include <stdexcept>
using WORD=uint16_t; using DWORD=uint32_t;
struct VECTOR3 { double x=0,y=0,z=0; };
struct NTVERTEX {float x=0,y=0,z=0,nx=0,ny=1,nz=0,tu=0,tv=0;};
struct MGROUP_TRANSFORM { enum {ROTATE=1}; unsigned mesh=0; virtual int Type()const{return ROTATE;} virtual ~MGROUP_TRANSFORM()=default; };
struct MGROUP_ROTATE:MGROUP_TRANSFORM {VECTOR3 ref,axis; double angle=0;unsigned *grp=nullptr,ngrp=0;};
struct ANIMATIONCOMP {double state0=0,state1=1; MGROUP_TRANSFORM* trans=nullptr; ANIMATIONCOMP* parent=nullptr;unsigned nchildren=0;};
struct ANIMATION {double defstate=1,state=.25;unsigned ncomp=14;ANIMATIONCOMP** comp=nullptr;};
struct GROUP {NTVERTEX *Vtx=nullptr;WORD* Idx=nullptr;unsigned nVtx=0,nIdx=0;DWORD UsrFlag=0,MtrlIdx=0,TexIdx=0;};
struct Mesh {std::vector<GROUP> groups;};using MESHHANDLE=Mesh*;
unsigned oapiMeshGroupCount(MESHHANDLE m){return unsigned(m->groups.size());}
GROUP* oapiMeshGroup(MESHHANDLE m,unsigned i){return &m->groups.at(i);}
struct VESSEL { Mesh mesh; ANIMATION animation;VECTOR3 offset;unsigned slots=1;
 unsigned setters=0, failSetters=0, throwSetters=0; bool ignoreSetter=false;
 MESHHANDLE GetMeshTemplate(unsigned){return &mesh;}
 unsigned GetAnimPtr(ANIMATION**p){*p=slots ? &animation : nullptr;return slots;}
 bool GetMeshOffset(unsigned,VECTOR3 &p){p=offset;return true;}
 double GetAnimation(unsigned){return animation.state;}
 bool SetAnimation(unsigned,double state){++setters;if(throwSetters){--throwSetters;throw std::runtime_error("transient setter exception");}if(failSetters){--failSetters;return false;}if(!ignoreSetter)animation.state=state;return true;}
};
VESSEL *focus=nullptr;VESSEL* oapiGetFocusInterface(){return focus;}
#include "gear_visual.h"
using core_metal::GearVisual;
struct Fixture {
 VESSEL vessel;std::vector<MGROUP_ROTATE> rotations;std::vector<ANIMATIONCOMP> comps;std::vector<ANIMATIONCOMP*> ptrs;std::vector<std::vector<unsigned>> groups;std::vector<NTVERTEX> vertices;std::vector<WORD> indices;
 Fixture(){auto defs=GearVisual::signature();rotations.resize(14);comps.resize(14);ptrs.resize(14);groups.resize(14);vertices.resize(122);indices.resize(122);vessel.mesh.groups.resize(122);
 for(unsigned i=0;i<14;++i){auto &d=defs[i];groups[i]=d.groups;auto &r=rotations[i];r.ref={d.pivot[0],d.pivot[1],d.pivot[2]};r.axis={d.axis[0],d.axis[1],d.axis[2]};r.angle=d.angle;r.grp=groups[i].data();r.ngrp=unsigned(groups[i].size());comps[i]={d.start,d.end,&r,nullptr,0};ptrs[i]=&comps[i];}
 vessel.animation.comp=ptrs.data();for(unsigned i=0;i<122;++i) vessel.mesh.groups[i]={&vertices[i],&indices[i],1,1,0,0,0};focus=&vessel;}
};
template<class F> void rejects(F f){bool rejected=false;try{f();}catch(const std::runtime_error&){rejected=true;}assert(rejected);}
int main(){
 {Fixture f;GearVisual v;v.attach(&f.vessel);auto token=v.handle;auto definitions=v.definitions;
 auto replacement=f.ptrs;f.vessel.animation.comp=replacement.data();v.validate(&f.vessel);assert(v.definitions==definitions);
 f.vessel.SetAnimation(0,1);v.detach();assert(f.vessel.GetAnimation(0)==.25);assert(!v.handles.valid(token));assert(!v.handle);auto calls=f.vessel.setters;v.detach();assert(f.vessel.setters==calls);}
 // Geometry rejection must not prevent safe animation-identity-based restoration.
 {Fixture f;GearVisual v;v.attach(&f.vessel);auto token=v.handle;f.vessel.SetAnimation(0,1);f.vertices[0].x=1;
 rejects([&]{v.validate(&f.vessel);});
 assert(!v.detach());assert(!v.restorationPending());
 assert(!v.handles.valid(token)&&!v.handle);assert(f.vessel.GetAnimation(0)==.25);
 f.vertices[0].x=0;v.detach();assert(f.vessel.GetAnimation(0)==.25);}
 puts("PASS geometry rejection records failure but safely restores original animation");
 for(unsigned mode=0;mode<3;++mode){Fixture f;GearVisual v;v.attach(&f.vessel);auto token=v.handle;f.vessel.SetAnimation(0,1);
 f.vessel.failSetters=mode==0;f.vessel.throwSetters=mode==1;f.vessel.ignoreSetter=mode==2;
 assert(!v.detach());assert(v.restorationPending());assert(!v.handles.valid(token)&&!v.handle);assert(f.vessel.animation.state==1);
 f.vessel.ignoreSetter=false;assert(v.detach());assert(!v.restorationPending());assert(f.vessel.animation.state==.25);auto calls=f.vessel.setters;v.detach();assert(f.vessel.setters==calls);}
 puts("PASS false/throwing/inert setter restoration retries after token retirement");
 // Removed or redefined slots must never receive the old state's setter.
 for(unsigned mode=0;mode<3;++mode){Fixture f;GearVisual v;v.attach(&f.vessel);f.vessel.SetAnimation(0,1);auto calls=f.vessel.setters;
 if(mode==0) f.vessel.slots=0;
 if(mode==1) f.vessel.animation.ncomp=0;
 if(mode==2) f.rotations[0].angle+=.01;
 assert(!v.detach());assert(v.restorationPending());assert(f.vessel.setters==calls && f.vessel.animation.state==1 && !v.handle);
 f.vessel.slots=1;f.vessel.animation.ncomp=14;f.rotations[0].angle=GearVisual::signature()[0].angle;
 v.detach();assert(f.vessel.animation.state==.25);}
 puts("PASS deleted/redefined IDs rejected without setter; repaired definition can retry");
 {Fixture f;GearVisual v;v.attach(&f.vessel);f.vessel.SetAnimation(0,1);auto calls=f.vessel.setters;focus=nullptr;
 assert(!v.detach());assert(f.vessel.setters==calls);focus=&f.vessel;assert(v.detach());assert(f.vessel.animation.state==.25);}
 puts("PASS missing owner rejected and retryable");
 {Fixture f;GearVisual v;v.attach(&f.vessel);f.vessel.SetAnimation(0,1);f.vertices[0].x=1;f.vessel.failSetters=1;
 assert(!v.detach());assert(v.restorationPending() && f.vessel.animation.state==1);
 f.vertices[0].x=0;assert(v.detach());assert(!v.restorationPending() && f.vessel.animation.state==.25);}
 puts("PASS invalid geometry plus setter rejection, exact repair and restoration retry");
 {bool ok=true;std::vector<unsigned> steps;
 ok &= core_metal::cleanupStep("expected diagnostic",[&]{steps.push_back(1);throw std::runtime_error("validation");});
 ok &= core_metal::cleanupStep("unpin",[&]{steps.push_back(2);});
 ok &= core_metal::cleanupStep("unregister",[&]{steps.push_back(3);throw 7;});
 ok &= core_metal::cleanupStep("world close",[&]{steps.push_back(4);});
 assert(!ok && steps==std::vector<unsigned>({1,2,3,4}));}
 puts("PASS independent nonthrow cleanup continues after diagnostics and retains failure");
 puts("PASS production GearVisual cleanup result and retry regressions");
}
