#pragma once
#include "gear_math.h"
#include "best_effort_cleanup.h"
#include <cstring>
namespace core_metal {
// Deliberately one DG mesh-0 instance, selected gear only. No hierarchy/edit API.
class GearVisual {
    static void require(bool ok,const char *m) {if(!ok) throw std::runtime_error(m);}
    struct Group { std::vector<NTVERTEX> vertices; std::vector<WORD> indices; DWORD flags,material,texture; };
    MESHHANDLE mesh_=nullptr;
    unsigned slots_=0;
    VESSEL *owner_=nullptr; // identity only; never dereference without fresh focus match
    bool restore_pending_=false;
    std::vector<Group> groups_;
    std::vector<gear::Rotation> read(VESSEL *vessel) const {
        ANIMATION *fresh=nullptr;
        require(vessel->GetAnimPtr(&fresh)==slots_ && fresh && id<slots_,"gear animation array changed");
        const auto &a=fresh[id];
        require(a.defstate==1 && a.ncomp==14 && a.comp,"gear definition removed/redefined");
        std::vector<gear::Rotation> result;
        for(unsigned i=0;i<a.ncomp;++i) {
            auto c=a.comp[i];
            require(c && c->trans && !c->parent && !c->nchildren && c->trans->mesh==0 && c->trans->Type()==MGROUP_TRANSFORM::ROTATE,"unsupported gear topology/kind");
            auto r=static_cast<const MGROUP_ROTATE*>(c->trans);
            require(r->grp && r->ngrp>0 && r->ngrp<=4,"unsupported whole-mesh/local gear transform");
            result.push_back({c->state0,c->state1,{r->ref.x,r->ref.y,r->ref.z},{r->axis.x,r->axis.y,r->axis.z},r->angle,{r->grp,r->grp+r->ngrp}});
        }
        return result;
    }
public:
    struct Draw {unsigned group; size_t base,first,count;};
    std::vector<Draw> draws;
    std::vector<gear::Rotation> definitions;
    std::vector<gear::Mat> computed,bound;
    VECTOR3 offset{};
    unsigned id=~0u;
    double loaded=0;
    gear::Handles handles;
    void *handle=nullptr;
    // Explicit client-only integration fault: never mutate shared engine geometry.
    bool validationFaultForTest=false;
    bool restorationPending() const {return restore_pending_;}
    void attach(VESSEL *vessel) {
        mesh_=vessel->GetMeshTemplate(0); require(mesh_,"gear mesh missing");
        ANIMATION *fresh=nullptr; slots_=vessel->GetAnimPtr(&fresh);
        require(fresh && slots_<4096,"gear animations unavailable");
        for(unsigned i=0;i<slots_;++i) if(fresh[i].defstate==1 && fresh[i].ncomp==14) {require(id==~0u,"ambiguous gear signature"); id=i;}
        require(id<slots_,"DG gear signature absent");
        definitions=read(vessel);
        require(definitions==signature(),"not the bounded source-defined DG gear signature");
        require(vessel->GetMeshOffset(0,offset),"gear offset missing");
        require(std::isfinite(offset.x)&&std::isfinite(offset.y)&&std::isfinite(offset.z),"gear nonfinite offset");
        for(unsigned g=0;g<oapiMeshGroupCount(mesh_);++g) {
            auto p=oapiMeshGroup(mesh_,g); require(p && (p->Vtx||!p->nVtx) && (p->Idx||!p->nIdx),"gear null geometry");
            Group copy; if(p->nVtx) copy.vertices.assign(p->Vtx,p->Vtx+p->nVtx); if(p->nIdx) copy.indices.assign(p->Idx,p->Idx+p->nIdx);
            copy.flags=p->UsrFlag;copy.material=p->MtrlIdx;copy.texture=p->TexIdx; groups_.push_back(std::move(copy));
        }
        loaded=vessel->GetAnimation(id); computed=gear::evaluate(definitions,1,loaded,groups_.size()); bound=computed;
        owner_=vessel; restore_pending_=true;
        handle=handles.create();
        require(handles.valid(handle)&&!handles.valid(reinterpret_cast<void*>(1)),"gear handle registry");
        printf("[core-metal] gear_attach id=%u default=1 loaded=%.17g definitions=14 initial_evaluated=1 owned_handle=1\n",id,loaded);
    }
    void validate(VESSEL *vessel) const {
        require(vessel && vessel==owner_,"gear owner missing/replaced");
        require(!validationFaultForTest,"injected client-only gear validation fault (engine data untouched)");
        require(vessel->GetMeshTemplate(0)==mesh_ && oapiMeshGroupCount(mesh_)==groups_.size(),"gear mesh replaced/topology changed");
        VECTOR3 current; require(vessel->GetMeshOffset(0,current) && current.x==offset.x && current.y==offset.y && current.z==offset.z,"gear offset changed");
        for(unsigned i=0;i<groups_.size();++i) {
            auto p=oapiMeshGroup(mesh_,i);const auto &g=groups_[i];
            require(p && p->nVtx==g.vertices.size() && p->nIdx==g.indices.size() && p->UsrFlag==g.flags && p->MtrlIdx==g.material && p->TexIdx==g.texture,"gear group topology/metadata changed");
            require((!p->nVtx || (p->Vtx && !std::memcmp(p->Vtx,g.vertices.data(),p->nVtx*sizeof(NTVERTEX)))) && (!p->nIdx || (p->Idx && !std::memcmp(p->Idx,g.indices.data(),p->nIdx*sizeof(WORD)))),"shared gear template mutated");
        }
        require(read(vessel)==definitions,"gear definition changed");
    }
    void update(unsigned frame,unsigned split,bool freeze) {
        auto vessel=oapiGetFocusInterface(); validate(vessel);
        double state=frame<=split ? 0 : 1;
        require(vessel->SetAnimation(id,state),"gear engine setter rejected");
        ANIMATION *fresh=nullptr; require(vessel->GetAnimPtr(&fresh)==slots_ && fresh && fresh[id].state==state && vessel->GetAnimation(id)==state,"gear state overwritten");
        computed=gear::evaluate(read(vessel),1,state,groups_.size());
        if(!freeze || frame==1) bound=computed;
        printf("[core-metal] gear_frame_json={\"frame\":%u,\"state\":%.17g,\"readback\":%.17g,\"template_unchanged\":true,\"bound\":[",frame,state,vessel->GetAnimation(id));
        for(size_t g=0;g<bound.size();++g) {printf("%s[",g?",":"");for(int i=0;i<16;++i) printf("%s%.17g",i?",":"",double(bound[g][i]));printf("]");} printf("]}\n");
    }
    bool detach() noexcept {
        if(!handle && !restore_pending_) return true;
        // Device-token retirement and outstanding engine restoration are distinct.
        if(handle) {handles.retire(handle); handle=nullptr;}
        VESSEL *vessel=nullptr;
        bool valid=cleanupStep("gear validation",[&]{vessel=oapiGetFocusInterface();validate(vessel);});
        bool restored=cleanupStep("gear restore",[&]{
            require(vessel && vessel==owner_,"gear restore owner missing/replaced");
            // Geometry/offset diagnostics must not block a safe state restoration.
            // A removed/redefined slot must NOT receive the original state's setter.
            require(read(vessel)==definitions,"gear restore definition changed");
            require(vessel->SetAnimation(id,loaded)&&vessel->GetAnimation(id)==loaded,"gear restore failed");
            ANIMATION *fresh=nullptr;
            require(vessel->GetAnimPtr(&fresh)==slots_ && fresh && fresh[id].state==loaded,"gear restore array readback failed");
            restore_pending_=false;
        });
        printf("[core-metal] gear_detach stale_handle_rejected=1 template_unchanged=%u state_restored=%u restore_pending=%u\n",valid,restored,restore_pending_);
        return valid && restored;
    }
    void exportSource(const std::string &path) const {
        FILE *f=fopen(path.c_str(),"w");require(f,"gear source open");
        fprintf(f,"{\"offset\":[%.17g,%.17g,%.17g],\"definitions\":[",offset.x,offset.y,offset.z);
        for(size_t i=0;i<definitions.size();++i) {auto &r=definitions[i];fprintf(f,"%s{\"start\":%.17g,\"end\":%.17g,\"angle\":%.17g,\"pivot\":[%.17g,%.17g,%.17g],\"axis\":[%.17g,%.17g,%.17g],\"groups\":[",i?",":"",r.start,r.end,r.angle,r.pivot[0],r.pivot[1],r.pivot[2],r.axis[0],r.axis[1],r.axis[2]);for(size_t j=0;j<r.groups.size();++j) fprintf(f,"%s%u",j?",":"",r.groups[j]);fprintf(f,"]}");}
        fprintf(f,"],\"draws\":[");
        for(size_t i=0;i<draws.size();++i) {auto &d=draws[i];fprintf(f,"%s{\"group\":%u,\"base\":%zu,\"first\":%zu,\"count\":%zu,\"vertices\":[",i?",":"",d.group,d.base,d.first,d.count);auto &vs=groups_[d.group].vertices;for(size_t j=0;j<vs.size();++j) {auto &v=vs[j];fprintf(f,"%s[%.9g,%.9g,%.9g,%.9g,%.9g,%.9g]",j?",":"",v.x,v.y,v.z,v.nx,v.ny,v.nz);} fprintf(f,"]}");}
        fprintf(f,"]}\n");require(!ferror(f),"gear source write");require(fclose(f)==0,"gear source close");
    }
    static std::vector<gear::Rotation> signature();
};
inline std::vector<gear::Rotation> GearVisual::signature() {
    // GearSubsys.cpp:73-126 and meshres.h: exact DG external group IDs.
    return {
        {0.3,1,{0,-1.048,8.561},{1,0,0},-1.6580628156661987,{97,101}},
        {0.3,0.9,{0,-1.145,8.65},{1,0,0},-1.5707963705062866,{67,86}},
        {0,0.3,{-0.3,-1.222,7.029},{0,0.052,0.999},-1.5707963705062866,{68,87}},
        {0.7,1,{-0.3,-1.222,7.029},{0,0.052,0.999},1.5707963705062866,{68,87}},
        {0,0.3,{0.3,-1.222,7.029},{0,0.052,0.999},1.5707963705062866,{69,88}},
        {0.7,1,{0.3,-1.222,7.029},{0,0.052,0.999},-1.5707963705062866,{69,88}},
        {0,1,{-3.607,-1.137,-3.08},{0,0,1},-1.5707963705062866,{96,100}},
        {0,1,{3.607,-1.137,-3.08},{0,0,1},1.5707963705062866,{95,99}},
        {0,1,{-3.658,-1.239,-3.038},{0,0,1},-1.919862151145935,{65,66,84,85}},
        {0,0.3,{-2.175,-1.178,-3.438},{0,0,1},1.5707963705062866,{64,83}},
        {0.7,1,{-2.175,-1.178,-3.438},{0,0,1},-1.5707963705062866,{64,83}},
        {0,1,{3.658,-1.239,-3.038},{0,0,1},1.919862151145935,{61,62,80,81}},
        {0,0.3,{2.175,-1.178,-3.438},{0,0,1},-1.5707963705062866,{63,82}},
        {0.7,1,{2.175,-1.178,-3.438},{0,0,1},1.5707963705062866,{63,82}},
    };
}
} // namespace core_metal
