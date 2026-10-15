#include "texture_store.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
using namespace core_metal;
static void require(bool ok,const char *message) { if(!ok) throw std::runtime_error(message); }
static void writeDDS(const fs::path &path) {
    std::vector<uint8_t> data(136);
    auto put=[&](size_t p,uint32_t n){for(unsigned i=0;i<4;++i)data[p+i]=uint8_t(n>>(i*8));};
    put(0,0x20534444); put(4,124); put(8,0x81007); put(12,4); put(16,4);
    put(20,8); put(76,32); put(80,4); put(84,0x31545844); put(108,0x1000);
    data[128]=0;data[129]=0xf8; // red RGB565; selectors all zero
    std::ofstream f(path,std::ios::binary); f.write(reinterpret_cast<const char*>(data.data()),data.size());
}
int main() {
    fs::path directory;
    try {
        const char *tmp=std::getenv("TMPDIR"); require(tmp,"TMPDIR required");
        std::string name=(fs::path(tmp)/"orbiter-texture-unit-XXXXXX").string();
        std::vector<char> buffer(name.begin(),name.end());buffer.push_back(0);
        auto created=mkdtemp(buffer.data()); require(created,"mkdtemp"); directory=created;
        auto high=directory/"high",low=directory/"low";
        fs::create_directories(high/"DG"); fs::create_directories(low/"DG");
        writeDDS(low/"DG"/"Hull.dds"); writeDDS(high/"DG"/"Hull.dds");
        fs::create_directory_symlink(high,directory/"staged-high");
        std::vector<fs::path> roots={fs::canonical(directory/"staged-high"),fs::canonical(low)};
        require(texturePath(roots,"dg\\HULL.dds")==fs::canonical(high/"DG"/"Hull.dds"),"high root/case/separator precedence");
        for(const char *invalid:{"../low/DG/Hull.dds","/etc/passwd","DG/../../low/DG/Hull.dds","DG/./Hull.dds","C:\\Hull.dds","DG/Hull.png",""})
            require(texturePath(roots,invalid).empty(),"unsafe path accepted");
        fs::create_symlink(low/"DG"/"Hull.dds",high/"DG"/"escape.dds");
        require(texturePath(roots,"DG/escape.dds").empty(),"outside root symlink");
        fs::create_symlink(high/"DG"/"Hull.dds",high/"DG"/"inside.dds");
        require(!texturePath(roots,"DG/inside.dds").empty(),"inside root symlink");
        // Default macOS volumes cannot contain case-colliding siblings. Feed
        // explicit candidates into the same production selection rule.
        require(unambiguousTextureMatch({high/"DG"/"Hull.dds",high/"DG"/"hULL.dds"}).empty(),"ambiguous exact case accepted");
        unsigned uploads=0;
        TextureStore<unsigned> store({high.string(),low.string()},[&](const orbiter::dds::Image &image){
            require(image.width==4 && image.height==4 && image.rgba.size()==64,"DDS upload dimensions");
            require(image.rgba[0]==255 && image.rgba[1]==0 && image.rgba[2]==0 && image.rgba[3]==255,"DDS upload channel order");
            return ++uploads;
        },192);
        void *global=store.load("DG/Hull.dds",8);
        require(global && store.load("dg\\hull.dds",15)==global && uploads==1,"global cache identity");
        store.release(global); require(store.lookup(global),"global released early");
        void *a=store.load("DG/Hull.dds",0),*b=store.load("DG/Hull.dds",7);
        require(a && b && a!=b && a!=global && store.resident==192,"nonglobal ownership");
        require(!store.load("DG/Hull.dds",0),"aggregate cap not enforced");
        require(store.pin(a),"pin");store.release(a); require(store.lookup(a),"pin did not protect release");
        store.unpin(a); require(!store.lookup(a) && store.resident==128,"unpinned dead texture resident");
        store.release(a); require(store.invalidReleases==1,"stale release");
        require(!store.lookup(reinterpret_cast<void*>(1)) && !store.pin(reinterpret_cast<void*>(1)),"foreign handle accepted");
        store.release(b); require(!store.lookup(b),"nonglobal leak");
        auto c=store.load("DG/Hull.dds",0); require(c && c!=a && c!=b,"stale identity reused");
        require(!store.load("DG/Hull.dds",16) && !store.load(nullptr,0),"unsupported flags/name accepted");
        {std::ofstream f(high/"DG"/"bad.dds");f<<"not a dds";}
        require(!store.load("DG/bad.dds",8),"malformed DDS success");
        {std::ofstream f(high/"DG"/"huge.dds");f.seekp(128*1024*1024);f.put(0);}
        require(!store.load("DG/huge.dds",8),"oversize file accepted");
        TextureStore<unsigned> failedUpload({high.string()},[](const orbiter::dds::Image&){return 0u;});
        require(!failedUpload.load("DG/Hull.dds",8) && !failedUpload.resident,"failed upload advertised success");
        TextureStore<unsigned> paddedAllocation({high.string()},[](const orbiter::dds::Image&){return 1u;},128,
            [](const unsigned&){return size_t(256);});
        require(!paddedAllocation.load("DG/Hull.dds",8) && !paddedAllocation.resident,"actual allocation cap bypass");
        unsigned previous=0;
        require(resolveTexture(unsigned(-2),previous,10)==0,"first inherit");
        require(resolveTexture(0,previous,10)==1 && resolveTexture(unsigned(-2),previous,10)==1,"zero-based group to one-based slot/inherit");
        require(resolveTexture(9,previous,10)==10,"last engine slot");
        require(resolveTexture(unsigned(-1),previous,10)==0,"default/no texture");
        bool threw=false;try{resolveTexture(10,previous,10);}catch(const std::runtime_error&){threw=true;}
        require(threw,"invalid texture index accepted");
        fs::remove_all(directory);
        std::cout<<"PASS: configured roots/case/containment, bounded DDS upload, global cache/non-global ownership/pins/stale handles, texture sentinels\n";
    } catch(const std::exception &e) { std::cerr<<"FAIL: "<<e.what()<<" retained="<<directory<<"\n";return 1; }
}
