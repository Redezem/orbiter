#pragma once
// Bundle-only startup. Shared content is read-only by policy; all engine outputs
// and root configs live in an exclusively created session, never in the bundle.
#include <filesystem>
#include <fstream>
#include <unistd.h>
#import <Foundation/Foundation.h>

namespace core_metal {
struct PreviewStartup {
    bool active=false, hidden=false;
    std::string scenario="MetalTerrainVessel", frames="24", mode="--terrain-vessel-test", snapshot;
    std::array<char*,5> args{};
    void prepare(int &argc,char **&argv) {
        active=std::filesystem::path(argv[0]).filename()=="orbiter-core-preview";
        if(!active) return;
        hidden=argc==2 && std::string(argv[1])=="--self-test";
        if(argc!=1 && !hidden) throw std::runtime_error("preview accepts only --self-test (hidden bounded) or no arguments (visible continuous)");
        namespace fs=std::filesystem;
        fs::path resources=[NSBundle mainBundle].resourcePath.UTF8String;
        // Validate every startup enumeration/copy input before creating even the
        // session parent. This is trusted-content preflight, not a TOCTOU sandbox.
        auto invalid=[](const fs::path &path){
            throw std::runtime_error("preview missing staged asset: "+path.string()+" (missing, wrong type or unreadable); restore staged content or rebuild bundle");
        };
        auto readableFile=[&](const fs::path &path){
            if(!fs::is_regular_file(path) || access(path.c_str(),R_OK)!=0) invalid(path);
            std::ifstream input(path,std::ios::binary);
            if(!input) invalid(path);
        };
        auto readableDirectory=[&](const fs::path &path){
            if(!fs::is_directory(path) || access(path.c_str(),R_OK|X_OK)!=0) invalid(path);
            // Force enumeration now, including iterator increment errors.
            for(const auto &entry:fs::directory_iterator(path)) (void)entry;
        };
        readableFile(resources/"content-root.txt");
        std::ifstream manifest(resources/"content-root.txt"); std::string root; std::getline(manifest,root);
        fs::path content=root;
        if(root.empty() || !content.is_absolute()) throw std::runtime_error("preview content-root.txt missing/invalid; rebuild bundle");
        for(const auto &path:{content,content/"Config",content/"Config/Earth",content/"Meshes",content/"Textures"})
            readableDirectory(path);
        for(const auto &name:{"Orbiter.cfg","Config/Earth.cfg","Textures/Earth/Archive/Surf.tree","Modules/Celbody/Earth/Atmosphere/EarthAtmJ71G.dylib","Modules/DeltaGlider.dylib"})
            readableFile(content/name);
        for(const auto &name:{"MetalEarth.cfg","MetalTerrainVessel.scn"}) readableFile(resources/name);
        for(const auto &item:fs::directory_iterator(content))
            if(item.path().extension()==".cfg") readableFile(item.path());
        fs::path parent=fs::path(NSHomeDirectory().UTF8String)/"Library/Application Support/OrbiterCorePreview/Sessions";
        fs::create_directories(parent);
        std::string pattern=(parent/"session-XXXXXX").string();
        if(!mkdtemp(pattern.data())) throw std::runtime_error("cannot create exclusive preview session");
        fs::path launch=pattern;
        fs::create_directory(launch/"Config"); fs::create_directory(launch/"Config/Earth"); fs::create_directory(launch/"Scenarios");
        auto copy=[&](fs::path source,fs::path target){ fs::copy_file(source,target,fs::copy_options::none); };
        copy(resources/"MetalEarth.cfg",launch/"Config/MetalEarth.cfg");
        copy(resources/"MetalTerrainVessel.scn",launch/"Scenarios/MetalTerrainVessel.scn");
        {std::ofstream f(launch/"Config/Earth/Atmosphere.cfg"); f<<"MODULE_ATM = EarthAtmJ71G\n"; if(!f) throw std::runtime_error("private atmosphere config write failed");}
        auto linkRemaining=[&](fs::path from,fs::path to){
            for(const auto &item:fs::directory_iterator(from)) {
                fs::path dest=to/item.path().filename();
                // symlink_status also detects dangling links and filesystem aliases.
                if(fs::symlink_status(dest).type()!=fs::file_type::not_found) continue;
                fs::create_symlink(item.path(),dest);
            }
        };
        linkRemaining(content/"Config/Earth",launch/"Config/Earth");
        linkRemaining(content/"Config",launch/"Config");
        for(const auto &item:fs::directory_iterator(content)) {
            fs::path dest=launch/item.path().filename();
            if(fs::symlink_status(dest).type()!=fs::file_type::not_found) continue;
            if(item.path().extension()==".cfg") copy(item.path(),dest);
            else if(item.is_directory() || item.path().extension()==".dylib") fs::create_symlink(item.path(),dest);
        }
        fs::current_path(launch);
        snapshot=(launch/"preview.ppm").string();
        // Preserve an independent diagnostic fd: freopen may close its stream
        // on failure, so reporting solely to stderr could silently lose the error.
        int diagnostics=dup(STDERR_FILENO);
        if(diagnostics<0) throw std::runtime_error("cannot preserve preview diagnostic stream");
        auto redirect=[&](const fs::path &path,FILE *stream){
            if(!freopen(path.c_str(),"w",stream)) {
                dprintf(diagnostics,"preview log redirection failed: %s\n",path.c_str());
                close(diagnostics);
                throw std::runtime_error("preview log redirection failed: "+path.string());
            }
        };
        redirect(launch/"preview-session.log",stdout);
        redirect(launch/"preview-errors.log",stderr);
        close(diagnostics);
        printf("[core-preview] session=%s content=%s hidden=%u continuous=%u private_atmosphere=EarthAtmJ71G paused_start=1 text_ime=unsupported fixed_patch=KSC\n",pattern.c_str(),root.c_str(),hidden,!hidden);
        args={argv[0],scenario.data(),frames.data(),mode.data(),snapshot.data()}; argc=5; argv=args.data();
    }
};
}
