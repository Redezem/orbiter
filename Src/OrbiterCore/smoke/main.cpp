// Path B smoke harness: run the REAL Orbiter engine core headlessly on
// macOS — no graphics client, no Win32 shell. Exercises the engine
// lifecycle: config load, scenario launch, world init, simulation frames,
// state readback through the public API, scenario save, clean close.
//
// Usage: orbiter-core-smoke <scenario-name> [frames]
//   run from the Orbiter content root (Config/, Textures/, Scenarios/ ...)

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <windows.h>   // portability shim layer (must precede engine headers)
#include "Orbiter.h"
#include "Psys.h"
#include "Vessel.h"
#include "Celbody.h"
#include "OrbiterAPI.h"
#include "cmdline.h"

extern Orbiter *g_pOrbiter;

int main (int argc, char **argv)
{
	const char *scenario = (argc > 1) ? argv[1] : "Demo/Tutorial 1";
	int frames = (argc > 2) ? atoi(argv[2]) : 120;

	printf("[smoke] scenario='%s' frames=%d\n", scenario, frames);

	g_pOrbiter = new Orbiter;

	// Parse a command line that forces the scenario (bypasses launchpad)
	char cmdline[512];
	snprintf(cmdline, sizeof(cmdline), "--scenariox=\"%s\"", scenario);
	orbiter::CommandLine::Parse(g_pOrbiter, cmdline);

	INITLOG("Orbiter_smoke.log", false);

	if (g_pOrbiter->Create(NULL) != S_OK) {
		printf("[smoke] FAIL: Orbiter::Create\n");
		return 2;
	}
	printf("[smoke] Create OK\n");

	// Launch the scenario (creates session; no gclient -> headless)
	g_pOrbiter->Launch(scenario);

	if (!oapiGetFocusInterface()) {
		printf("[smoke] FAIL: session/focus not created\n");
		return 3;
	}
	printf("[smoke] session OK\n");

	// Step the world headlessly
	for (int i = 0; i < frames; i++)
		g_pOrbiter->SingleFrame();
	printf("[smoke] %d frames stepped\n", frames);

	// Read back state through the public API
	VESSEL *v = oapiGetFocusInterface();
	if (!v) {
		printf("[smoke] FAIL: no focus vessel\n");
		return 4;
	}
	VESSELSTATUS vs;
	memset(&vs, 0, sizeof(vs));
	v->GetStatus(vs);

	double alt = 0, gs = 0;
	bool have_alt = oapiGetFocusAltitude(&alt);
	// VESSEL is the public module interface, not the internal object handle.
	// Casting it to OBJHANDLE makes the API interpret unrelated memory as Vessel.
	oapiGetGroundspeed(v->GetHandle(), &gs);

	// Surface quantities are undefined in orbit; fall back to orbital state.
	double peri = 0, apo = 0;
	bool have_orbit = (v->GetPeDist(peri) && v->GetApDist(apo)) != 0;
	double rv_mag = sqrt(vs.rpos.x*vs.rpos.x + vs.rpos.y*vs.rpos.y + vs.rpos.z*vs.rpos.z);
	double vel_mag = sqrt(vs.rvel.x*vs.rvel.x + vs.rvel.y*vs.rvel.y + vs.rvel.z*vs.rvel.z);

	printf("[smoke] focus vessel: %s (%s)\n", v->GetName(), v->GetClassName());
	printf("[smoke] alt=%s%.1f m  groundspeed=%.1f m/s  eng_main=%.2f\n",
		have_alt ? "" : "?", alt, gs, vs.eng_main);
	if (have_orbit)
		printf("[smoke] orbit: peri=%.1f km  apo=%.1f km  |r|=%.1f km  |v|=%.1f m/s\n",
			peri/1000.0, apo/1000.0, rv_mag/1000.0, vel_mag);

	// Save the resulting state as a scenario file
	bool saved = g_pOrbiter->SaveScenario("SmokeSaved", "smoke harness", 2);
	printf("[smoke] SaveScenario %s\n", saved ? "OK" : "FAIL");

	// Exercise real teardown instead of allowing configured fast shutdown
	// to exit(0) before cleanup and the final verification marker.
	g_pOrbiter->Cfg()->CfgDebugPrm.ShutdownMode = 0;
	g_pOrbiter->SetFastExit(false);
	g_pOrbiter->CloseSession();
	printf("[smoke] CloseSession OK\n");

	delete g_pOrbiter;
	g_pOrbiter = NULL;

	printf("[smoke] PASS\n");
	return saved ? 0 : 5;
}
