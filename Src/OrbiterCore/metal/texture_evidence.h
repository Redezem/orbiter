#pragma once
#include <fstream>
#include <iomanip>
#include <sstream>
#include "texture_store.h"
namespace core_metal {
// Evidence uses actual API-uploaded arrays, not a second mesh-file parser.
inline void textureJSONPath(std::ostream &out,const std::string &path) {
    out<<'"';
    constexpr char hex[]="0123456789abcdef";
    for(unsigned char c:path) {
        if(c=='"' || c=='\\') out<<'\\'<<char(c);
        else if(c<32) out<<"\\u00"<<hex[c>>4]<<hex[c&15];
        else out<<char(c);
    }
    out<<'"';
}
template<class Vertices,class UVs,class Colors,class Indices,class Ranges,class Store>
void writeTextureGeometry(const std::string &path,const Vertices &vertices,const UVs &uvs,
    const Colors &colors,const Indices &indices,const Ranges &ranges,const Store &store) {
    std::ofstream out(path);
    out<<std::setprecision(9)<<"{\"vertices\":[";
    for(size_t i=0;i<vertices.size();++i) {
        const auto &v=vertices[i];
        if(i) out<<',';
        out<<'['<<v.x<<','<<v.y<<','<<v.z<<','<<v.shade<<','<<uvs[i][0]<<','<<uvs[i][1];
        for(float c:colors[i]) out<<','<<c;
        out<<']';
    }
    out<<"],\"indices\":[";
    for(size_t i=0;i<indices.size();++i) {if(i) out<<',';out<<indices[i];}
    out<<"],\"ranges\":[";
    for(size_t i=0;i<ranges.size();++i) {
        if(i)out<<',';
        const auto &range=ranges[i];auto entry=store.lookup(range.handle);
        out<<"{\"first\":"<<range.first<<",\"count\":"<<range.count<<",\"texture\":";
        textureJSONPath(out,entry ? entry->path : std::string{}); out<<'}';
    }
    out<<"]}\n";
    if(!out) throw std::runtime_error("texture geometry evidence write failed");
}
}
