#pragma once
// Contract discovery only: no transform evaluator, device mesh or animated GPU path.
#include <cstring>

namespace core_metal {
template<class Client> void probeAnimationContract(Client &client) {
    auto require=[](bool ok,const char *message) { if(!ok) throw std::runtime_error(message); };
    VESSEL *vessel=oapiGetFocusInterface();
    require(vessel!=nullptr,"animation contract focus missing");
    ANIMATION *animations=nullptr;
    unsigned count=vessel->GetAnimPtr(&animations), gear=count;
    require(count>0 && count<4096 && animations,"animation definitions unavailable");
    // Identify the source-defined DG gear structurally, never assume its runtime ID.
    for(unsigned a=0;a<count;++a) {
        const auto &animation=animations[a];
        if(animation.ncomp!=14 || animation.defstate!=1) continue;
        bool match=true;
        for(unsigned c=0;c<animation.ncomp;++c) {
            auto component=animation.comp[c];
            match=match && component && component->trans &&
                component->trans->Type()==MGROUP_TRANSFORM::ROTATE &&
                component->trans->mesh==0 && !component->parent && !component->nchildren;
        }
        if(match) { require(gear==count,"ambiguous DG gear animation"); gear=a; }
    }
    require(gear<count,"source-defined 14-rotation DG gear not found");
    auto mesh=vessel->GetMeshTemplate(0);
    require(mesh!=nullptr,"animation contract mesh missing");
    auto snapshot=[&]() {
        std::vector<unsigned char> bytes;
        auto append=[&](const void *data,size_t size) {
            const auto *p=static_cast<const unsigned char*>(data);
            if(size) bytes.insert(bytes.end(),p,p+size);
        };
        for(unsigned g=0;g<oapiMeshGroupCount(mesh);++g) {
            auto group=oapiMeshGroup(mesh,g);
            require(group!=nullptr,"animation contract null group");
            append(group->Vtx,group->nVtx*sizeof(NTVERTEX));
            append(group->Idx,group->nIdx*sizeof(WORD));
        }
        return bytes;
    };
    const auto original=snapshot();
    require(!original.empty(),"animation contract empty template");
    const double initial=vessel->GetAnimation(gear);
    printf("[core-metal] animation_contract initial_id=%u animation_slots=%u defstate=%.17g state=%.17g components=14 template_bytes=%zu; NOT animated rendering\n",gear,count,animations[gear].defstate,initial,original.size());
    for(unsigned c=0;c<14;++c) {
        const auto *component=animations[gear].comp[c];
        const auto *rotation=static_cast<const MGROUP_ROTATE*>(component->trans);
        require(rotation->grp && rotation->ngrp>0 && rotation->ngrp<16,"gear group list unsupported");
        require(component->state0<component->state1,"gear interval invalid");
        printf("[core-metal] animation_component_json={\"component\":%u,\"mesh\":%u,\"kind\":\"rotate\",\"state0\":%.17g,\"state1\":%.17g,\"parent\":false,\"children\":0,\"pivot\":[%.17g,%.17g,%.17g],\"axis\":[%.17g,%.17g,%.17g],\"angle\":%.17g,\"groups\":[",c,rotation->mesh,component->state0,component->state1,rotation->ref.x,rotation->ref.y,rotation->ref.z,rotation->axis.x,rotation->axis.y,rotation->axis.z,rotation->angle);
        for(unsigned g=0;g<rotation->ngrp;++g) {
            require(rotation->grp[g]<oapiMeshGroupCount(mesh),"gear group outside template");
            printf("%s%u",g ? "," : "",rotation->grp[g]);
        }
        printf("]}\n");
    }
    // Positive transport control: the same visibility value still emits a real
    // engine event. This changes no visibility or geometry and is read back.
    unsigned before=client.visualEventCount();
    auto visibility=vessel->GetMeshVisibilityMode(0);
    vessel->SetMeshVisibilityMode(0,visibility);
    require(vessel->GetMeshVisibilityMode(0)==visibility,"visibility changed");
    require(client.visualEventCount()==before+1 && client.lastVisualEvent==EVENT_VESSEL_MESHVISMODE && client.lastVisualContext==0 && client.lastVisualIdentityOK,"registered engine visual event not delivered");
    printf("[core-metal] animation_contract event_transport=1 identity=1 message=%u context=0 handled=0\n",EVENT_VESSEL_MESHVISMODE);
    require(vessel->GetDevMesh(reinterpret_cast<VISHANDLE>(&client),0)==nullptr,"static client unexpectedly exposes a device mesh");
    for(double requested : {0.0,0.5,1.0}) {
        before=client.visualEventCount();
        require(vessel->SetAnimation(gear,requested),"real engine animation setter rejected");
        ANIMATION *fresh=nullptr;
        require(vessel->GetAnimPtr(&fresh)==count && fresh,"animation array changed");
        const double observed=vessel->GetAnimation(gear);
        require(observed==requested && fresh[gear].state==requested,"engine animation readback mismatch");
        require(client.visualEventCount()==before,"SetAnimation unexpectedly emitted visual event");
        require(vessel->GetMeshTemplate(0)==mesh && snapshot()==original,"shared template mutated by SetAnimation");
        printf("[core-metal] animation_state_json={\"requested\":%.17g,\"observed\":%.17g,\"definition_state\":%.17g,\"visual_events\":0,\"template_unchanged\":true}\n",requested,observed,fresh[gear].state);
    }
    require(vessel->SetAnimation(gear,initial) && vessel->GetAnimation(gear)==initial,"animation initial state restore failed");
    require(snapshot()==original,"template changed after restore");
    printf("[core-metal] animation_contract verified=1 restored=%.17g device_mesh=null; API-controlled engine state only, GPU remains static\n",initial);
}
} // namespace core_metal
