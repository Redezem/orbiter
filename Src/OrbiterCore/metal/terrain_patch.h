#pragma once
#include "terrain_geometry.h"
#include "ZTreeMgr.h"
#include "dds/dds_decoder.h"
#include <filesystem>
namespace core_metal {
class TerrainPatch {
    OBJHANDLE body_=nullptr;
    double radius_=0;
    VECTOR3 anchor_{};
    TerrainRegion region_{};
    id<MTLBuffer> vertices_=nil,indices_=nil;
    id<MTLTexture> texture_=nil;
    id<MTLRenderPipelineState> pipeline_=nil;
    float bound_[16]={};
    struct Cache {
        // Exact SDK ownership; destroyed on success and exceptions before world close.
        std::vector<ElevationTile> *p=InitTileCache(2);
        ~Cache(){ if(p) ReleaseTileCache(p); printf("[core-metal] terrain_cache_released_before_world_close=1\n"); }
    };
    static void require(bool b,const char *s){if(!b)throw std::runtime_error(s);}
public:
    bool coast=false,flat=false,freeze=false,swapUV=false,combined=false;
    OBJHANDLE body()const{return body_;}
    void camera(unsigned view) {
        // Real SDK ground observer, mean-radius altitude. Camera.cpp:279-295,
        // 1078-1083 uses body rotation and ground-frame rotation; no client pose.
        CameraMode_Ground mode;
        char spec[256];
        auto center=terrainCoordinate(region_,16,16);
        snprintf(spec,sizeof(spec),"Earth %.17g %.17g %.17gM 0 -90",center[0]*DEG,center[1]*DEG,coast?3500.:30000.);
        mode.Init(spec); mode.SetTarget(body_); mode.SetOrientation(view==2?.55:0.,-PI/2); mode.SetFOV(40);
        require(oapiSetCameraMode(mode),"terrain actual ground camera rejected");
        printf("[core-metal] terrain_test_controlled_camera view=%u mean_radius_altitude=1 NOT_scenario_camera=1\n",view);
    }
    void attach(oapi::GraphicsClient &client,id<MTLDevice> device) {
        char name[]="Earth"; body_=oapiGetGbodyByName(name);
        require(body_&&oapiGetObjectType(body_)==OBJTP_PLANET,"terrain live Earth missing");
        radius_=oapiGetSize(body_); require(std::isfinite(radius_)&&radius_>0,"terrain radius");
        require(oapiElevationManager(body_)!=nullptr,"terrain real elevation manager missing");
        region_=terrainRegion(coast);
        auto center=terrainCoordinate(region_,16,16);
        anchor_=terrainPoint(radius_,0,center[0],center[1]);
        // Fixed body-local double anchor, independent of camera and terrain height.
        char path[256]={};client.PlanetTexturePath(name,path);
        std::string normalized=path;std::replace(normalized.begin(),normalized.end(),'\\','/');
        require(normalized.size()<220&&std::filesystem::is_regular_file(normalized+"/Archive/Surf.tree"),"terrain configured Surf archive absent");
        std::unique_ptr<ZTreeMgr> tree(ZTreeMgr::CreateFromFile(path,ZTreeMgr::LAYER_SURF));
        require(bool(tree),"terrain core archive reader");
        int a=region_.level,ar=region_.row,ac=region_.col;DWORD count=0,index=DWORD(-1);BYTE *raw=nullptr;
        // Metadata-only nodes are not data. Walk actual available ancestors.
        for(;a>=0;--a,ar/=2,ac/=2){
            index=tree->Idx(a+4,ar,ac);
            if(index==DWORD(-1)||index>=tree->TOC().size())continue;
            DWORD bytes=tree->NodeSizeInflated(index);
            if(bytes<128)continue;
            require(bytes<=4*1024*1024,"terrain Surf payload cap");
            count=tree->ReadData(a+4,ar,ac,&raw);
            if(raw&&count==bytes)break;
            if(raw)tree->ReleaseData(raw);raw=nullptr;
        }
        auto deleter=[&](BYTE *p){if(p)tree->ReleaseData(p);};
        std::unique_ptr<BYTE,decltype(deleter)> payload(raw,deleter);
        require(a>=0&&raw,"terrain no Surf ancestor payload");
        auto decoded=orbiter::dds::decode(raw,count);require(bool(decoded),"terrain DDS decode");
        auto &im=decoded.image;
        require(im.width<=2048&&im.height<=2048,"terrain texture bound");
        for(size_t i=3;i<im.rgba.size();i+=4)require(im.rgba[i]==255,"terrain opaque imagery required");
        auto td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:im.width height:im.height mipmapped:NO];
        td.storageMode=MTLStorageModeShared;td.usage=MTLTextureUsageShaderRead;
        texture_=[device newTextureWithDescriptor:td];require(texture_!=nil,"terrain texture allocation");
        [texture_ replaceRegion:MTLRegionMake2D(0,0,im.width,im.height) mipmapLevel:0 withBytes:im.rgba.data() bytesPerRow:im.width*4];
        printf("[core-metal] terrain_source path=%s patch=%d,%d,%d ancestor=%d,%d,%d index=%u bytes=%u width=%u height=%u unlit=1 opaque=1 nearest_mip0=1 elevation_mode=2 exaggeration=1\n",normalized.c_str(),region_.level+4,region_.row,region_.col,a+4,ar,ac,index,count,im.width,im.height);
        printf("[core-metal] terrain_geometry_json={\"radius\":%.17g,\"anchor\":[%.17g,%.17g,%.17g],\"patch\":[%d,%d,%d],\"ancestor\":[%d,%d,%d],\"vertices\":[",radius_,anchor_.x,anchor_.y,anchor_.z,region_.level+4,region_.row,region_.col,a+4,ar,ac);
        std::vector<PlanetVertex> vertices;
        std::vector<VECTOR3> points;
        std::vector<uint32_t> indices;
        Cache cache;
        require(cache.p!=nullptr,"terrain elevation cache allocation");
        double factor=double(1<<(region_.level-a));
        for(unsigned row=0;row<=32;++row)for(unsigned col=0;col<=32;++col){
            auto geo=terrainCoordinate(region_,row,col);int actual=0;
            double h=oapiSurfaceElevationEx(body_,geo[0],geo[1],21,cache.p,nullptr,&actual);
            require(actual>0&&std::isfinite(h),"terrain missing actual elevation level");
            auto p=terrainPoint(radius_,flat?0:h,geo[0],geo[1]);points.push_back(p);
            auto offset=p-anchor_; // DOUBLE subtraction before float conversion.
            double u=(region_.col+.001+.998*col/32.)/factor-ac;
            double v=(region_.row+.001+.998*row/32.)/factor-ar;
            vertices.push_back({float(offset.x),float(offset.y),float(offset.z),1,float(swapUV?1-u:u),float(v)});
            auto &b=vertices.back();
            printf("%s[%.17g,%.17g,%.17g,%d,%.9g,%.9g,%.9g,%.9g,%.9g]",vertices.size()>1?",":"",geo[0],geo[1],h,actual,b.x,b.y,b.z,b.u,b.v);
        }
        printf("]}\n");
        for(unsigned row=0;row<32;++row)for(unsigned col=0;col<32;++col){uint32_t q=row*33+col;for(auto i:{q,q+33,q+1,q+1,q+33,q+34})indices.push_back(i);}
        // Geometric normals derived from actual curved triangle edges, never API shading normals.
        double normalMin=1;
        for(size_t i=0;i<indices.size();i+=3){auto p=indices[i],q=indices[i+1],r=indices[i+2];auto n=crossp(points[r]-points[p],points[q]-points[p]);require(length(n)>0,"terrain degenerate geometry");normalMin=std::min(normalMin,dotp(n,points[p])/(length(n)*length(points[p])));}
        printf("[core-metal] terrain_geometry_normals triangles=2048 minimum_radial_dot=%.17g shader_uses_normals=0\n",normalMin);
        vertices_=[device newBufferWithBytes:vertices.data() length:vertices.size()*sizeof(PlanetVertex) options:MTLResourceStorageModeShared];
        indices_=[device newBufferWithBytes:indices.data() length:indices.size()*4 options:MTLResourceStorageModeShared];
        require(vertices_&&indices_,"terrain mesh upload");
        NSString *source=@"#include <metal_stdlib>\nusing namespace metal; struct V {float4 p; float2 uv;}; struct O {float4 p [[position]]; float2 uv;}; vertex O tv(uint i [[vertex_id]],const device V* v [[buffer(0)]],constant float4x4& m [[buffer(1)]]) {O o; o.p=m*v[i].p; o.uv=v[i].uv; return o;} fragment float4 tf(O o [[stage_in]],texture2d<float> t [[texture(0)]]) {constexpr sampler s(coord::normalized,address::clamp_to_edge,filter::nearest,mip_filter::none); return float4(t.sample(s,o.uv,level(0)).rgb,1);}";
        NSError *error=nil;auto lib=[device newLibraryWithSource:source options:nil error:&error];require(lib!=nil,"terrain shader");
        auto pd=[MTLRenderPipelineDescriptor new];pd.vertexFunction=[lib newFunctionWithName:@"tv"];pd.fragmentFunction=[lib newFunctionWithName:@"tf"];
        pd.colorAttachments[0].pixelFormat=MTLPixelFormatBGRA8Unorm;pd.depthAttachmentPixelFormat=MTLPixelFormatDepth32Float;
        pipeline_=[device newRenderPipelineStateWithDescriptor:pd error:&error];require(pipeline_!=nil,"terrain pipeline");
    }
    void update(unsigned frame,unsigned view){
        VECTOR3 camera,body;MATRIX3 cr,br;oapiCameraGlobalPos(&camera);oapiCameraRotationMatrix(&cr);oapiGetGlobalPos(body_,&body);oapiGetRotationMatrix(body_,&br);
        auto t=tmul(cr,(body-camera)+mul(br,anchor_));auto r=mul(transp(cr),br);
        double nearPlane=combined?.5:10.,farPlane=combined?5000.:200000.;
        double aperture=oapiCameraAperture(),sy=1/tan(aperture),sx=sy*480/640,a=farPlane/(farPlane-nearPlane),b=-nearPlane*a;
        float computed[16];
        for(int col=0;col<4;++col){double x=col<3?r.data[col]:t.x,y=col<3?r.data[3+col]:t.y,z=col<3?r.data[6+col]:t.z;double v[4]={sx*x,sy*y,a*z+(col==3?b:0),z};for(int row=0;row<4;++row){computed[col*4+row]=float(v[row]);require(std::isfinite(v[row])&&std::isfinite(computed[col*4+row]),"terrain nonfinite actual pose");}}
        if(!freeze||frame==1)std::copy(computed,computed+16,bound_);
        printf("[core-metal] terrain_json={\"frame\":%u,\"view\":%u,\"radius\":%.17g,\"aperture\":%.17g",frame,view,radius_,aperture);
        auto array=[](const char *k,const double *p,int n){printf(",\"%s\":[",k);for(int i=0;i<n;++i)printf("%s%.17g",i?",":"",p[i]);printf("]");};
        array("camera",camera.data,3);array("body",body.data,3);array("cameraR",cr.data,9);array("bodyR",br.data,9);array("anchor",anchor_.data,3);array("anchorCamera",t.data,3);
        double upload[16],calc[16];for(int i=0;i<16;++i){upload[i]=bound_[i];calc[i]=computed[i];}array("bound",upload,16);array("computed",calc,16);printf("}\n");
    }
    void draw(id<MTLRenderCommandEncoder> e,id<MTLDepthStencilState> depth){
        [e setRenderPipelineState:pipeline_];[e setDepthStencilState:depth];[e setCullMode:MTLCullModeNone];
        [e setVertexBuffer:vertices_ offset:0 atIndex:0];[e setVertexBytes:bound_ length:sizeof(bound_) atIndex:1];[e setFragmentTexture:texture_ atIndex:0];
        [e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:32*32*6 indexType:MTLIndexTypeUInt32 indexBuffer:indices_ indexBufferOffset:0];
    }
    ~TerrainPatch(){body_=nullptr;printf("[core-metal] terrain_resources_released=1 engine_handle_dropped=1 before_world_close=1\n");}
};
}
