#pragma once
#include <array>
#include <vector>
#include <cmath>
#include <cstdint>
#include <stdexcept>
namespace core_metal {
// Metal float4+float2 struct has 16-byte alignment and 32-byte stride.
struct alignas(16) PlanetVertex { float x,y,z,w,u,v; };
static_assert(sizeof(PlanetVertex)==32,"Metal planet vertex stride");
struct PlanetMesh { std::vector<PlanetVertex> vertices; std::vector<uint32_t> indices; };
inline PlanetMesh planetMesh(double radius,unsigned hemisphere) {
    if(!std::isfinite(radius)||radius<=0||hemisphere>1) throw std::runtime_error("invalid globe mesh input");
    PlanetMesh m;
    constexpr double pi=3.14159265358979323846;
    for(unsigned row=0;row<=64;++row) for(unsigned col=0;col<=64;++col) {
        double lat=pi/2-pi*row/64,lon=-pi+hemisphere*pi+pi*col/64;
        m.vertices.push_back({float(radius*cos(lat)*cos(lon)),float(radius*sin(lat)),float(radius*cos(lat)*sin(lon)),1,float(col)/64,float(row)/64});
    }
    for(unsigned row=0;row<64;++row) for(unsigned col=0;col<64;++col) {
        uint32_t a=row*65+col,b=a+1,c=a+65,d=c+1;
        for(auto i:{a,c,b,b,c,d}) m.indices.push_back(i);
    }
    return m;
}
}
