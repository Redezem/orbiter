#include "dds_decoder.h"
#include <cstdlib>
#include <iostream>
#include <vector>
using Bytes = std::vector<std::uint8_t>;
void require(bool yes, const char* name) { if (!yes) { std::cerr << "FAIL: " << name << '\n'; std::exit(1); } }
void put(Bytes& b, std::size_t n, std::uint32_t v) { for (int i=0;i<4;++i) b[n+i] = static_cast<std::uint8_t>(v >> (8*i)); }
Bytes fixture(const char* format="DXT1", unsigned w=4, unsigned h=4) {
    Bytes b(128 + (format[3]=='1' ? 8 : 16), 0);
    put(b,0,0x20534444); put(b,4,124); put(b,8,0x81007);
    put(b,12,h); put(b,16,w); put(b,76,32); put(b,80,4);
    for(int i=0;i<4;++i) b[84+i]=format[i];
    put(b,108,0x1000); return b;
}
int main() {
    auto b=fixture();
    // Synthetic codec unit fixture: red, blue, 2:1 red/blue, 1:2 red/blue.
    b[128]=0; b[129]=0xf8; b[130]=0x1f; b[131]=0;
    put(b,132,0xe4e4e4e4);
    auto r=orbiter::dds::decode(b.data(),b.size());
    require(bool(r), "BC1 accepted");
    require(r.image.width==4 && r.image.height==4 && r.image.rgba.size()==64, "dimensions RGBA8");
    const unsigned expected[16]={255,0,0,255,0,0,255,255,170,0,85,255,85,0,170,255};
    for(unsigned i=0;i<16;++i) require(r.image.rgba[i]==expected[i], "BC1 2:1 and 1:2 interpolation");
    // BC1 c0 <= c1: midpoint and transparent black (including equal endpoints).
    b[128]=0x1f; b[129]=0; b[130]=0; b[131]=0xf8;
    r=orbiter::dds::decode(b.data(),b.size());
    require(bool(r) && r.image.rgba[8]==127 && r.image.rgba[10]==127 && r.image.rgba[11]==255,
            "BC1 midpoint");
    for(unsigned c=0;c<4;++c) require(r.image.rgba[12+c]==0, "BC1 transparent black");
    b[130]=0x1f; b[131]=0;
    r=orbiter::dds::decode(b.data(),b.size());
    require(bool(r) && r.image.rgba[15]==0, "BC1 equal endpoint transparency");
    b=fixture("DXT3");
    for(unsigned i=0;i<8;++i) b[128+i]=static_cast<std::uint8_t>((2*i)|((2*i+1)<<4));
    b[136]=0x1f; b[137]=0; b[138]=0; b[139]=0xf8;
    put(b,140,0xffffffff); // BC2 always four colors even with c0 <= c1.
    r=orbiter::dds::decode(b.data(),b.size());
    require(bool(r), "BC2 accepted");
    for(unsigned i=0;i<16;++i) {
        require(r.image.rgba[i*4+3]==i*17, "BC2 all explicit alpha nibbles");
        require(r.image.rgba[i*4]==170 && r.image.rgba[i*4+2]==85, "BC2 opaque color mode");
    }
    b=fixture("DXT5");
    b[136]=0x1f; b[138]=0; b[139]=0xf8; put(b,140,0xffffffff);
    std::uint64_t alphaSelectors=0;
    for(unsigned i=0;i<16;++i) alphaSelectors|=std::uint64_t(i%8)<<(3*i);
    for(unsigned i=0;i<6;++i) b[130+i]=static_cast<std::uint8_t>(alphaSelectors>>(8*i));
    const unsigned alphaExpected[3][8]={{240,16,208,176,144,112,80,48},{10,210,50,90,130,170,0,255},{50,50,50,50,50,50,0,255}};
    for(unsigned branch=0;branch<3;++branch) {
        b[128]=alphaExpected[branch][0]; b[129]=alphaExpected[branch][1];
        r=orbiter::dds::decode(b.data(),b.size());
        require(bool(r), "BC3 accepted");
        for(unsigned i=0;i<16;++i) {
            require(r.image.rgba[i*4+3]==alphaExpected[branch][i%8], "BC3 alpha >, <, = branches and selectors");
            require(r.image.rgba[i*4]==170 && r.image.rgba[i*4+2]==85, "BC3 four color mode");
        }
    }
    using orbiter::dds::Error;
    auto expectError=[](const Bytes& input, Error error) {
        auto result=orbiter::dds::decode(input.data(),input.size());
        require(result.error==error, "malformed DDS error classification");
        require(result.image.width==0 && result.image.height==0 && result.image.rgba.empty(), "failure has no partial image");
    };
    b=fixture();
    for(std::size_t n=0;n<b.size();++n) {
        Bytes prefix(b.begin(),b.begin()+n); expectError(prefix,Error::truncated);
    }
    require(orbiter::dds::decode(nullptr,1000).error==Error::truncated,"null pointer");
    for(unsigned offset : {0u,4u,76u,8u,108u}) {
        auto bad=b; put(bad,offset,0); expectError(bad,Error::invalid_header);
    }
    for(unsigned flag : {0u,0x40u,0x44u}) {
        auto bad=b; put(bad,80,flag); expectError(bad,Error::unsupported_format);
    }
    for(unsigned code : {0u,0x30315844u,0x32545844u,0x34545844u}) {
        auto bad=b; put(bad,84,code); expectError(bad,Error::unsupported_format);
    }
    for(unsigned offset : {12u,16u}) for(unsigned dim : {0u,16385u,0xffffffffu}) {
        auto bad=b; put(bad,offset,dim); expectError(bad,Error::dimension_limit);
    }
    auto bad=b; put(bad,12,16384); put(bad,16,16384); expectError(bad,Error::dimension_limit);
    for(unsigned caps : {0x200u,0x400u,0x200000u}) {
        bad=b; put(bad,112,caps); expectError(bad,Error::unsupported_format);
    }
    bad=b; put(bad,24,2); expectError(bad,Error::unsupported_format);
    bad=b; put(bad,8,0x881007); expectError(bad,Error::unsupported_format);
    // Three levels for 4x4: 8 bytes each, including sub-block mips.
    b.resize(152); put(b,8,0xa1007); put(b,28,3);
    r=orbiter::dds::decode(b.data(),b.size()); require(bool(r),"complete mip chain");
    for(std::size_t n=128;n<152;++n) {
        Bytes prefix(b.begin(),b.begin()+n); expectError(prefix,Error::truncated);
    }
    bad=b; put(bad,28,4); expectError(bad,Error::invalid_header);
    bad=b; put(bad,28,0); expectError(bad,Error::invalid_header);
    bad=b; put(bad,8,0x81007); expectError(bad,Error::invalid_header);
    // Non-multiple-of-four dimensions crop edge blocks, not the input blocks.
    for(const char* format : {"DXT1","DXT3","DXT5"}) {
        b=fixture(format,5,3); b.resize(128+(format[3]=='1'?16:32),0xff);
        r=orbiter::dds::decode(b.data(),b.size());
        require(bool(r) && r.image.rgba.size()==60,"cropped edge blocks");
        b.pop_back(); expectError(b,Error::truncated);
    }
    b=fixture("DXT1",1,1); r=orbiter::dds::decode(b.data(),b.size());
    require(bool(r) && r.image.rgba.size()==4,"minimum dimensions");
    // Supported cap boundaries, exact pixel budget, unaligned borrowed input.
    b=fixture("DXT1",16384,1); b.resize(128+4096*8);
    r=orbiter::dds::decode(b.data(),b.size());
    require(bool(r) && r.image.rgba.size()==16384*4,"maximum dimension accepted");
    b=fixture("DXT1",4096,4096); b.resize(128+1024*1024*8);
    r=orbiter::dds::decode(b.data(),b.size());
    require(bool(r) && r.image.rgba.size()==16777216*4,"maximum pixel budget accepted");
    put(b,12,4097); expectError(b,Error::dimension_limit);
    b=fixture(); b.insert(b.begin(),0xa5);
    r=orbiter::dds::decode(b.data()+1,b.size()-1);
    require(bool(r) && b[0]==0xa5,"unaligned input");
    // A 5x3 image consumes two blocks and uses the second block for x=4.
    b=fixture("DXT1",5,3); b.resize(144);
    b[128]=0; b[129]=0xf8; b[136]=0x1f; b[137]=0;
    r=orbiter::dds::decode(b.data(),b.size()); require(bool(r),"two horizontal blocks");
    for(unsigned y=0;y<3;++y) for(unsigned x=0;x<5;++x) {
        const auto offset=(y*5+x)*4;
        require(r.image.rgba[offset]==(x<4?255:0) && r.image.rgba[offset+2]==(x<4?0:255),"edge crop pixel mapping");
    }
    // Deterministic bounded mutation smoke fuzz, not exhaustive fuzzing.
    std::uint32_t state=0x5eed1234;
    auto random=[&]() { state^=state<<13; state^=state>>17; state^=state<<5; return state; };
    for(unsigned trial=0;trial<20000;++trial) {
        b=fixture(trial%3==0?"DXT1":trial%3==1?"DXT3":"DXT5",1+random()%16,1+random()%16);
        b.resize(128+256,0);
        for(std::size_t i=128;i<b.size();++i) b[i]=static_cast<std::uint8_t>(random());
        if(trial%2) for(unsigned i=0;i<4;++i) b[random()%128]=static_cast<std::uint8_t>(random());
        b.resize(random()%(b.size()+1));
        r=orbiter::dds::decode(b.data(),b.size());
        if(r) require(r.image.rgba.size()==std::size_t(r.image.width)*r.image.height*4,"fuzz success invariant");
        else require(r.image.rgba.empty() && !r.image.width && !r.image.height,"fuzz failure invariant");
    }
    std::cout << "DDS codec unit tests passed; 20000 bounded mutation cases\n";
}
