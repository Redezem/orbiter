// Real OrbiterCore -> GraphicsClient -> Metal lifecycle probe.
// Optional static focus-vessel template mesh diagnostic; no terrain/surfaces/HUD.
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
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>

extern Orbiter *g_pOrbiter;

class CoreMetalClient final : public oapi::GraphicsClient {
public:
    explicit CoreMetalClient(bool present, std::string snapshot = {}, bool engineCamera = false, bool freezeClip = false)
        : GraphicsClient(nullptr), present_(present), engine_camera_(engineCamera), freeze_clip_(freezeClip), snapshot_(std::move(snapshot)) {}
    void setTestCamera(unsigned view) {
        check(engine_camera_ && (view == 1 || view == 2), "invalid controlled camera");
        CameraMode_Track mode;
        mode.SetTarget(visual_object_);
        mode.SetTrackMode(CameraMode_Track::TM_RELATIVE);
        mode.SetPosition(4.0, view == 1 ? 0.4 : 1.1, view == 1 ? 0.3 : 0.5);
        mode.SetFOV(40.0); // full vertical FOV in degrees; engine API aperture is half-FOV radians.
        check(oapiSetCameraMode(mode), "engine camera mode rejected");
        controlled_view_=view;
        printf("[core-metal] test-controlled engine camera view=%u; NOT scenario camera\n",view);
    }
    void attachFocusMesh() {
        if (snapshot_.empty()) return;
        VESSEL *vessel = oapiGetFocusInterface();
        check(vessel != nullptr, "mesh focus missing");
        visual_object_ = vessel->GetHandle();
        RegisterVisObject(visual_object_, reinterpret_cast<VISHANDLE>(this));
        ++visual_register;
        struct SourceVertex { double x,y,z; float shade; };
        std::vector<SourceVertex> source;
        std::vector<uint32_t> indices;
        printf("[core-metal] camera_mode=%s\n",engine_camera_ ? "engine world perspective" : "diagnostic vessel-local orthographic auto-fit fixed-oblique; NOT engine camera");
        const unsigned mesh_count = vessel->GetMeshCount();
        for (unsigned m=0; m<mesh_count; ++m) {
            WORD visibility = vessel->GetMeshVisibilityMode(m);
            if (!(visibility & MESHVIS_EXTERNAL)) continue;
            MESHHANDLE mesh = vessel->GetMeshTemplate(m);
            check(mesh != nullptr, "visible mesh has no preloaded engine template (on-demand unsupported)");
            VECTOR3 offset;
            check(vessel->GetMeshOffset(m, offset), "mesh offset missing");
            unsigned before = indices.size();
            for (DWORD g=0; g<oapiMeshGroupCount(mesh); ++g) {
                MESHGROUP *group = oapiMeshGroup(mesh,g);
                check(group != nullptr, "null engine mesh group");
                // Bit 1 means do not render. Other group flags/textures/materials are not supported.
                if ((group->UsrFlag & 2) || !group->nIdx) continue;
                check(group->Vtx && group->Idx && group->nVtx && group->nIdx % 3 == 0, "invalid triangle group");
                uint32_t base = static_cast<uint32_t>(source.size());
                for (DWORD i=0; i<group->nVtx; ++i) {
                    const auto &v=group->Vtx[i];
                    double x=v.x+offset.x,y=v.y+offset.y,z=v.z+offset.z;
                    check(std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z), "nonfinite mesh vertex");
                    // Right/up/depth form a fixed orthonormal diagnostic view, not engine camera support.
                    source.push_back({engine_camera_ ? x : 0.8*x-0.6*z,
                                      engine_camera_ ? y : -0.36*x+0.8*y-0.48*z,
                                      engine_camera_ ? z : 0.48*x+0.6*y+0.64*z,
                                      0.35f+0.6f*std::abs(0.48f*v.nx+0.6f*v.ny+0.64f*v.nz)});
                }
                for (DWORD i=0;i<group->nIdx;++i) {
                    check(group->Idx[i] < group->nVtx, "mesh index out of bounds");
                    indices.push_back(base+group->Idx[i]);
                }
                ++uploaded_groups;
            }
            if (indices.size() != before) ++uploaded_meshes;
            printf("[core-metal] mesh=%u visibility=%u offset=%.3f,%.3f,%.3f groups=%u\n",
                   m,visibility,offset.x,offset.y,offset.z,oapiMeshGroupCount(mesh));
        }
        check(!source.empty() && !indices.empty(), "no external template triangles");
        double lo[3]={INFINITY,INFINITY,INFINITY}, hi[3]={-INFINITY,-INFINITY,-INFINITY};
        for (const auto &v:source) {
            double p[3]={v.x,v.y,v.z};
            for(int i=0;i<3;++i){lo[i]=std::min(lo[i],p[i]);hi[i]=std::max(hi[i],p[i]);}
        }
        double scale=1.6/std::max((hi[0]-lo[0])*height/width,hi[1]-lo[1]);
        double depth=hi[2]-lo[2];
        check(std::isfinite(scale)&&scale>0&&depth>0, "degenerate mesh bounds");
        struct GPUVertex { float x,y,z,shade; };
        std::vector<GPUVertex> vertices;
        for(const auto &v:source) vertices.push_back({
            float(engine_camera_ ? v.x : (v.x-(lo[0]+hi[0])*0.5)*scale*height/width),
            float(engine_camera_ ? v.y : (v.y-(lo[1]+hi[1])*0.5)*scale),
            float(engine_camera_ ? v.z : 0.1+0.8*(hi[2]-v.z)/depth), std::clamp(v.shade,0.35f,0.95f)});
        sample_local_ = _V(vertices[0].x, vertices[0].y, vertices[0].z);
        mesh_vertices_=[device_ newBufferWithBytes:vertices.data() length:vertices.size()*sizeof(GPUVertex) options:MTLResourceStorageModeShared];
        mesh_indices_=[device_ newBufferWithBytes:indices.data() length:indices.size()*sizeof(uint32_t) options:MTLResourceStorageModeShared];
        uploaded_vertices=vertices.size(); uploaded_triangles=indices.size()/3;
        check(mesh_vertices_ && mesh_indices_, "mesh GPU upload failed");
        NSError *error=nil;
        NSString *shader=@"#include <metal_stdlib>\nusing namespace metal;\nstruct V { float4 p; }; struct O { float4 p [[position]]; float shade; };\nvertex O mesh_vertex(uint i [[vertex_id]], const device V* v [[buffer(0)]], constant float4x4& clip [[buffer(1)]]) { O o; o.p=clip*float4(v[i].p.xyz,1); o.shade=v[i].p.w; return o; }\nfragment float4 mesh_fragment(O i [[stage_in]]) { return float4(i.shade,i.shade,i.shade,1); }";
        id<MTLLibrary> library=[device_ newLibraryWithSource:shader options:nil error:&error];
        if(!library) throw std::runtime_error(error.description.UTF8String);
        auto pipeline=[MTLRenderPipelineDescriptor new];
        pipeline.vertexFunction=[library newFunctionWithName:@"mesh_vertex"];
        pipeline.fragmentFunction=[library newFunctionWithName:@"mesh_fragment"];
        pipeline.colorAttachments[0].pixelFormat=MTLPixelFormatBGRA8Unorm;
        pipeline.depthAttachmentPixelFormat=MTLPixelFormatDepth32Float;
        mesh_pipeline_=[device_ newRenderPipelineStateWithDescriptor:pipeline error:&error];
        if(!mesh_pipeline_) throw std::runtime_error(error.description.UTF8String);
        auto dd=[MTLDepthStencilDescriptor new]; dd.depthCompareFunction=MTLCompareFunctionLess; dd.depthWriteEnabled=YES;
        mesh_depth_state_=[device_ newDepthStencilStateWithDescriptor:dd];
        auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:width height:height mipmapped:NO];
        td.usage=MTLTextureUsageRenderTarget; td.storageMode=MTLStorageModePrivate;
        mesh_depth_=[device_ newTextureWithDescriptor:td];
        check(mesh_depth_state_ && mesh_depth_, "mesh depth allocation failed");
    }
    void detachFocusMesh() {
        if(visual_object_) { UnregisterVisObject(visual_object_); visual_object_=nullptr; ++visual_unregister; }
    }
    int clbkVisEvent(OBJHANDLE, VISHANDLE, DWORD, DWORD_PTR) override {
        ++unsupported_visual_events;
        return 0; // Template snapshot only: never claim animation or instance mutation support.
    }
    ~CoreMetalClient() override { release(); }
    bool clbkInitialise() override {
        SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP, "1");
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
        // Mirror the actual drawable scene for GPU readback; never show the window.
        layer_.framebufferOnly = NO;
        auto desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                     width:width height:height mipmapped:NO];
        desc.usage = MTLTextureUsageRenderTarget;
        desc.storageMode = MTLStorageModeShared;
        target_ = [device_ newTextureWithDescriptor:desc];
        if (!target_) return fail("offscreen texture unavailable");
        if (engine_camera_) {
            mesh_oracle_ = [device_ newTextureWithDescriptor:desc];
            if (!mesh_oracle_) return fail("mesh oracle texture unavailable");
        }
        printf("[core-metal] scene_source=%s pre_ui_oracle=%s\n",present_ ? "drawable" : "offscreen", engine_camera_ ? (present_ ? "drawable" : "offscreen") : "none");
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
        if (engine_camera_) {
            // Preserve GPU scene pixels BEFORE the real engine ImGui compositing.
            [encoder_ endEncoding];
            encoder_ = nil;
            auto copy = [buffer_ blitCommandEncoder];
            [copy copyFromTexture:(present_ ? drawable_.texture : target_) sourceSlice:0 sourceLevel:0
                  sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(width,height,1)
                  toTexture:mesh_oracle_ destinationSlice:0 destinationLevel:0
                  destinationOrigin:MTLOriginMake(0,0,0)];
            [copy endEncoding];
            auto ui = [MTLRenderPassDescriptor renderPassDescriptor];
            ui.colorAttachments[0].texture = present_ ? drawable_.texture : target_;
            ui.colorAttachments[0].loadAction = MTLLoadActionLoad;
            ui.colorAttachments[0].storeAction = MTLStoreActionStore;
            encoder_ = [buffer_ renderCommandEncoderWithDescriptor:ui];
            check(copy && encoder_, "mesh oracle/UI encoding failed");
        }
        ImGui::Render();
        if (encoder_) ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), buffer_, encoder_);
    }
    uint64_t clbkImGuiSurfaceTexture(SURFHANDLE) override { return 0; }
    bool verified(unsigned n) const {
        return updates == n && renders == n && newframes == n && drawdata == n &&
               displays == n && completed == n && errors == 0 && readbacks == n &&
               scheduled == (present_ ? n : 0) && acquired == scheduled && hidden_checks == n && creates == 1 && closes == 1 && destroys == 1 &&
               imgui_init == 1 && imgui_shutdown == 1 &&
               (snapshot_.empty() || (uploaded_meshes > 0 && uploaded_triangles > 0 &&
                 mesh_draw_calls == n && drawn_triangles == uploaded_triangles*n &&
                 geometry_readbacks == n && visual_register == 1 && visual_unregister == 1)) &&
               (!engine_camera_ || camera_frames_ == n);
    }
    void report() const {
        printf("[core-metal] drawable_acquired=%u present_scheduled=%u actual_presented=%u hidden_checks=%u\n",acquired,scheduled,presented,hidden_checks);
        if (present_ && presented != scheduled) printf("[core-metal] hidden-window limitation: positive presentedTime observed for %u/%u submissions within 100ms/frame; scanout NOT established for remaining frames\n",presented,scheduled);
        if(engine_camera_) printf("[core-metal] camera_frames=%u\n",camera_frames_);
        printf("[core-metal] update=%u newframe=%u render=%u drawdata=%u display=%u completed=%u presented=%u readback=%u errors=%u\n",
               updates,newframes,renders,drawdata,displays,completed,presented,readbacks,errors);
        printf("[core-metal] create=%u close=%u destroy=%u imgui_init=%u imgui_shutdown=%u\n",
               creates,closes,destroys,imgui_init,imgui_shutdown);
        if (!snapshot_.empty()) printf("[core-metal] mesh uploads=%u groups=%u vertices=%u triangles=%u draw_calls=%u drawn_triangles=%llu geometry_readbacks=%u visual_register=%u visual_unregister=%u unsupported_events=%u\n",
            uploaded_meshes,uploaded_groups,uploaded_vertices,uploaded_triangles,mesh_draw_calls,
            drawn_triangles,geometry_readbacks,visual_register,visual_unregister,unsupported_visual_events);
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
        check((SDL_GetWindowFlags(window_) & SDL_WINDOW_HIDDEN) &&
              !(SDL_GetWindowFlags(window_) & SDL_WINDOW_INPUT_FOCUS), "probe window visible/focused");
        ++hidden_checks;
        if(engine_camera_) updateEngineCamera();
        drawable_ = present_ ? [layer_ nextDrawable] : nil;
        if (present_ && !drawable_) throw std::runtime_error("hidden layer returned no drawable; presentation unavailable (window will NOT be shown)");
        if (drawable_) ++acquired;
        auto pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = present_ ? drawable_.texture : target_;
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass.colorAttachments[0].clearColor = MTLClearColorMake(0.125, 0.25, 0.5, 1.0);
        if (mesh_depth_) {
            pass.depthAttachment.texture=mesh_depth_;
            pass.depthAttachment.loadAction=MTLLoadActionClear;
            pass.depthAttachment.storeAction=MTLStoreActionDontCare;
            pass.depthAttachment.clearDepth=1.0;
        }
        buffer_ = [queue_ commandBuffer];
        encoder_ = [buffer_ renderCommandEncoderWithDescriptor:pass];
        if (!buffer_ || !encoder_) throw std::runtime_error("Metal encoding failed");
        if (mesh_pipeline_) {
            [encoder_ setRenderPipelineState:mesh_pipeline_];
            [encoder_ setDepthStencilState:mesh_depth_state_];
            [encoder_ setCullMode:MTLCullModeNone];
            [encoder_ setVertexBuffer:mesh_vertices_ offset:0 atIndex:0];
            if (!freeze_clip_ || renders==1) std::copy(clip_,clip_+16,gpu_clip_);
            [encoder_ setVertexBytes:gpu_clip_ length:sizeof(gpu_clip_) atIndex:1];
            if(engine_camera_) {
                printf("[core-metal] gpu_clip_json=[");
                for(int i=0;i<16;++i) printf("%s%.17g",i ? "," : "",double(gpu_clip_[i]));
                printf("]\n");
            }
            [encoder_ drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:uploaded_triangles*3
                indexType:MTLIndexTypeUInt32 indexBuffer:mesh_indices_ indexBufferOffset:0];
            ++mesh_draw_calls; drawn_triangles+=uploaded_triangles;
        }
        // Deliberately no Render2DOverlay: native surfaces/Sketchpad/HUD are not supported.
    }
    bool clbkDisplayFrame() override {
        ++displays;
        check(drawdata == displays, "drawdata/display order");
        [encoder_ endEncoding];
        encoder_ = nil;
        if (present_) {
            check(drawable_.texture.width == width && drawable_.texture.height == height, "drawable size mismatch");
            auto copy = [buffer_ blitCommandEncoder];
            check(copy != nil, "drawable copy encoder unavailable");
            [copy copyFromTexture:drawable_.texture sourceSlice:0 sourceLevel:0
                  sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(width,height,1)
                  toTexture:target_ destinationSlice:0 destinationLevel:0
                  destinationOrigin:MTLOriginMake(0,0,0)];
            [copy endEncoding];
            [buffer_ presentDrawable:drawable_];
            ++scheduled; // Submission request, NOT proof of display scanout.
        }
        [buffer_ commit];
        [buffer_ waitUntilCompleted];
        if (buffer_.status != MTLCommandBufferStatusCompleted || buffer_.error) {
            ++errors;
            fprintf(stderr, "[core-metal] GPU error: %s\n", buffer_.error.description.UTF8String);
            return false;
        }
        ++completed;
        if (present_) {
            // GPU completion does not imply presentation. Hidden windows may never scan out.
            // Bounded wait only; never activate/show a window to force WindowServer progress.
            for (unsigned poll=0; poll<10 && drawable_.presentedTime<=0; ++poll) SDL_Delay(10);
            double time=drawable_.presentedTime;
            if (time>0) ++presented;
            printf("[core-metal] drawable frame=%u id=%llu presented_time=%.9f gpu_completed=1\n",
                   completed,(unsigned long long)drawable_.drawableID,time);
        }
        {
            // Verify actual GPU output, not just callback dispatch. Corner avoids ImGui content.
            unsigned char bgra[4] = {};
            [target_ getBytes:bgra bytesPerRow:4 fromRegion:MTLRegionMake2D(width-1,height-1,1,1) mipmapLevel:0];
            check(bgra[0] == 128 && bgra[1] == 64 && bgra[2] == 32 && bgra[3] == 255, "GPU clear pixel");
            ++readbacks;
            if (mesh_pipeline_) {
                std::vector<unsigned char> pixels(width*height*4);
                id<MTLTexture> source = engine_camera_ ? mesh_oracle_ : target_;
                [source getBytes:pixels.data() bytesPerRow:width*4 fromRegion:MTLRegionMake2D(0,0,width,height) mipmapLevel:0];
                unsigned background=0,geometry=0;
                for (size_t i=0;i<pixels.size();i+=4) {
                    const auto *p=&pixels[i];
                    if(p[0]==128&&p[1]==64&&p[2]==32&&p[3]==255) ++background;
                    if(p[0]==p[1]&&p[1]==p[2]&&p[0]>=80&&p[3]==255) ++geometry;
                }
                check(background>1000 && geometry>1000, "GPU mesh/background pixel counts");
                ++geometry_readbacks;
                if (completed==1 || (controlled_view_==2 && !second_snapshot_)) {
                    std::string path = completed==1 ? snapshot_ : snapshot_+".view2.ppm";
                    FILE *f=fopen(path.c_str(),"wb");
                    check(f!=nullptr,"snapshot open failed");
                    bool ok=fprintf(f,"P6\n%u %u\n255\n",width,height)>0;
                    std::vector<unsigned char> rgb(width*height*3);
                    for(size_t i=0,j=0;i<pixels.size();i+=4,j+=3) {
                        rgb[j]=pixels[i+2];rgb[j+1]=pixels[i+1];rgb[j+2]=pixels[i];
                    }
                    ok=(fwrite(rgb.data(),1,rgb.size(),f)==rgb.size()) && ok;
                    ok=(fclose(f)==0) && ok;
                    check(ok,"snapshot write failed");
                    if(completed!=1) second_snapshot_=true;
                    printf("[core-metal] GPU pixels background=%u geometry=%u snapshot=%s\n",background,geometry,path.c_str());
                }
            }
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
    void updateEngineCamera() {
        VECTOR3 camera, vessel, direction;
        MATRIX3 cameraRotation, vesselRotation;
        oapiCameraGlobalPos(&camera);
        oapiCameraRotationMatrix(&cameraRotation);
        oapiCameraGlobalDir(&direction);
        oapiGetGlobalPos(visual_object_, &vessel);
        oapiGetRotationMatrix(visual_object_, &vesselRotation);
        double aperture=oapiCameraAperture();
        printf("[core-metal] raw camera=%.17g,%.17g,%.17g vessel=%.17g,%.17g,%.17g cameraR=",camera.x,camera.y,camera.z,vessel.x,vessel.y,vessel.z);
        for(int i=0;i<9;++i) printf(" %.17g",cameraRotation.data[i]);
        printf(" vesselR=");
        for(int i=0;i<9;++i) printf(" %.17g",vesselRotation.data[i]);
        printf("\n");
        check(std::isfinite(aperture) && aperture>0 && aperture<1.5707963267948966, "invalid engine aperture");
        // SDK matrices multiply column vectors: vessel local -> global, camera local -> global.
        // Subtract global positions in DOUBLE first (heliocentric values can be ~1e11 m).
        // Do not add each local point to the huge global position before subtracting.
        VECTOR3 relative=vessel-camera;
        VECTOR3 viewTranslation=tmul(cameraRotation,relative);
        MATRIX3 viewRotation=mul(transp(cameraRotation),vesselRotation);
        // Orbiter camera looks along +Z, +X right, +Y up (Camera.cpp / D3D9 Scene.cpp).
        // Metal clip accepts that same X/Y and 0<=Z<=W. No OpenGL -Z or Y flip:
        // Metal's viewport maps +NDC Y upward into the top-left-origin render target.
        // Client-owned bounded vessel frustum, not claimed to be engine near/far policy.
        constexpr double nearPlane=0.1, farPlane=1000000.0;
        double sy=1.0/std::tan(aperture), sx=sy*height/width;
        double a=farPlane/(farPlane-nearPlane), b=-nearPlane*a;
        for(unsigned col=0;col<4;++col) {
            double x=col<3 ? viewRotation.data[col] : viewTranslation.x;
            double y=col<3 ? viewRotation.data[3+col] : viewTranslation.y;
            double z=col<3 ? viewRotation.data[6+col] : viewTranslation.z;
            double values[4]={sx*x,sy*y,a*z+(col==3 ? b : 0),z};
            for(unsigned row=0;row<4;++row) {
                clip_[col*4+row]=static_cast<float>(values[row]);
                check(std::isfinite(clip_[col*4+row]), "nonfinite engine clip matrix");
            }
        }
        ++camera_frames_;
        // Exact input doubles and computed float matrix; GPU binding logged separately.
        printf("[core-metal] camera_json={\"frame\":%u,\"controlled_view\":%u",renders,controlled_view_);
        auto vector=[](const char *key,const VECTOR3 &v) {
            printf(",\"%s\":[%.17g,%.17g,%.17g]",key,v.x,v.y,v.z);
        };
        auto matrix=[](const char *key,const MATRIX3 &m) {
            printf(",\"%s\":[",key);
            for(int i=0;i<9;++i) printf("%s%.17g",i ? "," : "",m.data[i]);
            printf("]");
        };
        vector("camera_position",camera); vector("vessel_position",vessel);
        vector("direction",direction); vector("sample_local",sample_local_);
        matrix("camera_rotation",cameraRotation); matrix("vessel_rotation",vesselRotation);
        printf(",\"aperture\":%.17g,\"near\":%.17g,\"far\":%.17g,\"clip_matrix\":[",aperture,nearPlane,farPlane);
        for(int i=0;i<16;++i) printf("%s%.17g",i ? "," : "",double(clip_[i]));
        printf("]}\n");
    }
    void release() {
        mesh_vertices_=nil; mesh_indices_=nil; mesh_pipeline_=nil; mesh_depth_state_=nil; mesh_depth_=nil;
        mesh_oracle_=nil;
        encoder_ = nil; buffer_ = nil; drawable_ = nil; target_ = nil; queue_ = nil; layer_ = nil; device_ = nil;
        if (view_) SDL_Metal_DestroyView(view_);
        view_ = nullptr;
        if (window_) SDL_DestroyWindow(window_);
        window_ = nullptr;
        if (sdl_) SDL_Quit();
        sdl_ = false;
    }
    bool present_, engine_camera_=false, freeze_clip_=false, sdl_ = false, imgui_ready_ = false;
    float gpu_clip_[16]={};
    unsigned camera_frames_=0, controlled_view_=0;
    bool second_snapshot_=false;
    VECTOR3 sample_local_={};
    // Column-major Metal matrix. Diagnostic vertices are already clip coordinates.
    float clip_[16]={1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    std::string snapshot_;
    OBJHANDLE visual_object_=nullptr;
    id<MTLBuffer> mesh_vertices_=nil, mesh_indices_=nil;
    id<MTLRenderPipelineState> mesh_pipeline_=nil;
    id<MTLDepthStencilState> mesh_depth_state_=nil;
    id<MTLTexture> mesh_depth_=nil;
    id<MTLTexture> mesh_oracle_=nil;
    unsigned uploaded_meshes=0,uploaded_groups=0,uploaded_vertices=0,uploaded_triangles=0,mesh_draw_calls=0;
    unsigned geometry_readbacks=0,visual_register=0,visual_unregister=0,unsupported_visual_events=0;
    unsigned long long drawn_triangles=0;
    SDL_Window *window_ = nullptr;
    SDL_MetalView view_ = nullptr;
    id<MTLDevice> device_ = nil;
    id<MTLCommandQueue> queue_ = nil;
    CAMetalLayer *layer_ = nil;
    id<MTLTexture> target_ = nil;
    id<CAMetalDrawable> drawable_ = nil;
    id<MTLCommandBuffer> buffer_ = nil;
    id<MTLRenderCommandEncoder> encoder_ = nil;
    unsigned acquired=0,scheduled=0,hidden_checks=0;
    unsigned updates=0,newframes=0,renders=0,drawdata=0,displays=0,completed=0,presented=0,readbacks=0,errors=0;
    unsigned creates=0,closes=0,destroys=0,imgui_init=0,imgui_shutdown=0;
};

int main(int argc, char **argv) {
    if (argc < 3 || argc > 6) {
        fprintf(stderr,"Usage (from content root): orbiter-core-metal <scenario> <frames 1..10000> [--mesh snapshot.ppm | --engine-camera snapshot.ppm | --engine-camera-test snapshot.ppm | --diagnostic-frozen-camera-test snapshot.ppm] [--hidden-present]\n");
        return 2;
    }
    char *end = nullptr;
    long frames = strtol(argv[2], &end, 10);
    bool present = argc>=4 && std::string(argv[argc-1])=="--hidden-present";
    int modeArgc=argc-(present ? 1 : 0);
    if (!end || *end || frames < 1 || frames > 10000 ||
        (modeArgc != 3 && modeArgc != 5) ||
        (modeArgc == 5 && ((std::string(argv[3]) != "--mesh" && std::string(argv[3]) != "--engine-camera" && std::string(argv[3]) != "--engine-camera-test" && std::string(argv[3]) != "--diagnostic-frozen-camera-test") || !argv[4][0]))) return 2;
    bool engineCamera=modeArgc==5 && std::string(argv[3])!="--mesh";
    bool freezeClip=modeArgc==5 && std::string(argv[3])=="--diagnostic-frozen-camera-test";
    bool cameraTest=modeArgc==5 && (std::string(argv[3])=="--engine-camera-test" || freezeClip);
    if(cameraTest && frames<2) return 2;
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
            client = new CoreMetalClient(present, modeArgc == 5 ? argv[4] : "", engineCamera, freezeClip);
            if(freezeClip) printf("[core-metal] DIAGNOSTIC NEGATIVE CONTROL: GPU clip frozen at frame 1; API camera still changes\n");
            attached = oapiRegisterGraphicsClient(client);
            if (!attached || !client->clbkInitialise()) throw std::runtime_error("graphics registration/init failed");
            launched = true;
            g_pOrbiter->Launch(argv[1]);
            if (!oapiGetFocusInterface()) throw std::runtime_error("no focus vessel");
            printf("[core-metal] focus=%s scenario=%s frames=%ld\n", oapiGetFocusInterface()->GetName(), argv[1], frames);
            client->attachFocusMesh();
            for (long i=0; i<frames; ++i) { @autoreleasepool {
                if(cameraTest && (i==0 || i==frames/2)) client->setTestCamera(i==0 ? 1 : 2);
                g_pOrbiter->SingleFrame();
            } }
            client->detachFocusMesh();
            g_pOrbiter->CloseSession();
            launched = false;
            result = client->verified(static_cast<unsigned>(frames)) ? 0 : 1;
        } catch (const std::exception &e) {
            fprintf(stderr,"[core-metal] FAIL: %s\n", e.what());
        }
        if (launched) { if(client) client->detachFocusMesh(); g_pOrbiter->CloseSession(); }
        if (client) client->report();
        if (attached && !oapiUnregisterGraphicsClient(client)) result = 1;
        delete client;
        delete g_pOrbiter;
        g_pOrbiter = nullptr;
        printf("[core-metal] teardown complete; %s\n", result == 0 ? "PASS" : "FAIL");
        return result;
    }
}
