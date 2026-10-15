#pragma once
#include "ZTreeMgr.h"
#include "dds/dds_decoder.h"
#include "planet_mesh.h"
#include <filesystem>
// Included by the ObjC++ client after SDK and Metal headers. Owns no engine resource.
namespace core_metal {
class PlanetSurface {
    OBJHANDLE body_=nullptr;
    double radius_=0;
    id<MTLBuffer> vertices_[2]={nil,nil},indices_=nil;
    id<MTLTexture> textures_[2]={nil,nil};
    id<MTLRenderPipelineState> pipeline_=nil;
    float bound_[16]={};
    static void require(bool b,const char *s) { if(!b) throw std::runtime_error(s); }
public:
    bool freeze=false,ignoreSampling=false,swap=false;
    OBJHANDLE body() const { return body_; }
    void attach(oapi::GraphicsClient &client,id<MTLDevice> device) {
        char earthName[]="Earth";
        body_=oapiGetGbodyByName(earthName);
        require(body_ && oapiGetObjectType(body_)==OBJTP_PLANET,"live Earth planet missing");
        auto tile=static_cast<const int*>(oapiGetObjectParam(body_,OBJPRM_PLANET_TILEENGINE));
        require(tile && *tile==2,"Earth requires type-2 tile engine");
        radius_=oapiGetSize(body_); require(std::isfinite(radius_)&&radius_>0,"invalid live Earth radius");
        char name[256]={},path[256]={}; oapiGetObjectName(body_,name,sizeof(name));
        client.PlanetTexturePath(name,path);
        std::string normalized=path; std::replace(normalized.begin(),normalized.end(),'\\','/');
        require(normalized.size()<220 && std::filesystem::is_regular_file(normalized+"/Archive/Surf.tree"),"configured Earth archive unavailable");
        std::unique_ptr<ZTreeMgr> tree(ZTreeMgr::CreateFromFile(path,ZTreeMgr::LAYER_SURF));
        require(bool(tree),"core Surf.tree reader failed");
        printf("[core-metal] planet_source name=%s path=%s radius=%.17g roots=level4 unlit=1 elevation=0 nearest_clamp=1 frustum=1000:50000000\n",name,normalized.c_str(),radius_);
        for(unsigned h=0;h<2;++h) {
            // Only the two valid root addresses are requested. Existing trusted-content
            // reader is not a hostile-archive parser. Bound payload before allocation.
            DWORD idx=tree->Idx(4,0,h);
            require(idx!=DWORD(-1) && idx<tree->TOC().size(),"surface root index missing");
            DWORD expected=tree->NodeSizeInflated(idx);
            require(expected>=128 && expected<=4*1024*1024,"surface root payload size");
            BYTE *raw=nullptr;
            DWORD count=tree->ReadData(4,0,h,&raw);
            auto deleter=[&](BYTE *p){if(p) tree->ReleaseData(p);};
            std::unique_ptr<BYTE,decltype(deleter)> payload(raw,deleter);
            require(raw && count==expected,"surface root inflate failed");
            auto decoded=orbiter::dds::decode(raw,count);
            require(bool(decoded)&&decoded.image.width==512&&decoded.image.height==512,"surface root DDS requires 512x512");
            const auto &im=decoded.image;
            for(size_t i=3;i<im.rgba.size();i+=4) require(im.rgba[i]==255,"nonopaque surface root");
            auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:512 height:512 mipmapped:NO];
            td.storageMode=MTLStorageModeShared; td.usage=MTLTextureUsageShaderRead;
            textures_[h]=[device newTextureWithDescriptor:td];
            require(textures_[h]!=nil,"planet texture allocation");
            [textures_[h] replaceRegion:MTLRegionMake2D(0,0,512,512) mipmapLevel:0 withBytes:im.rgba.data() bytesPerRow:512*4];
            auto mesh=planetMesh(radius_,h);
            vertices_[h]=[device newBufferWithBytes:mesh.vertices.data() length:mesh.vertices.size()*sizeof(PlanetVertex) options:MTLResourceStorageModeShared];
            if(h==0) indices_=[device newBufferWithBytes:mesh.indices.data() length:mesh.indices.size()*4 options:MTLResourceStorageModeShared];
            require(vertices_[h]&&indices_,"planet mesh upload");
            printf("[core-metal] planet_root hemisphere=%u index=%u bytes=%u width=512 height=512\n",h,idx,count);
        }
        NSString *source=@"#include <metal_stdlib>\nusing namespace metal; struct V {float4 p; float2 uv;}; struct O {float4 p [[position]]; float2 uv;}; vertex O pv(uint i [[vertex_id]],const device V* v [[buffer(0)]],constant float4x4& m [[buffer(1)]]) {O o; o.p=m*v[i].p; o.uv=v[i].uv; return o;} fragment float4 pf(O o [[stage_in]],texture2d<float> t [[texture(0)]],constant uint& enabled [[buffer(0)]]) {constexpr sampler s(coord::normalized,address::clamp_to_edge,filter::nearest,mip_filter::none); return enabled ? float4(t.sample(s,o.uv,level(0)).rgb,1) : float4(1,1,1,1);}";
        NSError *error=nil; auto library=[device newLibraryWithSource:source options:nil error:&error];
        require(library!=nil,"planet shader compilation");
        auto pd=[MTLRenderPipelineDescriptor new];
        pd.vertexFunction=[library newFunctionWithName:@"pv"]; pd.fragmentFunction=[library newFunctionWithName:@"pf"];
        pd.colorAttachments[0].pixelFormat=MTLPixelFormatBGRA8Unorm; pd.depthAttachmentPixelFormat=MTLPixelFormatDepth32Float;
        pipeline_=[device newRenderPipelineStateWithDescriptor:pd error:&error]; require(pipeline_!=nil,"planet pipeline creation");
    }
    void update(unsigned frame,unsigned view) {
        VECTOR3 camera,body; MATRIX3 cr,br;
        oapiCameraGlobalPos(&camera); oapiCameraRotationMatrix(&cr);
        oapiGetGlobalPos(body_,&body); oapiGetRotationMatrix(body_,&br);
        double aperture=oapiCameraAperture();
        require(std::isfinite(aperture)&&aperture>0&&aperture<1.57,"planet aperture invalid");
        auto t=tmul(cr,body-camera); auto r=mul(transp(cr),br);
        double sy=1/tan(aperture),sx=sy*480/640,a=50000000./(50000000.-1000.),b=-1000*a;
        float computed[16];
        for(int col=0;col<4;++col) {
            double x=col<3?r.data[col]:t.x,y=col<3?r.data[3+col]:t.y,z=col<3?r.data[6+col]:t.z;
            double v[4]={sx*x,sy*y,a*z+(col==3?b:0),z};
            for(int row=0;row<4;++row) {computed[col*4+row]=float(v[row]);require(std::isfinite(v[row])&&std::isfinite(computed[col*4+row]),"nonfinite Earth/camera pose");}
        }
        if(!freeze||frame==1) std::copy(computed,computed+16,bound_);
        printf("[core-metal] planet_json={\"frame\":%u,\"view\":%u,\"radius\":%.17g,\"aperture\":%.17g",frame,view,radius_,aperture);
        auto array=[](const char *key,const double *p,int n){printf(",\"%s\":[",key);for(int i=0;i<n;++i)printf("%s%.17g",i?",":"",p[i]);printf("]");};
        array("camera",camera.data,3); array("body",body.data,3); array("cameraR",cr.data,9); array("bodyR",br.data,9);
        double upload[16];for(int i=0;i<16;++i)upload[i]=bound_[i];array("bound",upload,16);printf("}\n");
    }
    void draw(id<MTLRenderCommandEncoder> e,id<MTLDepthStencilState> depth) {
        [e setRenderPipelineState:pipeline_]; [e setDepthStencilState:depth]; [e setCullMode:MTLCullModeNone];
        [e setVertexBytes:bound_ length:sizeof(bound_) atIndex:1];
        unsigned enabled=!ignoreSampling; [e setFragmentBytes:&enabled length:sizeof(enabled) atIndex:0];
        for(unsigned h=0;h<2;++h) {
            [e setVertexBuffer:vertices_[h] offset:0 atIndex:0]; [e setFragmentTexture:textures_[swap?1-h:h] atIndex:0];
            [e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:64*64*6 indexType:MTLIndexTypeUInt32 indexBuffer:indices_ indexBufferOffset:0];
        }
    }
    ~PlanetSurface() {body_=nullptr; printf("[core-metal] planet_resources_released=1 engine_handle_dropped=1 before_world_close=1\n");}
};
}
