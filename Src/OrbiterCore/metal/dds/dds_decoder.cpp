#include "dds_decoder.h"
#include <array>
#include <new>

namespace orbiter { namespace dds {
namespace {
std::uint32_t u32(const std::uint8_t* p) {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1])<<8) |
           (std::uint32_t(p[2])<<16) | (std::uint32_t(p[3])<<24);
}
using Color = std::array<std::uint8_t,4>;
Color rgb565(unsigned c) {
    const unsigned r=(c>>11)&31, g=(c>>5)&63, b=c&31;
    return {{static_cast<std::uint8_t>((r<<3)|(r>>2)),
             static_cast<std::uint8_t>((g<<2)|(g>>4)),
             static_cast<std::uint8_t>((b<<3)|(b>>2)),255}};
}
}
Result decode(const std::uint8_t* data, std::size_t size) {
    if (!data || size < 128) return {Error::truncated,{}};
    if(u32(data)!=0x20534444 || u32(data+4)!=124 || u32(data+76)!=32)
        return {Error::invalid_header,{}};
    // Absolute file offsets: flags=8, height=12, width=16, depth=24,
    // mip count=28, DDS_PIXELFORMAT=76..107, caps=108, caps2=112.
    const auto flags=u32(data+8), pfFlags=u32(data+80);
    if((flags&0x1007)!=0x1007 || !(u32(data+108)&0x1000))
        return {Error::invalid_header,{}};
    if((pfFlags&4)==0 || (pfFlags&~5u)!=0 || u32(data+112)!=0 ||
       (flags&0x800000) || u32(data+24)>1)
        return {Error::unsupported_format,{}};
    const auto format=u32(data+84);
    if(format!=0x31545844 && format!=0x33545844 && format!=0x35545844)
        return {Error::unsupported_format,{}};
    const std::size_t blockSize=format==0x31545844 ? 8 : 16;
    const auto w=u32(data+16), h=u32(data+12);
    if(!w || !h || w>16384 || h>16384 || std::uint64_t(w)*h>16777216)
        return {Error::dimension_limit,{}};
    const std::size_t bx=(w+3)/4, by=(h+3)/4;
    const auto declaredMips=u32(data+28);
    unsigned maxMips=1;
    for(auto dim=w>h?w:h; dim>1; dim/=2) ++maxMips;
    if(((flags&0x20000) && !declaredMips) ||
       (!(flags&0x20000) && declaredMips>1) || declaredMips>maxMips)
        return {Error::invalid_header,{}};
    const unsigned mipCount=declaredMips ? declaredMips : 1;
    // Caps precede all size arithmetic. Even on 32-bit hosts, all products
    // below fit size_t (<=64 MiB output). Subtract remaining bytes to avoid
    // offset+length overflow; every declared mip must be present.
    std::size_t remaining=size-128;
    auto mw=w, mh=h;
    for(unsigned level=0;level<mipCount;++level) {
        const std::size_t bytes=std::size_t((mw+3)/4)*((mh+3)/4)*blockSize;
        if(bytes>remaining) return {Error::truncated,{}};
        remaining-=bytes;
        mw=mw>1?mw/2:1; mh=mh>1?mh/2:1;
    }
    Result result;
    result.image.width=w; result.image.height=h;
    try { result.image.rgba.resize(std::size_t(w)*h*4); }
    catch(const std::bad_alloc&) { return {Error::allocation_failed,{}}; }
    for(std::size_t y=0;y<by;++y) for(std::size_t x=0;x<bx;++x) {
        const auto* block=data+128+(y*bx+x)*blockSize;
        const auto* p=block+(blockSize==8 ? 0 : 8);
        const unsigned c0=p[0]|(unsigned(p[1])<<8), c1=p[2]|(unsigned(p[3])<<8);
        Color palette[4]={rgb565(c0),rgb565(c1),{}, {}};
        if(c0>c1 || blockSize==16) {
            for(int c=0;c<3;++c) {
                palette[2][c]=(2*palette[0][c]+palette[1][c])/3;
                palette[3][c]=(palette[0][c]+2*palette[1][c])/3;
            }
            palette[2][3]=palette[3][3]=255;
        } else {
            for(int c=0;c<3;++c) palette[2][c]=(palette[0][c]+palette[1][c])/2;
            palette[2][3]=255;
        }
        const auto selectors=u32(p+4);
        std::uint8_t alpha[8]={block[0],block[1],0,0,0,0,0,0};
        std::uint64_t alphaBits=0;
        if(format==0x35545844) {
            if(alpha[0]>alpha[1]) {
                for(unsigned i=2;i<8;++i) alpha[i]=((8-i)*alpha[0]+(i-1)*alpha[1])/7;
            } else {
                for(unsigned i=2;i<6;++i) alpha[i]=((6-i)*alpha[0]+(i-1)*alpha[1])/5;
                alpha[6]=0; alpha[7]=255;
            }
            for(unsigned i=0;i<6;++i) alphaBits|=std::uint64_t(block[2+i])<<(8*i);
        }
        for(unsigned i=0;i<16;++i) {
            const auto px=x*4+i%4, py=y*4+i/4;
            if(px>=w || py>=h) continue;
            const auto& color=palette[(selectors>>(2*i))&3];
            for(unsigned c=0;c<4;++c) result.image.rgba[(py*w+px)*4+c]=color[c];
            if(format==0x33545844)
                result.image.rgba[(py*w+px)*4+3]=((block[i/2]>>(4*(i%2)))&15)*17;
            if(format==0x35545844)
                result.image.rgba[(py*w+px)*4+3]=alpha[(alphaBits>>(3*i))&7];
        }
    }
    return result;
}
} }
