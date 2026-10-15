// Opt-in real-engine atmosphere integration probe. No replacement model.
#include <windows.h>
#include "Orbiter.h"
#include "Planet.h"
#include "CelBodyAPI.h"
#include "OrbiterAPI.h"
#include "cmdline.h"
#include <cstdio>
#include <cmath>
#include <cstring>
extern Orbiter *g_pOrbiter;
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    g_pOrbiter = new Orbiter;
    char cmd[512]; snprintf(cmd, sizeof(cmd), "--scenariox=\"%s\"", argv[1]);
    orbiter::CommandLine::Parse(g_pOrbiter, cmd);
    INITLOG("Orbiter_atmosphere.log", false);
    if (g_pOrbiter->Create(nullptr) != S_OK) return 3;
    g_pOrbiter->Launch(argv[1]);
    char name[] = "Earth";
    OBJHANDLE earth = oapiGetObjectByName(name);
    CELBODY *body = earth ? oapiGetCelbodyInterface(earth) : nullptr;
    ATMOSPHERE *model = body && body->Version() >= 2 ? static_cast<CELBODY2*>(body)->GetAtmosphere() : nullptr;
    const ATMCONST *constants = earth ? oapiGetPlanetAtmConstants(earth) : nullptr;
    bool ok = model && constants && oapiPlanetHasAtmosphere(earth);
    if (!ok) fprintf(stderr, "ERROR: Earth native atmosphere unavailable; explicit module selection required\n");
    if (ok) {
        printf("model=%s pointer=%p p0=%.17g rho0=%.17g R=%.17g gamma=%.17g altlimit=%.17g\n",
            model->clbkName(), static_cast<void*>(model), constants->p0, constants->rho0,
            constants->R, constants->gamma, constants->altlimit);
        ok = !strcmp(model->clbkName(), "J71G") && constants->p0 == 101325 &&
            constants->rho0 == 1.2247 && constants->R == 286.91 &&
            constants->gamma == 1.4 && constants->altlimit == 2500000;
        for (const char *bodyName : {"Sun", "Earth"}) {
            char objectName[32]; snprintf(objectName, sizeof(objectName), "%s", bodyName);
            OBJHANDLE h = oapiGetObjectByName(objectName);
            VECTOR3 pos, vel;
            if (!h) { ok = false; continue; }
            oapiGetGlobalPos(h, &pos); oapiGetGlobalVel(h, &vel);
            for (double x : {pos.x,pos.y,pos.z,vel.x,vel.y,vel.z}) ok = ok && std::isfinite(x);
            printf("body=%s pos=%.17g,%.17g,%.17g vel=%.17g,%.17g,%.17g\n", bodyName,pos.x,pos.y,pos.z,vel.x,vel.y,vel.z);
        }
        double previousP = HUGE_VAL, previousRho = HUGE_VAL;
        for (double alt : {0.,1000.,10000.,15000.,25000.,40000.,60000.,80000.,100000.,200000.,400000.}) {
            ATMPARAM p;
            oapiGetPlanetAtmParams(earth, alt, 0., 0., &p);
            // Actual engine sound-speed implementation, not a probe substitute.
            double sound = static_cast<Planet*>(earth)->AtmSoundSpeed(p.T);
            printf("sample %.17g %.17g %.17g %.17g %.17g\n", alt,p.T,p.p,p.rho,sound);
            ok = ok && std::isfinite(p.T) && std::isfinite(p.p) && std::isfinite(p.rho) &&
                std::isfinite(sound) && p.T > 0 && p.p > 0 && p.rho > 0 && sound > 0 &&
                p.p < previousP && p.rho < previousRho;
            previousP = p.p; previousRho = p.rho;
        }
        ATMPARAM outside{};
        bool rejected = !static_cast<Planet*>(earth)->GetAtmParam(2500001,0,0,&outside);
        printf("above_limit_rejected=%d\n", rejected && outside.p == 0 && outside.rho == 0 && outside.T == 0);
        ok = ok && rejected && outside.p == 0 && outside.rho == 0 && outside.T == 0;
    }
    g_pOrbiter->Cfg()->CfgDebugPrm.ShutdownMode = 0;
    g_pOrbiter->SetFastExit(false);
    g_pOrbiter->CloseSession();
    delete g_pOrbiter; g_pOrbiter = nullptr;
    printf("world_closed=1 %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 4;
}
