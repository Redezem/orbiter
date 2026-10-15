// Opt-in CPU-only real-engine probe. Input/output are scalar radians/metres.
#include <windows.h>
#include "Orbiter.h"
#include "Log.h"
#include "OrbiterAPI.h"
#include "cmdline.h"
#include <cstdio>
#include <cmath>
#include <cstdlib>
extern Orbiter *g_pOrbiter;
int main(int argc, char **argv) {
    if (argc != 4) return 2; // scenario, witness file, mode (0/1/2)
    FILE *input = fopen(argv[2], "r");
    if (!input) return 2;
    g_pOrbiter = new Orbiter;
    char cmd[512]; snprintf(cmd, sizeof(cmd), "--scenariox=\"%s\"", argv[1]);
    orbiter::CommandLine::Parse(g_pOrbiter, cmd);
    INITLOG("Orbiter_elevation.log", false);
    if (g_pOrbiter->Create(NULL) != S_OK) return 3;
    // Must be set BEFORE Launch constructs Earth/ElevationManager. No source config writes.
    g_pOrbiter->Cfg()->CfgVisualPrm.ElevMode = atoi(argv[3]);
    g_pOrbiter->Cfg()->CfgPRenderPrm.TileLoadFlags = 2; // actual archives only
    g_pOrbiter->Launch(argv[1]);
    char earthName[] = "Earth", sunName[] = "Sun";
    OBJHANDLE earth = oapiGetObjectByName(earthName);
    OBJHANDLE sun = oapiGetObjectByName(sunName);
    bool ok = earth && sun && oapiElevationManager(earth) && oapiGetFocusInterface();
    if (ok) {
        VECTOR3 p, s; MATRIX3 rotation;
        oapiGetGlobalPos(earth, &p); oapiGetGlobalPos(sun, &s);
        oapiGetRotationMatrix(earth, &rotation);
        const double radius = oapiGetSize(earth);
        for (double v : {p.x, p.y, p.z, s.x, s.y, s.z}) ok = ok && std::isfinite(v);
        for (double v : rotation.data) ok = ok && std::isfinite(v);
        ok = ok && std::isfinite(radius) && radius > 6000000;
        printf("earth_pose=%.17g,%.17g,%.17g sun_pose=%.17g,%.17g,%.17g finite=%d\n", p.x,p.y,p.z,s.x,s.y,s.z,ok);
    }
    auto *cache = InitTileCache(2); // eviction and mixed request levels
    double lng, lat; int requested;
    if (ok) {
        printf("radius=%.17g mode=%d tilesource=2\n", oapiGetSize(earth), atoi(argv[3]));
        while (fscanf(input, "%lf %lf %d", &lng, &lat, &requested) == 3) {
            int actual = -1, warmLevel = -1, localLevel = -1;
            double h = oapiSurfaceElevationEx(earth, lng, lat, requested, cache, nullptr, &actual);
            double warm = oapiSurfaceElevationEx(earth, lng, lat, requested, cache, nullptr, &warmLevel);
            double local = oapiSurfaceElevationEx(earth, lng, lat, requested, nullptr, nullptr, &localLevel);
            ok = ok && std::isfinite(h) && h == warm && h == local && actual == warmLevel && actual == localLevel;
            printf("sample %.17g %.17g %d %d %.17g %.17g %.17g\n", lng, lat, requested, actual, h, warm, local);
        }
    }
    fclose(input);
    ReleaseTileCache(cache); cache = nullptr;
    printf("cache_released_before_world_close=1\n");
    g_pOrbiter->Cfg()->CfgDebugPrm.ShutdownMode = 0;
    g_pOrbiter->SetFastExit(false);
    g_pOrbiter->CloseSession();
    delete g_pOrbiter; g_pOrbiter = nullptr;
    printf("world_closed=1 %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 4;
}
