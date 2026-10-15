#pragma once
#include "dds/dds_decoder.h"
#include <algorithm>
#include <filesystem>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace core_metal {
namespace fs = std::filesystem;
inline std::string textureSeparators(std::string s) {
    std::replace(s.begin(),s.end(),'\\','/'); return s;
}
inline std::string textureFold(std::string s) {
    for(auto &c:s) if(c>='A' && c<='Z') c+=('a'-'A'); return s;
}
inline bool textureWithin(const fs::path &root,const fs::path &path) {
    auto r=root.begin(),p=path.begin();
    for(;r!=root.end();++r,++p) if(p==path.end() || *p!=*r) return false;
    return true;
}
inline fs::path unambiguousTextureMatch(const std::vector<fs::path> &matches) {
    return matches.size()==1 ? matches.front() : fs::path{};
}
// Configured roots may themselves be symlinks (staged content). Resolve these
// once. Child links are allowed only if their canonical target stays in that root.
inline fs::path texturePath(const std::vector<fs::path> &roots,const std::string &name) {
    fs::path rel(textureSeparators(name));
    if(rel.empty() || rel.is_absolute() || name.find(':')!=std::string::npos) return {};
    for(const auto &part:rel) if(part==".." || part=="." || part.empty()) return {};
    if(textureFold(rel.extension().string())!=".dds") return {};
    for(const auto &root:roots) {
        fs::path current=root;
        bool found=true;
        for(const auto &part:rel) {
            std::vector<fs::path> matches;
            std::error_code ec;
            for(fs::directory_iterator it(current,ec),end; !ec && it!=end; it.increment(ec))
                if(textureFold(it->path().filename().string())==textureFold(part.string())) matches.push_back(it->path());
            // Do not pick an arbitrary case collision, even when one is exact.
            if(!matches.empty() && unambiguousTextureMatch(matches).empty()) return {};
            if(ec || matches.empty()) { found=false; break; }
            current=fs::canonical(matches[0],ec);
            if(ec || !textureWithin(root,current)) return {};
        }
        if(found && fs::is_regular_file(current)) return current;
    }
    return {};
}
inline unsigned resolveTexture(unsigned raw,unsigned &previous,unsigned count) {
    // MESHGROUP indices are zero-based; oapiGetTextureHandle slots are one-based.
    // Match D3D9Mesh::ProcessInherit (Mesh.cpp:560-563), including raw 0.
    if(raw!=unsigned(-2) && raw!=unsigned(-1) && raw>=count)
        throw std::runtime_error("unsupported engine texture index");
    unsigned resolved=raw==unsigned(-2) ? previous : raw==unsigned(-1) ? 0 : raw+1;
    if(resolved>count) throw std::runtime_error("unsupported inherited texture slot");
    previous=resolved; return resolved;
}

// Opaque identities never reused: dead entries remain small tombstones so stale
// handles cannot alias future textures. Entries and aggregate residency are bounded.
// Global bit3 requests share a repository entry until destruction; non-global
// requests have independent ownership. Pins protect mesh copies across releases.
// All low four flags are honored by shared, decoded, mip0-only storage. No writes,
// surface API, mip filtering, or alpha blending are provided by this static slice.
template<class Payload> class TextureStore {
public:
    struct Entry {
        Payload payload{};
        std::string path;
        unsigned width=0,height=0,refs=1,pins=0;
        size_t bytes=0;
        bool global=false,live=true;
    };
    using Upload=std::function<Payload(const orbiter::dds::Image&)>;
    using AllocationSize=std::function<size_t(const Payload&)>;
    TextureStore(std::vector<std::string> roots,Upload upload,size_t cap=256u*1024*1024,AllocationSize allocationSize={})
        : upload_(std::move(upload)), allocation_size_(std::move(allocationSize)), cap_(cap) {
        for(auto &root:roots) {
            std::error_code ec;
            auto p=fs::canonical(textureSeparators(root),ec);
            if(!ec && fs::is_directory(p)) roots_.push_back(p);
        }
    }
    void *load(const char *name,unsigned flags) noexcept {
        ++requests;
        try {
            if(!name || flags&~15u) return nullptr;
            auto path=texturePath(roots_,name);
            if(path.empty()) return nullptr;
            auto key=path.string();
            if(flags&8) {
                auto it=globals_.find(key);
                if(it!=globals_.end()) { ++hits; ++successes; return it->second; }
            }
            if(entries_.size()>=4096) return nullptr;
            int fd=open(path.c_str(),O_RDONLY|O_NOFOLLOW);
            if(fd<0) return nullptr;
            struct FD { int fd; ~FD(){close(fd);} } guard{fd};
            struct stat stat{};
            if(fstat(fd,&stat) || !S_ISREG(stat.st_mode) || stat.st_size<128 || stat.st_size>128*1024*1024) return nullptr;
            std::vector<uint8_t> data(static_cast<size_t>(stat.st_size));
            size_t offset=0;
            while(offset<data.size()) {
                auto n=read(fd,data.data()+offset,data.size()-offset);
                if(n<=0) return nullptr;
                offset+=size_t(n);
            }
            auto decoded=orbiter::dds::decode(data.data(),data.size());
            if(!decoded || decoded.image.rgba.size()>cap_-resident) return nullptr;
            auto payload=upload_(decoded.image);
            if(!payload) return nullptr;
            size_t charged=decoded.image.rgba.size();
            if(allocation_size_) charged=std::max(charged,allocation_size_(payload));
            if(charged>cap_-resident) return nullptr;
            auto entry=std::make_unique<Entry>();
            entry->payload=std::move(payload); entry->path=key;
            entry->width=decoded.image.width; entry->height=decoded.image.height;
            entry->bytes=charged; entry->global=bool(flags&8);
            void *handle=entry.get();
            size_t bytes=entry->bytes;
            entries_.emplace(handle,std::move(entry));
            try { if(flags&8) globals_.emplace(key,handle); }
            catch(...) { entries_.erase(handle); throw; }
            resident+=bytes;
            ++uploads; ++successes;
            return handle;
        } catch(...) { return nullptr; }
    }
    const Entry *lookup(void *handle) const {
        auto it=entries_.find(handle);
        return it!=entries_.end() && it->second->live ? it->second.get() : nullptr;
    }
    bool pin(void *handle) { auto *e=mutableEntry(handle); if(!e) return false; ++e->pins; return true; }
    void unpin(void *handle) { auto *e=mutableEntry(handle); if(e && e->pins) { --e->pins; collect(*e); } }
    void release(void *handle) {
        ++releases;
        auto *e=mutableEntry(handle);
        if(!e) { ++invalidReleases; return; }
        if(!e->global && e->refs) { --e->refs; collect(*e); }
    }
    size_t resident=0;
    unsigned requests=0,successes=0,hits=0,uploads=0,releases=0,invalidReleases=0;
private:
    Entry *mutableEntry(void *h) { return const_cast<Entry*>(lookup(h)); }
    void collect(Entry &e) {
        if(!e.global && !e.refs && !e.pins) {
            e.payload=Payload{}; e.live=false; resident-=e.bytes;
        }
    }
    Upload upload_;
    AllocationSize allocation_size_;
    size_t cap_;
    std::vector<fs::path> roots_;
    std::unordered_map<void*,std::unique_ptr<Entry>> entries_;
    std::unordered_map<std::string,void*> globals_;
};
}
