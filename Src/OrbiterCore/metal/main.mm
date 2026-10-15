// Real OrbiterCore -> GraphicsClient -> Metal lifecycle probe.
// No mesh, terrain, native surfaces, Sketchpad, or HUD implementation yet.
// Keep the engine's Win32-compatible int BOOL distinct from Objective-C's bool BOOL.
// Renaming the alias locally preserves the C++ ABI without changing shared headers.
#define BOOL OrbiterWinBOOL
#include <windows.h>
#include "Orbiter.h"
#include "OrbiterAPI.h"
#include "GraphicsAPI.h"
#include "cmdline.h"
#include "Log.h"
#include "Input.h"
#undef BOOL
#include "imgui.h"
#include "backends/imgui_impl_metal.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

extern Orbiter *g_pOrbiter;

class CoreMetalClient final : public oapi::GraphicsClient {
public:
    explicit CoreMetalClient(bool present) : GraphicsClient(nullptr), present_(present) {}
    ~CoreMetalClient() override { release(); }
    bool clbkInitialise() override {
        if (!SDL_Init(SDL_INIT_VIDEO)) return fail(SDL_GetError());
        sdl_ = true;
        window_ = SDL_CreateWindow("Orbiter core Metal probe", width, height,
                                  SDL_WINDOW_METAL | SDL_WINDOW_HIDDEN);
        if (!window_) return fail(SDL_GetError());
        view_ = SDL_Metal_CreateView(window_);
        if (!view_) return fail(SDL_GetError());
        device_ = MTLCreateSystemDefaultDevice();
        queue_ = [device_ newCommandQueue];
        if (!device_ || !queue_) return fail("Metal device/queue unavailable");
        layer_ = (__bridge CAMetalLayer *)SDL_Metal_GetLayer(view_);
        layer_.device = device_;
        layer_.pixelFormat = MTLPixelFormatBGRA8Unorm;
        layer_.drawableSize = CGSizeMake(width, height);
        layer_.allowsNextDrawableTimeout = YES;
        auto desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                     width:width height:height mipmapped:NO];
        desc.usage = MTLTextureUsageRenderTarget;
        desc.storageMode = MTLStorageModeShared;
        target_ = [device_ newTextureWithDescriptor:desc];
        if (!target_) return fail("offscreen texture unavailable");
        printf("[core-metal] device=%s mode=%s hidden=1\n", device_.name.UTF8String,
               present_ ? "hidden-present" : "offscreen");
        return true;
    }
    bool clbkFullscreenMode() const override { return false; }
    void clbkGetViewportSize(DWORD *w, DWORD *h) const override { *w = width; *h = height; }
    bool clbkGetRenderParam(DWORD, DWORD *) const override { return false; }
    void clbkImGuiInit() override {
        ++imgui_init;
        auto &io = ImGui::GetIO();
        // No platform windows/input in this bounded, hidden probe. The engine owns contexts.
        io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(width, height);
        if (!ImGui_ImplMetal_Init(device_)) throw std::runtime_error("ImGui Metal init failed");
        imgui_ready_ = true;
    }
    void clbkImGuiShutdown() override {
        ++imgui_shutdown;
        if (imgui_ready_) ImGui_ImplMetal_Shutdown();
        imgui_ready_ = false;
    }
    void clbkImGuiNewFrame() override {
        ++newframes;
        check(updates == renders + 1 && newframes == updates, "update/newframe order");
        auto &io = ImGui::GetIO();
        io.DisplaySize = ImVec2(width, height);
        io.DeltaTime = 1.0f / 60.0f;
        // Backend preparation only: DialogManager calls ImGui::NewFrame itself.
        auto pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = target_;
        ImGui_ImplMetal_NewFrame(pass);
    }
    void clbkImGuiRenderDrawData() override {
        ++drawdata;
        check(renders == drawdata && displays + 1 == renders, "render/drawdata order");
        ImGui::Render();
        if (encoder_) ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), buffer_, encoder_);
    }
    uint64_t clbkImGuiSurfaceTexture(SURFHANDLE) override { return 0; }
    bool verified(unsigned n) const {
        return updates == n && renders == n && newframes == n && drawdata == n &&
               displays == n && completed == n && errors == 0 && readbacks == (present_ ? 0 : n) &&
               presented == (present_ ? n : 0) && creates == 1 && closes == 1 && destroys == 1 &&
               imgui_init == 1 && imgui_shutdown == 1;
    }
    void report() const {
        printf("[core-metal] update=%u newframe=%u render=%u drawdata=%u display=%u completed=%u presented=%u readback=%u errors=%u\n",
               updates,newframes,renders,drawdata,displays,completed,presented,readbacks,errors);
        printf("[core-metal] create=%u close=%u destroy=%u imgui_init=%u imgui_shutdown=%u\n",
               creates,closes,destroys,imgui_init,imgui_shutdown);
    }
protected:
    HWND clbkCreateRenderWindow() override {
        ++creates;
        // A stable native token is required for engine Pane creation, not a Win32 window.
        return reinterpret_cast<HWND>(window_);
    }
    void clbkUpdate(bool) override { ++updates; }
    void clbkRenderScene() override {
        ++renders;
        check(newframes == renders && displays + 1 == renders, "newframe/render order");
        drawable_ = present_ ? [layer_ nextDrawable] : nil;
        if (present_ && !drawable_) throw std::runtime_error("hidden layer returned no drawable");
        auto pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = present_ ? drawable_.texture : target_;
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass.colorAttachments[0].clearColor = MTLClearColorMake(0.125, 0.25, 0.5, 1.0);
        buffer_ = [queue_ commandBuffer];
        encoder_ = [buffer_ renderCommandEncoderWithDescriptor:pass];
        if (!buffer_ || !encoder_) throw std::runtime_error("Metal encoding failed");
        // Deliberately no Render2DOverlay: native surfaces/Sketchpad/HUD are not supported.
    }
    bool clbkDisplayFrame() override {
        ++displays;
        check(drawdata == displays, "drawdata/display order");
        [encoder_ endEncoding];
        encoder_ = nil;
        if (present_) [buffer_ presentDrawable:drawable_];
        [buffer_ commit];
        [buffer_ waitUntilCompleted];
        if (buffer_.status != MTLCommandBufferStatusCompleted || buffer_.error) {
            ++errors;
            fprintf(stderr, "[core-metal] GPU error: %s\n", buffer_.error.description.UTF8String);
            return false;
        }
        ++completed;
        if (present_) ++presented;
        else {
            // Verify actual GPU output, not just callback dispatch. Corner avoids ImGui content.
            unsigned char bgra[4] = {};
            [target_ getBytes:bgra bytesPerRow:4 fromRegion:MTLRegionMake2D(width-1,height-1,1,1) mipmapLevel:0];
            check(bgra[0] == 128 && bgra[1] == 64 && bgra[2] == 32 && bgra[3] == 255, "GPU clear pixel");
            ++readbacks;
        }
        buffer_ = nil;
        drawable_ = nil;
        return true;
    }
    void clbkCloseSession(bool fast) override { ++closes; check(!fast, "fast close disabled"); }
    void clbkDestroyRenderWindow(bool fast) override {
        ++destroys;
        check(!fast, "fast destroy disabled");
        release();
    }
private:
    static constexpr DWORD width = 640, height = 480;
    static void check(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
    bool fail(const char *message) { fprintf(stderr,"[core-metal] init: %s\n",message); return false; }
    void release() {
        encoder_ = nil; buffer_ = nil; drawable_ = nil; target_ = nil; queue_ = nil; layer_ = nil; device_ = nil;
        if (view_) SDL_Metal_DestroyView(view_);
        view_ = nullptr;
        if (window_) SDL_DestroyWindow(window_);
        window_ = nullptr;
        if (sdl_) SDL_Quit();
        sdl_ = false;
    }
    bool present_, sdl_ = false, imgui_ready_ = false;
    SDL_Window *window_ = nullptr;
    SDL_MetalView view_ = nullptr;
    id<MTLDevice> device_ = nil;
    id<MTLCommandQueue> queue_ = nil;
    CAMetalLayer *layer_ = nil;
    id<MTLTexture> target_ = nil;
    id<CAMetalDrawable> drawable_ = nil;
    id<MTLCommandBuffer> buffer_ = nil;
    id<MTLRenderCommandEncoder> encoder_ = nil;
    unsigned updates=0,newframes=0,renders=0,drawdata=0,displays=0,completed=0,presented=0,readbacks=0,errors=0;
    unsigned creates=0,closes=0,destroys=0,imgui_init=0,imgui_shutdown=0;
};

int main(int argc, char **argv) {
    if (argc < 3 || argc > 4) {
        fprintf(stderr,"Usage (from content root): orbiter-core-metal <scenario> <frames 1..10000> [--hidden-present]\n");
        return 2;
    }
    char *end = nullptr;
    long frames = strtol(argv[2], &end, 10);
    if (!end || *end || frames < 1 || frames > 10000 ||
        (argc == 4 && std::string(argv[3]) != "--hidden-present")) return 2;
    setvbuf(stdout, nullptr, _IONBF, 0);
    @autoreleasepool {
        g_pOrbiter = new Orbiter;
        CoreMetalClient *client = nullptr;
        bool attached = false, launched = false;
        int result = 1;
        try {
            std::string cmd = "--scenariox=\"" + std::string(argv[1]) + "\"";
            orbiter::CommandLine::Parse(g_pOrbiter, cmd.data());
            INITLOG("Orbiter_core_metal.log", false);
            if (g_pOrbiter->Create(nullptr) != S_OK) throw std::runtime_error("Orbiter::Create failed");
            // Graphics sessions require the core-owned input framework before Launch.
            if (!g_pOrbiter->GetDInput())
                throw std::runtime_error("core native Create returned no CDIFramework7");
            g_pOrbiter->Cfg()->CfgDebugPrm.ShutdownMode = 0;
            g_pOrbiter->SetFastExit(false);
            client = new CoreMetalClient(argc == 4);
            attached = oapiRegisterGraphicsClient(client);
            if (!attached || !client->clbkInitialise()) throw std::runtime_error("graphics registration/init failed");
            launched = true;
            g_pOrbiter->Launch(argv[1]);
            if (!oapiGetFocusInterface()) throw std::runtime_error("no focus vessel");
            printf("[core-metal] focus=%s scenario=%s frames=%ld\n", oapiGetFocusInterface()->GetName(), argv[1], frames);
            for (long i=0; i<frames; ++i) { @autoreleasepool { g_pOrbiter->SingleFrame(); } }
            g_pOrbiter->CloseSession();
            launched = false;
            client->report();
            result = client->verified(static_cast<unsigned>(frames)) ? 0 : 1;
        } catch (const std::exception &e) {
            fprintf(stderr,"[core-metal] FAIL: %s\n", e.what());
        }
        if (launched) g_pOrbiter->CloseSession();
        if (attached && !oapiUnregisterGraphicsClient(client)) result = 1;
        delete client;
        delete g_pOrbiter;
        g_pOrbiter = nullptr;
        printf("[core-metal] teardown complete; %s\n", result == 0 ? "PASS" : "FAIL");
        return result;
    }
}
