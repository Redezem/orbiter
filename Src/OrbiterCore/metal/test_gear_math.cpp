#include "gear_math.h"
#include <cassert>
#include <cstdio>
using namespace core_metal::gear;
int main() {
    Rotation r{0,1,{1,2,3},{1,0,0},1.5707963267948966,{0}};
    auto m=evaluate({r},1,0,2);
    auto p=point(m[0],{1,3,3});
    assert(std::abs(p[0]-1)<1e-6 && std::abs(p[1]-2)<1e-6 && std::abs(p[2]-2)<1e-6);
    auto pivot=point(m[0],r.pivot);
    for(int i=0;i<3;++i) assert(std::abs(pivot[i]-r.pivot[i])<1e-6);
    assert(m[1]==identity());
    assert(evaluate({r},1,1,1)[0]==identity());
    Rotation s{0,1,{0,0,0},{0,1,0},.7,{0}};
    // Internal composition consistency, not an independent multiplication oracle.
    // The Python GPU regression independently uses scalar quaternion vector action.
    auto both=evaluate({r,s},1,0,1)[0];
    auto sequential=point(evaluate({r},1,0,1)[0],point(evaluate({s},1,0,1)[0],{2,3,4}));
    auto actual=point(both,{2,3,4});
    for(int i=0;i<3;++i) assert(std::abs(actual[i]-sequential[i])<1e-6);
    auto forward=evaluate({r,s},0,1,1)[0];
    auto fseq=point(evaluate({s},0,1,1)[0],point(evaluate({r},0,1,1)[0],{2,3,4}));
    auto fact=point(forward,{2,3,4});
    for(int i=0;i<3;++i) assert(std::abs(fact[i]-fseq[i])<1e-6);
    Rotation interval{.3,.9,{0,0,0},{0,0,1},1,{0}};
    assert(evaluate({interval},1,0,1)==evaluate({interval},1,.3,1));
    assert(evaluate({interval},1,.9,1)[0]==identity());
    auto half=point(evaluate({interval},1,.6,1)[0],{1,0,0});
    assert(std::abs(half[0]-std::cos(-.5))<1e-6 && std::abs(half[1]-std::sin(-.5))<1e-6);
    auto bad=[&](Rotation value,double target=0) {bool rejected=false;try {evaluate({value},1,target,1);}catch(const std::runtime_error&) {rejected=true;}assert(rejected);};
    auto invalid=r; invalid.end=invalid.start; bad(invalid);
    invalid=r;invalid.groups={1};bad(invalid);
    invalid=r;invalid.groups.clear();bad(invalid);
    invalid=r;invalid.axis={0,0,0};bad(invalid);
    invalid=r;invalid.pivot[0]=NAN;bad(invalid);
    invalid=r;invalid.angle=INFINITY;bad(invalid);
    bad(r,NAN);bad(r,-.1);bad(r,1.1);
    Handles handles; auto a=handles.create(); assert(handles.valid(a)); assert(!handles.valid(reinterpret_cast<void*>(1)));
    handles.retire(a); assert(!handles.valid(a)); auto b=handles.create(); assert(a!=b && handles.valid(b));
    handles.retire(b); assert(!handles.valid(b));
    puts("PASS gear default/current, reverse composition, pivot, invariant, interval rejection, stale/foreign handles");
}
