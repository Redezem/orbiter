#pragma once
#include <array>
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace core_metal::gear {
using Vec=std::array<double,3>;
using Mat=std::array<float,16>; // Metal column vectors; transpose of D3D9 row-vector form.
inline Mat identity() { return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1}; }
inline Mat multiply(const Mat &a,const Mat &b) {
    Mat out{};
    for(int c=0;c<4;++c) for(int r=0;r<4;++r) for(int k=0;k<4;++k) out[c*4+r]+=a[k*4+r]*b[c*4+k];
    return out;
}
inline Vec point(const Mat &m,Vec p) {
    Vec out{};
    for(int r=0;r<3;++r) {out[r]=m[12+r]; for(int c=0;c<3;++c) out[r]+=m[c*4+r]*p[c];}
    return out;
}
struct Rotation { double start,end; Vec pivot,axis; double angle; std::vector<unsigned> groups;
    bool operator==(const Rotation &b) const { return start==b.start && end==b.end && pivot==b.pivot && axis==b.axis && angle==b.angle && groups==b.groups; }
};
inline Mat rotation(const Rotation &r,double ds) {
    // Exact source convention: D3D9Util.cpp:871 quaternion, NOT normalized
    // Rodrigues: DG's (0,.052,.999) axis is deliberately not exactly unit length.
    float half=float(ds)*float(r.angle)*.5f, s=std::sin(half), w=std::cos(half);
    float x=s*float(r.axis[0]),y=s*float(r.axis[1]),z=s*float(r.axis[2]);
    Mat m=identity();
    m[0]=1-2*(y*y+z*z); m[1]=2*(x*y+w*z); m[2]=2*(x*z-w*y);
    m[4]=2*(x*y-w*z); m[5]=1-2*(x*x+z*z); m[6]=2*(y*z+w*x);
    m[8]=2*(x*z+w*y); m[9]=2*(y*z-w*x); m[10]=1-2*(x*x+y*y);
    auto p=point(m,r.pivot); for(int i=0;i<3;++i) m[12+i]=float(r.pivot[i]-p[i]);
    return m;
}
inline std::vector<Mat> evaluate(const std::vector<Rotation> &definitions,double defstate,double target,size_t groups) {
    if(!std::isfinite(target)||target<0||target>1||!std::isfinite(defstate)||defstate<0||defstate>1) throw std::runtime_error("gear state invalid");
    std::vector<Mat> result(groups,identity());
    for(size_t n=0;n<definitions.size();++n) {
        auto &r=definitions[target>defstate?n:definitions.size()-1-n];
        double norm=0; for(int i=0;i<3;++i) {if(!std::isfinite(r.pivot[i])||!std::isfinite(r.axis[i])) throw std::runtime_error("gear rotation nonfinite"); norm+=r.axis[i]*r.axis[i];}
        if(!std::isfinite(r.start)||!std::isfinite(r.end)||r.start<0||r.end>1||r.start>=r.end||!std::isfinite(r.angle)||norm<.99||norm>1.01||r.groups.empty()) throw std::runtime_error("gear rotation unsupported");
        double ds=(std::clamp(target,r.start,r.end)-std::clamp(defstate,r.start,r.end))/(r.end-r.start);
        auto m=rotation(r,ds);
        for(auto g:r.groups) { if(g>=groups) throw std::runtime_error("gear group outside instance"); result[g]=multiply(m,result[g]); }
    }
    return result;
}
// Tombstones survive retirement for the registry lifetime; no stale-pointer reuse.
// Opaque inputs are compared, never dereferenced.
class Handles {
    struct Token { bool alive=true; };
    std::vector<std::unique_ptr<Token>> tokens;
public:
    void *create() { tokens.push_back(std::make_unique<Token>()); return tokens.back().get(); }
    bool valid(void *p) const {for(auto &t:tokens) if(t.get()==p) return t->alive; return false;}
    void retire(void *p) {for(auto &t:tokens) if(t.get()==p) { t->alive=false; return; }}
};
}
