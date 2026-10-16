// Real OrbiterCore -> GraphicsClient -> Metal lifecycle probe.
// Static templates, opt-in DG gear, and bounded anchored terrain; no HUD/full app.
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
#include <array>
#include "material_semantics.h"
#include "metal_textures.h"
#include "texture_evidence.h"
#include "session_input.h"
#include "preview_keymap.h"
#include "session_control_probe.h"
#include "buffered_control_probe.h"
#include "laptop_control_probe.h"
#include "imgui_platform.h"
#include "backends/imgui_impl_sdl3.h"
#include "imgui_input_probe.h"
#include "animation_contract_probe.h"
#include "gear_visual.h"
#include "planet_surface.h"
#include "terrain_patch.h"
#include "preview_startup.h"

extern Orbiter *g_pOrbiter;

class CoreMetalClient final : public oapi::GraphicsClient {
public:
    explicit CoreMetalClient(bool present, std::string snapshot = {}, bool engineCamera = false, bool freezeClip = false, bool materials = false, bool textures = false, bool ignoreSampling = false)
        : GraphicsClient(nullptr), present_(present), engine_camera_(engineCamera), freeze_clip_(freezeClip), materials_(materials), textures_(textures), ignore_sampling_(ignoreSampling), snapshot_(std::move(snapshot)) {}
    void setTestCamera(unsigned view, bool materialViews=false) {
        check(engine_camera_ && (view == 1 || view == 2), "invalid controlled camera");
        if(terrain_ && !terrainVessel) { terrain_->camera(view); controlled_view_=view; return; }
        CameraMode_Track mode;
        mode.SetTarget(planet_ ? planet_->body() : visual_object_);
        mode.SetTrackMode(CameraMode_Track::TM_RELATIVE);
        // Material views look down onto actual canopy materials rather than the
        // original camera regression's all-white underside. Original views unchanged.
        if(terrainVessel) mode.SetPosition(4.0,view==1?2.74:2.04,.02);
        else if(planet_) mode.SetPosition(3.0,view==1 ? 0.4 : 2.1,view==1 ? 0.3 : -0.3);
        else mode.SetPosition(textures_ ? 2.8 : 4.0, materialViews ? (view == 1 ? 2.74 : 2.04) : (view == 1 ? 0.4 : 1.1), materialViews ? (view == 1 ? -0.6 : -0.8) : (view == 1 ? 0.3 : 0.5));
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
        if(gearTest) { gear_=std::make_unique<core_metal::GearVisual>(); gear_->attach(vessel);
            check(vessel->GetDevMesh(reinterpret_cast<VISHANDLE>(this),0)==gear_->handle && !clbkGetMesh(reinterpret_cast<VISHANDLE>(1),0) && !clbkGetMesh(reinterpret_cast<VISHANDLE>(this),1),"gear exact device mesh lookup");
            check(clbkGetMeshGroup(reinterpret_cast<DEVMESHHANDLE>(gear_->handle),0,nullptr)==-2 && clbkEditMeshGroup(reinterpret_cast<DEVMESHHANDLE>(gear_->handle),0,nullptr)==-2,"unsupported device group APIs must reject");
            printf("[core-metal] gear_device_lookup exact=1 foreign_visual_rejected=1 foreign_index_rejected=1 get_edit=unsupported\n");
        }
        std::vector<std::array<float,4>> normals;
        struct SourceVertex { double x,y,z; float shade; };
        std::vector<SourceVertex> source;
        std::vector<uint32_t> indices;
        std::vector<std::array<float,4>> colors;
        std::vector<std::array<float,2>> uvs;
        if(materials_) printf("[core-metal] materials_mode=opaque-diffuse-only textures=%s; alpha stored but not blended; no specular/emissive/scene lighting\n",textures_ ? "mip0-uv-diffuse" : "unsupported");
        printf("[core-metal] camera_mode=%s\n",engine_camera_ ? "engine world perspective" : "diagnostic vessel-local orthographic auto-fit fixed-oblique; NOT engine camera");
        const unsigned mesh_count = vessel->GetMeshCount();
        for (unsigned m=0; m<mesh_count; ++m) {
            WORD visibility = vessel->GetMeshVisibilityMode(m);
            if (!(visibility & MESHVIS_EXTERNAL)) continue;
            if(gear_) check(m==0,"gear slice supports only external mesh 0");
            MESHHANDLE mesh = vessel->GetMeshTemplate(m);
            check(mesh != nullptr, "visible mesh has no preloaded engine template (on-demand unsupported)");
            VECTOR3 offset;
            check(vessel->GetMeshOffset(m, offset), "mesh offset missing");
            unsigned before = indices.size();
            if(materials_) {
                unsigned available=0;
                for(DWORD t=1;t<=oapiMeshTextureCount(mesh);++t) {
                    auto handle=oapiGetTextureHandle(mesh,t);
                    if(handle) ++available;
                    if(textures_) {
                        const auto *entry=texture_store_->lookup(handle);
                        check(entry!=nullptr,"unavailable template texture slot");
                        printf("[core-metal] texture_slot slot=%u handle=%p path=%s\n",t,handle,entry->path.c_str());
                    }
                }
                printf("[core-metal] material_mesh=%u engine_materials=%u engine_textures=%u nonnull_texture_handles=%u\n",m,oapiMeshMaterialCount(mesh),oapiMeshTextureCount(mesh),available);
            }
            unsigned previousMaterial=core_metal::defaultMaterial;
            unsigned previousTexture=0;
            for (DWORD g=0; g<oapiMeshGroupCount(mesh); ++g) {
                MESHGROUP *group = oapiMeshGroup(mesh,g);
                check(group != nullptr, "null engine mesh group");
                std::array<float,4> diffuse={1,1,1,1};
                void *textureHandle=nullptr;
                if(textures_) {
                    unsigned index=core_metal::resolveTexture(group->TexIdx,previousTexture,oapiMeshTextureCount(mesh));
                    if(index) {
                        textureHandle=oapiGetTextureHandle(mesh,index);
                        check(textureHandle && texture_store_->lookup(textureHandle),"engine group texture unavailable or foreign handle");
                    }
                    printf("[core-metal] texture_group mesh=%u group=%u raw=%u resolved=%u handle=%p\n",m,g,group->TexIdx,index,textureHandle);
                }
                if(materials_) {
                    unsigned resolved=core_metal::resolveMaterial(group->MtrlIdx,previousMaterial,oapiMeshMaterialCount(mesh));
                    if(resolved!=core_metal::defaultMaterial) {
                        const MATERIAL *material=oapiMeshMaterial(mesh,resolved);
                        check(material!=nullptr,"engine material missing");
                        const auto &d=material->diffuse;
                        check(core_metal::validDiffuse(d.r,d.g,d.b,d.a),"unsupported diffuse value (requires finite normalized RGBA)");
                        diffuse={d.r,d.g,d.b,d.a};
                    }
                    printf("[core-metal] material_json={\"mesh\":%u,\"group\":%u,\"raw\":%u,\"resolved\":%u,\"source\":\"%s\",\"texture_index\":%u,\"diffuse\":[%.9g,%.9g,%.9g,%.9g]}\n",m,g,group->MtrlIdx,resolved,resolved==core_metal::defaultMaterial ? "default" : "engine",group->TexIdx,diffuse[0],diffuse[1],diffuse[2],diffuse[3]);
                }
                // Bit 1 means do not render. Other flags/textures remain unsupported;
                // the opt-in material pass only consumes the resolved diffuse RGB.
                if ((group->UsrFlag & 2) || !group->nIdx) continue;
                if(materials_) material_palette_.push_back(diffuse);
                check(group->Vtx && group->Idx && group->nVtx && group->nIdx % 3 == 0, "invalid triangle group");
                uint32_t base = static_cast<uint32_t>(source.size());
                if(gear_) gear_->draws.push_back({g,base,indices.size(),group->nIdx});
                if(textures_) {
                    if(textureHandle) check(texture_store_->pin(textureHandle),"texture mesh pin failed");
                    texture_ranges_.push_back({indices.size(),group->nIdx,textureHandle});
                }
                for (DWORD i=0; i<group->nVtx; ++i) {
                    const auto &v=group->Vtx[i];
                    double x=v.x+(gear_?0:offset.x),y=v.y+(gear_?0:offset.y),z=v.z+(gear_?0:offset.z);
                    if(gear_) {check(std::isfinite(v.nx)&&std::isfinite(v.ny)&&std::isfinite(v.nz),"gear nonfinite source normal"); normals.push_back({v.nx,v.ny,v.nz,0});}
                    check(std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z), "nonfinite mesh vertex");
                    // Right/up/depth form a fixed orthonormal diagnostic view, not engine camera support.
                    source.push_back({engine_camera_ ? x : 0.8*x-0.6*z,
                                      engine_camera_ ? y : -0.36*x+0.8*y-0.48*z,
                                      engine_camera_ ? z : 0.48*x+0.6*y+0.64*z,
                                      0.35f+0.6f*std::abs(0.48f*v.nx+0.6f*v.ny+0.64f*v.nz)});
                    if(materials_) colors.push_back(diffuse);
                    if(textures_) {
                        check(std::isfinite(v.tu)&&std::isfinite(v.tv),"nonfinite engine UV");
                        uvs.push_back({v.tu,v.tv});
                    }
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
        if(textures_) core_metal::writeTextureGeometry(snapshot_+".geometry.json",vertices,uvs,colors,indices,texture_ranges_,*texture_store_);
        mesh_vertices_=[device_ newBufferWithBytes:vertices.data() length:vertices.size()*sizeof(GPUVertex) options:MTLResourceStorageModeShared];
        mesh_indices_=[device_ newBufferWithBytes:indices.data() length:indices.size()*sizeof(uint32_t) options:MTLResourceStorageModeShared];
        uploaded_vertices=vertices.size(); uploaded_triangles=indices.size()/3;
        check(mesh_vertices_ && mesh_indices_, "mesh GPU upload failed");
        if(materials_) {
            check(colors.size()==vertices.size(),"material vertex count");
            material_colors_=[device_ newBufferWithBytes:colors.data() length:colors.size()*sizeof(colors[0]) options:MTLResourceStorageModeShared];
            check(material_colors_!=nil,"material GPU upload failed");
        }
        if(gear_) {
            gear_normals_=[device_ newBufferWithBytes:normals.data() length:normals.size()*sizeof(normals[0]) options:MTLResourceStorageModeShared];
            check(gear_normals_!=nil,"gear normals upload");
            gear_->exportSource(snapshot_+".gear-source.json");
        }
        NSError *error=nil;
        if(textures_) {
            texture_uvs_=[device_ newBufferWithBytes:uvs.data() length:uvs.size()*sizeof(uvs[0]) options:MTLResourceStorageModeShared];
            check(texture_uvs_!=nil,"UV upload failed");
        }
        NSString *shader=@"#include <metal_stdlib>\nusing namespace metal;\nstruct V { float4 p; }; struct O { float4 p [[position]]; float shade; };\nvertex O mesh_vertex(uint i [[vertex_id]], const device V* v [[buffer(0)]], constant float4x4& clip [[buffer(1)]]) { O o; o.p=clip*float4(v[i].p.xyz,1); o.shade=v[i].p.w; return o; }\nfragment float4 mesh_fragment(O i [[stage_in]]) { return float4(i.shade,i.shade,i.shade,1); }";
        if(materials_) shader=@"#include <metal_stdlib>\nusing namespace metal;\nstruct V { float4 p; }; struct O { float4 p [[position]]; float shade; float4 diffuse; };\nvertex O mesh_vertex(uint i [[vertex_id]], const device V* v [[buffer(0)]], constant float4x4& clip [[buffer(1)]], const device float4* colors [[buffer(2)]]) { O o; o.p=clip*float4(v[i].p.xyz,1); o.shade=v[i].p.w; o.diffuse=colors[i]; return o; }\nfragment float4 mesh_fragment(O i [[stage_in]]) { return float4(i.diffuse.rgb*i.shade,1); }";
        if(textures_) shader=@"#include <metal_stdlib>\nusing namespace metal;\nstruct V { float4 p; }; struct O { float4 p [[position]]; float shade; float4 diffuse; float2 uv; };\nvertex O mesh_vertex(uint i [[vertex_id]], const device V* v [[buffer(0)]], constant float4x4& clip [[buffer(1)]], const device float4* colors [[buffer(2)]], const device float2* uv [[buffer(3)]]) { O o; o.p=clip*float4(v[i].p.xyz,1); o.shade=v[i].p.w; o.diffuse=colors[i]; o.uv=uv[i]; return o; }\nfragment float4 mesh_fragment(O i [[stage_in]], texture2d<float> tex [[texture(0)]], constant uint& enabled [[buffer(0)]]) { constexpr sampler s(coord::normalized,address::repeat,filter::nearest,mip_filter::none); float3 sample=float3(1); if(enabled) sample=tex.sample(s,i.uv,level(0)).rgb; return float4(sample*i.diffuse.rgb*i.shade,1); }";
        if(gear_) {
            shader=[shader stringByReplacingOccurrencesOfString:@"constant float4x4& clip [[buffer(1)]]" withString:@"constant float4x4& clip [[buffer(1)]], const device float4* normals [[buffer(4)]], constant float4x4& group [[buffer(5)]], constant float4& offset [[buffer(6)]]"];
            shader=[shader stringByReplacingOccurrencesOfString:@"o.p=clip*float4(v[i].p.xyz,1); o.shade=v[i].p.w;" withString:@"float3 p=(group*float4(v[i].p.xyz,1)).xyz+offset.xyz; float3 n=(group*float4(normals[i].xyz,0)).xyz; o.p=clip*float4(p,1); o.shade=clamp(0.35f+0.6f*abs(dot(float3(0.48f,0.6f,0.64f),n)),0.35f,0.95f);"];
        }
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
        if(terrainMode) {
            terrain_=std::make_unique<core_metal::TerrainPatch>();
            terrain_->coast=terrainCoast; terrain_->flat=terrainFlat; terrain_->freeze=terrainFreeze; terrain_->swapUV=terrainSwap; terrain_->combined=terrainVessel;
            terrain_->attach(*this,device_);
            if(terrainVessel) setTestCamera(1,true); else terrain_->camera(1);
            controlled_view_=1;
        }
        if(planetMode) {
            planet_=std::make_unique<core_metal::PlanetSurface>();
            planet_->freeze=planetFreeze; planet_->ignoreSampling=planetIgnore; planet_->swap=planetSwap;
            planet_->attach(*this,device_);
        }
    }
    MESHHANDLE clbkGetMesh(VISHANDLE visual, DWORD index) override {
        return gear_ && visual_object_ && visual==reinterpret_cast<VISHANDLE>(this) && index==0 && gear_->handles.valid(gear_->handle) ? gear_->handle : nullptr;
    }
    bool terrainMode=false,terrainCoast=false,terrainFlat=false,terrainFreeze=false,terrainSwap=false;
    bool terrainVessel=false,omitVessel=false,omitTerrain=false,disableSceneDepth=false,terrainLast=false;
    bool planetMode=false,planetFreeze=false,planetIgnore=false,planetSwap=false;
    bool gearTest=false,gearFreeze=false,gearCleanupFault=false;
    unsigned gearSplit=0;
    bool detachFocusMesh() noexcept {
        bool ok=true;
        terrain_.reset();
        planet_.reset();
        if(gear_) {
            if(gearCleanupFault) gear_->validationFaultForTest=true;
            ok &= gear_->detach();
            // One bounded retry for a transient setter failure, before world close.
            // Keep the first failure even when the retry succeeds.
            if(gear_->restorationPending()) ok &= gear_->detach();
            ok &= core_metal::cleanupStep("gear token",[&]{check(!clbkGetMesh(reinterpret_cast<VISHANDLE>(this),0),"retired device mesh exposed");});
            gear_normals_=nil;
        }

        if(texture_store_) {
            bool pinsOK=true;
            for(auto &range:texture_ranges_) if(range.handle)
                pinsOK &= core_metal::cleanupStep("texture unpin",[&]{texture_store_->unpin(range.handle);});
            ok &= pinsOK;
            texture_ranges_.clear(); texture_uvs_=nil;
            printf("[core-metal] texture_mesh_pins_released=%u before_engine_teardown=1\n",pinsOK);
        }
        if(visual_object_) {
            ok &= core_metal::cleanupStep("visual unregister",[&]{UnregisterVisObject(visual_object_); ++visual_unregister; printf("[core-metal] visual_unregistered=1 before_engine_teardown=1\n");});
            visual_object_=nullptr;
        }
        // All command buffers are synchronously completed; no engine material pointer
        // was retained. Drop the material GPU copy before world/template destruction.
        if(materials_ && material_colors_) {
            material_colors_=nil;
            printf("[core-metal] material_resources_released=1 before_engine_teardown=1\n");
        }
        return ok;
    }
    int clbkVisEvent(OBJHANDLE object, VISHANDLE visual, DWORD message, DWORD_PTR context) override {
        ++unsupported_visual_events;
        if(animationContract) {
            lastVisualEvent=message; lastVisualContext=context;
            lastVisualIdentityOK=object==visual_object_ && visual==reinterpret_cast<VISHANDLE>(this);
        }
        return 0; // Template snapshot only: never claim animation or instance mutation support.
    }
    bool animationContract=false, lastVisualIdentityOK=false;
    DWORD lastVisualEvent=0;
    DWORD_PTR lastVisualContext=0;
    unsigned visualEventCount() const { return unsupported_visual_events; }
    ~CoreMetalClient() override { release(); }
    bool platformInput=false, visiblePreview=false;
    void updatePreviewTitle() {
        SDL_SetWindowTitle(window_,oapiGetPause() ? "Orbiter Core Preview — PAUSED; Ctrl+P resume — fixed KSC patch" : "Orbiter Core Preview — RUNNING; Ctrl+P pause — fixed KSC patch");
    }
    SDL_WindowID windowID() const { return SDL_GetWindowID(window_); }
    bool clbkInitialise() override {
        SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP, visiblePreview ? "0" : "1");
        if(platformInput) {
            SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER,"1");
            SDL_SetHint(SDL_HINT_MOUSE_AUTO_CAPTURE,"0");
        }
        if (!SDL_Init(SDL_INIT_VIDEO)) return fail(SDL_GetError());
        sdl_ = true;
        window_ = SDL_CreateWindow(visiblePreview ? "Orbiter Core Preview — paused; Ctrl+P resume — fixed KSC patch" : "Orbiter core Metal probe", width, height,
                                  SDL_WINDOW_METAL | (visiblePreview ? 0 : SDL_WINDOW_HIDDEN));
        if (!window_) return fail(SDL_GetError());
        view_ = SDL_Metal_CreateView(window_);
        if (!view_) return fail(SDL_GetError());
        device_ = MTLCreateSystemDefaultDevice();
        queue_ = [device_ newCommandQueue];
        if (!device_ || !queue_) return fail("Metal device/queue unavailable");
        if(textures_) {
            auto &dirs=g_pOrbiter->Cfg()->CfgDirPrm;
            texture_store_=core_metal::makeMetalTextures(device_,dirs.HightexDir,dirs.TextureDir);
            printf("[core-metal] texture_roots high=%s standard=%s; RGBA8Unorm shared mip0 nearest-repeat opaque\n",dirs.HightexDir,dirs.TextureDir);
        }
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
        printf("[core-metal] device=%s mode=%s hidden=%u\n", device_.name.UTF8String,
               visiblePreview ? "visible-preview" : (present_ ? "hidden-present" : "offscreen"),!visiblePreview);
        return true;
    }
    bool clbkFullscreenMode() const override { return false; }
    void clbkGetViewportSize(DWORD *w, DWORD *h) const override { *w = width; *h = height; }
    bool clbkGetRenderParam(DWORD, DWORD *) const override { return false; }
    void clbkImGuiInit() override {
        ++imgui_init;
        auto &io = ImGui::GetIO();
        // Engine owns the only context; platform input is explicitly opt-in.
        io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(width, height);
        if (!ImGui_ImplMetal_Init(device_)) throw std::runtime_error("ImGui Metal init failed");
        imgui_ready_ = true;
        if(platformInput) {
            if(!core_metal::initImGuiPlatform(window_)) throw std::runtime_error("SDL ImGui platform init failed");
            platform_ready_=true;
            platform_context_=ImGui::GetCurrentContext();
            printf("[core-metal] platform_init context=%p window=%u event_only=1 global_mouse_poll=0 cursor_change=0 os_ime=0 clipboard=disabled screensaver_enabled=%u\n",platform_context_,windowID(),SDL_ScreenSaverEnabled());
        }
    }
    void clbkImGuiShutdown() override {
        ++imgui_shutdown;
        if(platform_ready_) {
            check(ImGui::GetCurrentContext()==platform_context_ && window_ && SDL_GetWindowFromID(windowID())==window_,"platform shutdown owner/window invalid");
            ImGui_ImplSDL3_Shutdown();
            check(!ImGui::GetIO().BackendPlatformUserData,"platform shutdown failed");
            printf("[core-metal] platform_shutdown context_alive=1 window_alive=1 backend_cleared=1\n");
        }
        platform_ready_=false;
        if (imgui_ready_) ImGui_ImplMetal_Shutdown();
        imgui_ready_ = false;
    }
    void clbkImGuiNewFrame() override {
        ++newframes;
        check(updates == renders + 1 && newframes == updates, "update/newframe order");
        auto &io = ImGui::GetIO();
        if(platform_ready_) {
            check(ImGui::GetCurrentContext()==platform_context_,"platform context owner changed");
            ImGui_ImplSDL3_NewFrame();
        }
        // This renderer intentionally fixes its Metal target/drawable to 640x480,
        // even on Retina displays. SDL's OS backing scale is not our target scale.
        io.DisplaySize = ImVec2(width, height);
        if(platform_ready_) io.DisplayFramebufferScale=ImVec2(1,1);
        else io.DeltaTime = 1.0f / 60.0f; // preserve legacy diagnostic timing
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
    SURFHANDLE clbkLoadTexture(const char *fname, DWORD flags=0) override {
        if(textures_) {
            auto handle=texture_store_ ? texture_store_->load(fname,flags) : nullptr;
            auto entry=texture_store_ ? texture_store_->lookup(handle) : nullptr;
            printf("[core-metal] texture_load name=%s flags=%u handle=%p path=%s width=%u height=%u\n",fname ? fname : "(null)",flags,handle,entry ? entry->path.c_str() : "unsupported",entry ? entry->width : 0,entry ? entry->height : 0);
            return handle;
        }
        if(materials_) {
            ++unsupported_texture_loads_;
            printf("[core-metal] unsupported_texture_request=%s flags=%u returned=null\n",fname,flags);
        }
        return nullptr; // No fake handles or white-texture stand-ins.
    }
    void clbkReleaseTexture(SURFHANDLE handle) override {
        if(texture_store_) texture_store_->release(handle);
    }
    bool verified(unsigned n) const {
        return updates == n && renders == n && newframes == n && drawdata == n &&
               displays == n && completed == n && errors == 0 && readbacks == n &&
               scheduled == (present_ ? n : 0) && acquired == scheduled && hidden_checks == (visiblePreview ? 0 : n) && creates == 1 && closes == 1 && destroys == 1 &&
               imgui_init == 1 && imgui_shutdown == 1 &&
               (snapshot_.empty() || (uploaded_meshes > 0 && uploaded_triangles > 0 &&
                 (terrainVessel ? terrain_draw_calls==(omitTerrain?0:n) && mesh_draw_calls==(omitVessel?0:n) : (terrainMode ? terrain_draw_calls==n && mesh_draw_calls==0 : mesh_draw_calls == n && drawn_triangles == uploaded_triangles*n)) &&
                 geometry_readbacks == n && visual_register == 1 && visual_unregister == 1)) &&
               (!engine_camera_ || camera_frames_ == n);
    }
    void report() const {
        printf("[core-metal] drawable_acquired=%u present_scheduled=%u actual_presented=%u hidden_checks=%u\n",acquired,scheduled,presented,hidden_checks);
        if (present_ && presented != scheduled) printf("[core-metal] hidden-window limitation: positive presentedTime observed for %u/%u submissions within 100ms/frame; scanout NOT established for remaining frames\n",presented,scheduled);
        if(engine_camera_) printf("[core-metal] camera_frames=%u\n",camera_frames_);
        if(materials_ && !textures_) printf("[core-metal] texture_load_callbacks=%u texture_successes=0 textures_supported=0\n",unsupported_texture_loads_);
        printf("[core-metal] update=%u newframe=%u render=%u drawdata=%u display=%u completed=%u presented=%u readback=%u errors=%u\n",
               updates,newframes,renders,drawdata,displays,completed,presented,readbacks,errors);
        printf("[core-metal] create=%u close=%u destroy=%u imgui_init=%u imgui_shutdown=%u\n",
               creates,closes,destroys,imgui_init,imgui_shutdown);
        if(terrainVessel) printf("[core-metal] combined_draws terrain=%u vessel=%u same_encoder=1 same_depth_attachment=1\n",terrain_draw_calls,mesh_draw_calls);
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
        if(!visiblePreview) { check((SDL_GetWindowFlags(window_) & SDL_WINDOW_HIDDEN) &&
              !(SDL_GetWindowFlags(window_) & SDL_WINDOW_INPUT_FOCUS), "probe window visible/focused");
        ++hidden_checks; }
        if(textures_) for(const auto &range:texture_ranges_)
            check(!range.handle || texture_store_->lookup(range.handle),"stale/foreign texture draw handle");
        if(gear_) gear_->update(renders,gearSplit,gearFreeze); // after actual engine step, before draw
        if(engine_camera_) updateEngineCamera();
        if(terrain_) terrain_->update(renders,controlled_view_);
        if(planet_) planet_->update(renders,controlled_view_);
        if(gear_) controlled_view_=renders>gearSplit ? 2 : 1; // snapshot state label, NOT camera mutation
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
        if(terrain_ && !omitTerrain && !terrainLast) { terrain_->draw(encoder_,mesh_depth_state_); ++terrain_draw_calls; }
        if(planet_) planet_->draw(encoder_,mesh_depth_state_);
        if (mesh_pipeline_ && (!terrainMode || terrainVessel) && !omitVessel) {
            [encoder_ setRenderPipelineState:mesh_pipeline_];
            [encoder_ setDepthStencilState:disableSceneDepth ? nil : mesh_depth_state_];
            [encoder_ setCullMode:MTLCullModeNone];
            [encoder_ setVertexBuffer:mesh_vertices_ offset:0 atIndex:0];
            if(gear_) {
                [encoder_ setVertexBuffer:gear_normals_ offset:0 atIndex:4];
                float offset[4]={float(gear_->offset.x),float(gear_->offset.y),float(gear_->offset.z),0};
                [encoder_ setVertexBytes:offset length:sizeof(offset) atIndex:6];
            }
            if(textures_) [encoder_ setVertexBuffer:texture_uvs_ offset:0 atIndex:3];
            if(materials_) [encoder_ setVertexBuffer:material_colors_ offset:0 atIndex:2];
            if (!freeze_clip_ || renders==1) std::copy(clip_,clip_+16,gpu_clip_);
            [encoder_ setVertexBytes:gpu_clip_ length:sizeof(gpu_clip_) atIndex:1];
            if(engine_camera_) {
                printf("[core-metal] gpu_clip_json=[");
                for(int i=0;i<16;++i) printf("%s%.17g",i ? "," : "",double(gpu_clip_[i]));
                printf("]\n");
            }
            if(textures_) {
                size_t draw=0;
                for(const auto &range:texture_ranges_) {
                    if(gear_) {const auto &m=gear_->bound[gear_->draws.at(draw++).group]; [encoder_ setVertexBytes:m.data() length:sizeof(m) atIndex:5];}
                    const auto *entry=texture_store_->lookup(range.handle);
                    unsigned enabled=entry && !ignore_sampling_;
                    [encoder_ setFragmentTexture:entry ? entry->payload : nil atIndex:0];
                    [encoder_ setFragmentBytes:&enabled length:sizeof(enabled) atIndex:0];
                    [encoder_ drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:range.count
                        indexType:MTLIndexTypeUInt32 indexBuffer:mesh_indices_ indexBufferOffset:range.first*sizeof(uint32_t)];
                    ++texture_group_draws_;
                }
            } else if(gear_) {
                for(const auto &d:gear_->draws) {const auto &m=gear_->bound[d.group];
                    [encoder_ setVertexBytes:m.data() length:sizeof(m) atIndex:5];
                    [encoder_ drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:d.count indexType:MTLIndexTypeUInt32 indexBuffer:mesh_indices_ indexBufferOffset:d.first*sizeof(uint32_t)];
                }
            } else [encoder_ drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:uploaded_triangles*3
                indexType:MTLIndexTypeUInt32 indexBuffer:mesh_indices_ indexBufferOffset:0];
            ++mesh_draw_calls; drawn_triangles+=uploaded_triangles;
        }
        if(terrain_ && !omitTerrain && terrainLast) { terrain_->draw(encoder_,mesh_depth_state_); ++terrain_draw_calls; }
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
        if(gear_) {
            double expected=renders<=gearSplit ? 0 : 1;
            check(oapiGetFocusInterface()->GetAnimation(gear_->id)==expected,"gear state changed during draw");
            printf("[core-metal] gear_after_gpu frame=%u state=%.17g unchanged=1\n",renders,expected);
        }
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
            // Close combined view legitimately covers the entire target with ground.
            if(!terrainVessel) check(bgra[0] == 128 && bgra[1] == 64 && bgra[2] == 32 && bgra[3] == 255, "GPU clear pixel");
            ++readbacks;
            if (mesh_pipeline_) {
                std::vector<unsigned char> pixels(width*height*4);
                id<MTLTexture> source = engine_camera_ ? mesh_oracle_ : target_;
                [source getBytes:pixels.data() bytesPerRow:width*4 fromRegion:MTLRegionMake2D(0,0,width,height) mipmapLevel:0];
                unsigned background=0,geometry=0,chromatic=0,invalid=0;
                for (size_t i=0;i<pixels.size();i+=4) {
                    const auto *p=&pixels[i];
                    if(p[0]==128&&p[1]==64&&p[2]==32&&p[3]==255) { ++background; continue; }
                    if(textures_ || planetMode || terrainMode) { if(p[3]==255) { ++geometry; if(p[0]!=p[1]||p[1]!=p[2]) ++chromatic; } else ++invalid; }
                    else if(materials_) {
                        bool allowed=std::any_of(material_palette_.begin(),material_palette_.end(),[p](const auto &color){ return core_metal::diffusePixel(p,color.data()); });
                        if(allowed) { ++geometry; if(p[0]!=p[1] || p[1]!=p[2]) ++chromatic; }
                        else ++invalid;
                    } else if(p[0]==p[1]&&p[1]==p[2]&&p[0]>=80&&p[3]==255) ++geometry;
                }
                if(materials_ && !textures_) {
                    check(invalid==0,"GPU non-material palette (mesh-only oracle)");
                    printf("[core-metal] material_pixels frame=%u chromatic=%u invalid=%u geometry=%u\n",completed,chromatic,invalid,geometry);
                }
                check((terrainVessel || background>1000) && geometry>1000, "GPU mesh/background pixel counts");
                if(textures_) {
                    check(invalid==0,"nonopaque texture output");
                    printf("[core-metal] texture_pixels frame=%u chromatic=%u geometry=%u ignore_sampling=%u\n",completed,chromatic,geometry,ignore_sampling_);
                }
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
        const double nearPlane=terrainVessel ? 0.5 : (terrainMode ? 10.0 : (planetMode ? 1000.0 : 0.1)), farPlane=terrainVessel ? 5000.0 : (terrainMode ? 200000.0 : (planetMode ? 50000000.0 : 1000000.0));
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
        if(terrainVessel && renders==1) {
            VESSELSTATUS2 state{}; state.version=2; oapiGetFocusInterface()->GetStatusEx(&state);
            printf("[core-metal] combined_actual_state status=%d flight_flags=%u longitude=%.17g latitude=%.17g heading=%.17g landed_cog_height=%.17g static_template_only=1\n",state.status,oapiGetFocusInterface()->GetFlightStatus(),state.surf_lng,state.surf_lat,state.surf_hdg,state.vrot.x);
        }
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
        terrain_.reset();
        planet_.reset();
        if(texture_store_) {
            printf("[core-metal] texture_store requests=%u successes=%u uploads=%u cache_hits=%u releases=%u invalid_releases=%u resident=%zu group_draws=%u\n",texture_store_->requests,texture_store_->successes,texture_store_->uploads,texture_store_->hits,texture_store_->releases,texture_store_->invalidReleases,texture_store_->resident,texture_group_draws_);
            texture_ranges_.clear(); texture_uvs_=nil; texture_store_.reset();
            printf("[core-metal] texture_store_destroyed=1\n");
        }
        gear_normals_=nil; gear_.reset();
        material_colors_=nil; material_palette_.clear();
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
    std::unique_ptr<core_metal::TerrainPatch> terrain_;
    std::unique_ptr<core_metal::PlanetSurface> planet_;
    std::unique_ptr<core_metal::GearVisual> gear_;
    id<MTLBuffer> gear_normals_=nil;
    bool present_, engine_camera_=false, freeze_clip_=false, materials_=false, sdl_ = false, imgui_ready_ = false;
    ImGuiContext *platform_context_=nullptr;
    bool platform_ready_=false;
    bool textures_=false,ignore_sampling_=false;
    struct TextureRange { size_t first,count; void *handle; };
    std::vector<TextureRange> texture_ranges_;
    std::unique_ptr<core_metal::MetalTextureStore> texture_store_;
    id<MTLBuffer> texture_uvs_=nil;
    unsigned texture_group_draws_=0;
    id<MTLBuffer> material_colors_=nil;
    std::vector<std::array<float,4>> material_palette_;
    float gpu_clip_[16]={};
    unsigned camera_frames_=0, controlled_view_=0, unsupported_texture_loads_=0;
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
    unsigned terrain_draw_calls=0;
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
    core_metal::PreviewStartup preview;
    try { preview.prepare(argc,argv); }
    catch(const std::exception &e) { fprintf(stderr,"[core-preview] startup failed: %s\n",e.what()); return 2; }
    // Independent trailing session/presentation options leave old positional modes intact.
    bool session=false, sessionTest=false, ignoreInput=false, present=false, bufferedTest=false, ignoreBuffer=false;
    bool platformInput=false,imguiTest=false,ignoreForward=false;
    bool laptopTest=false,laptopIgnore=false;
    bool animationContract=false,gearTest=false,gearFreeze=false,gearCleanupFault=false;
    bool terrainCoast=false,terrainFlat=false,terrainFreeze=false,terrainSwap=false;
    bool omitVessel=false,omitTerrain=false,disableSceneDepth=false,terrainLast=false;
    bool planetFreeze=false,planetIgnore=false,planetSwap=false;
    while(argc>3) {
        std::string option=argv[argc-1];
        if(option=="--diagnostic-omit-vessel") { if(omitVessel) return 2; omitVessel=true; --argc; }
        else if(option=="--diagnostic-omit-terrain") { if(omitTerrain) return 2; omitTerrain=true; --argc; }
        else if(option=="--diagnostic-disable-scene-depth") { if(disableSceneDepth) return 2; disableSceneDepth=true; --argc; }
        else if(option=="--diagnostic-terrain-last") { if(terrainLast) return 2; terrainLast=true; --argc; }
        else if(option=="--hidden-present") { if(present) return 2; present=true; --argc; }
        else if(option=="--terrain-coast") { if(terrainCoast) return 2; terrainCoast=true; --argc; }
        else if(option=="--diagnostic-flat-terrain") { if(terrainFlat) return 2; terrainFlat=true; --argc; }
        else if(option=="--diagnostic-freeze-terrain-transform") { if(terrainFreeze) return 2; terrainFreeze=true; --argc; }
        else if(option=="--diagnostic-swap-terrain-uv") { if(terrainSwap) return 2; terrainSwap=true; --argc; }
        else if(option=="--diagnostic-freeze-planet-transform") { if(planetFreeze) return 2; planetFreeze=true; --argc; }
        else if(option=="--diagnostic-ignore-planet-sampling") { if(planetIgnore) return 2; planetIgnore=true; --argc; }
        else if(option=="--diagnostic-swap-planet-hemispheres") { if(planetSwap) return 2; planetSwap=true; --argc; }
        else if(option=="--gear-cleanup-fault-test") { if(gearCleanupFault) return 2; gearCleanupFault=true; --argc; }
        else if(option=="--gear-animation-test" || option=="--gear-animation-frozen-test") { if(gearTest) return 2; gearTest=true; gearFreeze=option=="--gear-animation-frozen-test"; --argc; }
        else if(option=="--animation-contract-test") { if(animationContract) return 2; animationContract=true; --argc; }
        else if(option=="--platform-input" || option=="--imgui-input-test" || option=="--imgui-input-ignore-test") {
            if(session) return 2;
            session=true; platformInput=true; imguiTest=option!="--platform-input"; ignoreForward=option=="--imgui-input-ignore-test"; --argc;
        }
        else if(option=="--buffered-input-test" || option=="--buffered-input-ignore-test") {
            if(session) return 2;
            session=true; bufferedTest=true; ignoreBuffer=option=="--buffered-input-ignore-test"; --argc;
        }
        else if(option=="--session-input" || option=="--session-input-test" || option=="--session-input-ignore-test") {
            if(session) return 2;
            session=true; sessionTest=option!="--session-input"; ignoreInput=option=="--session-input-ignore-test"; --argc;
        } else if(option=="--session-laptop-test" || option=="--session-laptop-ignore-test") {
            if(session) return 2;
            session=true; laptopTest=true; ignoreInput=laptopIgnore=option=="--session-laptop-ignore-test"; --argc;
        } else break;
    }
    if (argc < 3 || argc > 6) {
        fprintf(stderr,"Combined depth: <scenario> <frames> --terrain-vessel-test snapshot.ppm [--diagnostic-omit-vessel|--diagnostic-omit-terrain|--diagnostic-disable-scene-depth|--diagnostic-terrain-last]; actual external camera, KSC patch, static template, no contact-physics claim.\n");
        fprintf(stderr,"Bounded terrain: <scenario> <frames> --terrain-patch|--terrain-patch-test snapshot.ppm [--terrain-coast] [--diagnostic-flat-terrain|--diagnostic-freeze-terrain-transform|--diagnostic-swap-terrain-uv]; fixed engine ground camera, unlit real curved patch, no streaming/contact/flight coverage.\n");
        fprintf(stderr,"Earth orbit-only modes: <scenario> <frames> --planet-surface|--planet-surface-test snapshot.ppm [--diagnostic-freeze-planet-transform|--diagnostic-ignore-planet-sampling|--diagnostic-swap-planet-hemispheres]; hidden/offscreen, unlit h=0 sphere, not elevation terrain.\n");
        fprintf(stderr,"Usage (from content root): orbiter-core-metal <scenario> <frames 1..10000> [--mesh snapshot.ppm | --engine-camera snapshot.ppm | --engine-camera-test snapshot.ppm | --diagnostic-frozen-camera-test snapshot.ppm | --materials snapshot.ppm | --materials-test snapshot.ppm | --materials-reference-test snapshot.ppm | --textures snapshot.ppm | --textures-test snapshot.ppm | --textures-ignore-sampling-test snapshot.ppm] [--hidden-present] [--animation-contract-test | --gear-animation-test | --gear-animation-frozen-test] [--platform-input | --imgui-input-test | --imgui-input-ignore-test | --buffered-input-test | --buffered-input-ignore-test | --session-input | --session-input-test | --session-input-ignore-test]\nSession test modes require a bound of at least 16 frames; all windows remain hidden.\n");
        return 2;
    }
    char *end = nullptr;
    long frames = strtol(argv[2], &end, 10);
    int modeArgc=argc;
    bool terrainVessel=modeArgc==5 && std::string(argv[3])=="--terrain-vessel-test";
    if((omitVessel||omitTerrain||disableSceneDepth||terrainLast) && !terrainVessel) return 2;
    if(omitVessel && omitTerrain) return 2;
    if(terrainVessel) terrainCoast=true;
    if(preview.active) { session=true; platformInput=true; present=true; }
    bool terrain=terrainVessel || (modeArgc==5 && (std::string(argv[3])=="--terrain-patch" || std::string(argv[3])=="--terrain-patch-test"));
    if((terrainCoast||terrainFlat||terrainFreeze||terrainSwap) && !terrain) return 2;
    if(terrain && (session||gearTest||animationContract||present) && !preview.active) return 2;
    bool planet=modeArgc==5 && (std::string(argv[3])=="--planet-surface" || std::string(argv[3])=="--planet-surface-test");
    if((planetFreeze||planetIgnore||planetSwap) && !planet) return 2;
    if(planet && (session||gearTest||animationContract||present)) return 2;
    bool textures=terrainVessel || (modeArgc==5 && (std::string(argv[3])=="--textures" || std::string(argv[3])=="--textures-test" || std::string(argv[3])=="--textures-ignore-sampling-test"));
    if (!end || *end || frames < 1 || frames > 10000 ||
        (modeArgc != 3 && modeArgc != 5) ||
        (modeArgc == 5 && ((!terrain && !planet && !textures && std::string(argv[3]) != "--mesh" && std::string(argv[3]) != "--engine-camera" && std::string(argv[3]) != "--engine-camera-test" && std::string(argv[3]) != "--diagnostic-frozen-camera-test" && std::string(argv[3]) != "--materials" && std::string(argv[3]) != "--materials-test" && std::string(argv[3]) != "--materials-reference-test") || !argv[4][0]))) return 2;
    bool engineCamera=modeArgc==5 && std::string(argv[3])!="--mesh";
    bool freezeClip=modeArgc==5 && std::string(argv[3])=="--diagnostic-frozen-camera-test";
    bool materials=textures || (modeArgc==5 && (std::string(argv[3])=="--materials" || std::string(argv[3])=="--materials-test"));
    bool materialViews=(textures && std::string(argv[3])!="--textures") || (modeArgc==5 && (std::string(argv[3])=="--materials-test" || std::string(argv[3])=="--materials-reference-test"));
    bool cameraTest=modeArgc==5 && (std::string(argv[3])=="--terrain-patch-test" || std::string(argv[3])=="--planet-surface-test" || std::string(argv[3])=="--engine-camera-test" || materialViews || freezeClip);
    if(preview.active) cameraTest=false; // initial tracked DG view only; subsequent pose is engine-owned
    if(cameraTest && frames<2) return 2;
    if(sessionTest && frames<16) return 2;
    if(laptopTest && frames<16) return 2;
    if(bufferedTest && frames<19) return 2;
    if(imguiTest && frames<22) return 2;
    if(animationContract && (modeArgc!=5 || session)) return 2;
    if(gearTest && (modeArgc!=5 || session || animationContract || !textures || freezeClip || cameraTest || frames<2 || present || std::string(argv[3])!="--textures")) return 2;
    if(gearCleanupFault && !gearTest) return 2;
    setvbuf(stdout, nullptr, _IONBF, 0);
    @autoreleasepool {
        g_pOrbiter = new Orbiter;
        CoreMetalClient *client = nullptr;
        bool attached = false, launched = false;
        std::unique_ptr<core_metal::SessionInput> input;
        std::unique_ptr<core_metal::SessionControlProbe> controlProbe;
        std::unique_ptr<core_metal::BufferedControlProbe> bufferedProbe;
        std::unique_ptr<core_metal::ImGuiInputProbe> imguiProbe;
        std::unique_ptr<core_metal::LaptopControlProbe> laptopProbe;
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
            if(terrain) { g_pOrbiter->Cfg()->CfgVisualPrm.ElevMode=2; g_pOrbiter->Cfg()->CfgPRenderPrm.TileLoadFlags=2; }
            client = new CoreMetalClient(present, modeArgc == 5 ? argv[4] : "", engineCamera, freezeClip, materials, textures, textures && std::string(argv[3])=="--textures-ignore-sampling-test");
            client->visiblePreview=preview.active && !preview.hidden;
            client->platformInput=platformInput;
            client->terrainMode=terrain; client->terrainCoast=terrainCoast; client->terrainFlat=terrainFlat; client->terrainFreeze=terrainFreeze; client->terrainSwap=terrainSwap;
            client->terrainVessel=terrainVessel; client->omitVessel=omitVessel; client->omitTerrain=omitTerrain; client->disableSceneDepth=disableSceneDepth; client->terrainLast=terrainLast;
            if(terrainVessel) printf("[core-metal] combined_scene near=0.5 far=5000 shared_depth_attachment=1 omit_vessel=%u omit_terrain=%u disable_vessel_depth=%u terrain_last=%u static_template=1 contact_physics_claim=0\n",omitVessel,omitTerrain,disableSceneDepth,terrainLast);
            client->planetMode=planet; client->planetFreeze=planetFreeze; client->planetIgnore=planetIgnore; client->planetSwap=planetSwap;
            client->animationContract=animationContract;
            client->gearTest=gearTest; client->gearFreeze=gearFreeze; client->gearSplit=unsigned(frames/2);
            client->gearCleanupFault=gearCleanupFault;
            if(gearCleanupFault) printf("[core-metal] DIAGNOSTIC: client-only detach validation fault; shared engine geometry unchanged\n");
            if(freezeClip) printf("[core-metal] DIAGNOSTIC NEGATIVE CONTROL: GPU clip frozen at frame 1; API camera still changes\n");
            attached = oapiRegisterGraphicsClient(client);
            if (!attached || !client->clbkInitialise()) throw std::runtime_error("graphics registration/init failed");
            launched = true;
            g_pOrbiter->Launch(argv[1]);
            if (!oapiGetFocusInterface()) throw std::runtime_error("no focus vessel");
            printf("[core-metal] focus=%s scenario=%s frames=%ld\n", oapiGetFocusInterface()->GetName(), argv[1], frames);
            client->attachFocusMesh();
            if(preview.active) { g_pOrbiter->Pause(true); printf("[core-preview] engine Pause(true); Ctrl+P toggles real engine pause; static DG mesh, no gear animation\n"); }
            if(animationContract) core_metal::probeAnimationContract(*client);
            if(gearTest) client->setTestCamera(1);
            if(session) {
                if(!g_pOrbiter->GetDInput()->GetKbdDevice()) throw std::runtime_error("native session has no actual keyboard device");
                auto publisher=ignoreInput ? +[](unsigned,bool){} : (ignoreBuffer ? shim_set_key_state_only : shim_set_key);
                if(preview.active) publisher=+[](unsigned key,bool down) {
                    // Deliberately bounded preview contract: flight keypad, the
                    // laptop flight keys named by preview_keymap.h, modifiers,
                    // and Ctrl+P only. No save/dialog/gear shortcuts or text entry.
                    bool allowed=(key>=0x47 && key<=0x53) || key==0x37 || key==0xb5 ||
                       key==0x19 || key==0x1d || key==0x9d || key==0x2a || key==0x36;
                    for(auto code:core_metal::previewLaptopPhysicalKeys())
                        allowed = allowed || key==core_metal::directInputScancode(code);
                    if(allowed) shim_set_key(key,down);
                }; else if(laptopTest && !laptopIgnore) publisher=+[](unsigned key,bool down) {
                    // Laptop regression mirrors the preview publisher allowlist so
                    // the hidden test proves exactly the shipped physical contract;
                    // the ignore variant publishes nothing at all.
                    bool allowed=(key>=0x47 && key<=0x53) || key==0x37 || key==0xb5 ||
                       key==0x19 || key==0x1d || key==0x9d || key==0x2a || key==0x36;
                    for(auto code:core_metal::previewLaptopPhysicalKeys())
                        allowed = allowed || key==core_metal::directInputScancode(code);
                    if(allowed) shim_set_key(key,down);
                }; // ignore variant keeps the base no-op publisher
                input=std::make_unique<core_metal::SessionInput>(client->windowID(),publisher,platformInput && !ignoreForward ? core_metal::processImGuiEvent : nullptr);
                input->pump(); // drain startup window notifications before the test schedule
                if(imguiTest) imguiProbe=std::make_unique<core_metal::ImGuiInputProbe>(client->windowID());
                if(bufferedTest) bufferedProbe=std::make_unique<core_metal::BufferedControlProbe>(client->windowID());
                if(sessionTest) controlProbe=std::make_unique<core_metal::SessionControlProbe>(client->windowID());
                if(laptopTest) laptopProbe=std::make_unique<core_metal::LaptopControlProbe>(client->windowID());
                printf("[core-metal] session input=held-DIK hidden=%u buffered_commands=bounded-transitions bridge_publication=%u buffered_publication=%u\n",!client->visiblePreview,!ignoreInput,!ignoreBuffer);
            }
            unsigned executed=0;
            for (long i=0; preview.active || i<frames; ++i) { @autoreleasepool {
                if(preview.active && preview.hidden && i==frames) {
                    SDL_Event event{}; event.type=SDL_EVENT_QUIT;
                    if(!SDL_PushEvent(&event)) throw std::runtime_error("self-test quit enqueue failed");
                    printf("[core-preview] self-test process-local SDL quit; no key injection\n");
                }
                if(controlProbe) controlProbe->before(static_cast<unsigned>(i));
                if(bufferedProbe) bufferedProbe->before(static_cast<unsigned>(i));
                if(imguiProbe) imguiProbe->before(static_cast<unsigned>(i));
                if(laptopProbe) laptopProbe->before(static_cast<unsigned>(i));
                if(input) input->pump();
                if(preview.active && input->quitRequested()) break;
                if(cameraTest && (i==0 || i==frames/2)) client->setTestCamera(i==0 ? 1 : 2,materialViews);
                if(imguiProbe) imguiProbe->beforeEngine();
                g_pOrbiter->SingleFrame();
                if(preview.active) {
                    if(preview.hidden && !oapiGetPause()) throw std::runtime_error("preview did not remain actually paused");
                    if(!preview.hidden) client->updatePreviewTitle();
                }
                ++executed;
                if(controlProbe) controlProbe->after(static_cast<unsigned>(i),g_pOrbiter->KeyState());
                if(bufferedProbe) bufferedProbe->after(static_cast<unsigned>(i),g_pOrbiter->KeyState());
                if(imguiProbe) imguiProbe->after(static_cast<unsigned>(i),g_pOrbiter->KeyState());
                if(laptopProbe) laptopProbe->after(static_cast<unsigned>(i),g_pOrbiter->KeyState());
                if(input && input->quitRequested()) break;
                if(preview.active) SDL_Delay(16);
            } }
            bool controlsOK=(!controlProbe || controlProbe->verified()) && (!bufferedProbe || bufferedProbe->verified()) && (!imguiProbe || imguiProbe->verified()) && (!laptopProbe || laptopProbe->verified());
            if(preview.active && preview.hidden) {
                controlsOK &= executed==unsigned(frames) && input->quitRequested();
                printf("[core-preview] bounded frames=%u real_pause=%u SDL_quit=%u\n",executed,oapiGetPause(),input->quitRequested());
            }
            imguiProbe.reset();
            input.reset(); // all held keys released before core/device/world teardown
            if(platformInput) {
                unsigned char released[256]{};
                if(FAILED(g_pOrbiter->GetDInput()->GetKbdDevice()->GetDeviceState(sizeof(released),released))) throw std::runtime_error("platform teardown device poll failed");
                for(auto key:released) if(key) throw std::runtime_error("platform teardown left a held key");
                printf("[core-metal] platform_session_destroyed actual_device_keys=all-released\n");
            }
            bool cleanupOK=client->detachFocusMesh();
            cleanupOK &= core_metal::cleanupStep("world close",[&]{g_pOrbiter->CloseSession(); printf("[core-metal] world_closed=1\n");});
            launched = false;
            result = client->verified(executed) && controlsOK && cleanupOK ? 0 : 1;
        } catch (const std::exception &e) {
            fprintf(stderr,"[core-metal] FAIL: %s\n", e.what());
        }
        imguiProbe.reset();
        input.reset();
        if (launched) {
            if(client) client->detachFocusMesh(); // result is already FAIL on this path
            core_metal::cleanupStep("world close",[&]{g_pOrbiter->CloseSession(); printf("[core-metal] world_closed=1\n");});
        }
        if (client) client->report();
        if (attached && !oapiUnregisterGraphicsClient(client)) result = 1;
        delete client;
        delete g_pOrbiter;
        g_pOrbiter = nullptr;
        printf("[core-metal] teardown complete; %s\n", result == 0 ? "PASS" : "FAIL");
        return result;
    }
}
