#include "planet_mesh.h"
#include <iostream>
#include <limits>
static void check(bool b) {if(!b) throw std::runtime_error("planet mesh test failed");}
int main() {
    constexpr double radius=7123456.75; // Arbitrary radius proves no Earth constant.
    auto west=core_metal::planetMesh(radius,0),east=core_metal::planetMesh(radius,1);
    for(const auto *m:{&west,&east}) {
        check(m->vertices.size()==65*65 && m->indices.size()==64*64*6);
        for(const auto &v:m->vertices) {
            check(std::abs(std::sqrt(double(v.x)*v.x+double(v.y)*v.y+double(v.z)*v.z)-radius)<1);
            check(v.w==1 && v.u>=0&&v.u<=1&&v.v>=0&&v.v<=1);
        }
        for(auto i:m->indices) check(i<m->vertices.size());
        for(unsigned row=0;row<=64;++row) for(unsigned col=0;col<=64;++col) {
            const auto &v=m->vertices[row*65+col];
            check(v.u==float(col)/64 && v.v==float(row)/64);
            if(row==0) check(v.y==float(radius));
            if(row==64) check(v.y==-float(radius));
        }
        for(unsigned row=0;row<64;++row) for(unsigned col=0;col<64;++col) {
            unsigned a=row*65+col,j=(row*64+col)*6;
            std::array<unsigned,6> expected={a,a+65,a+1,a+1,a+65,a+66};
            for(unsigned k=0;k<6;++k) check(m->indices[j+k]==expected[k]);
        }
    }
    check(west.vertices[32*65+32].z==-float(radius));
    check(east.vertices[32*65+32].z==float(radius));
    for(unsigned row=0;row<=64;++row) {
        const auto &a=west.vertices[row*65+64],&b=east.vertices[row*65];
        check(a.x==b.x && a.y==b.y && a.z==b.z && a.u==1 && b.u==0);
    }
    for(auto input:{0.,-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        bool rejected=false;try{core_metal::planetMesh(input,0);}catch(const std::runtime_error&){rejected=true;}check(rejected);
    }
    bool rejected=false;try{core_metal::planetMesh(radius,2);}catch(const std::runtime_error&){rejected=true;}check(rejected);
    std::cout<<"PASS planet mesh: 2x64x64 cells, radius/hemisphere/V/UV/seam/topology/range/Metal stride\n";
}
