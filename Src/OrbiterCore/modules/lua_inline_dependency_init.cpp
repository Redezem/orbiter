// Native counterpart of the SDK DllMain process-attach/detach callbacks for
// the LuaInline module (same seam as celbody_dependency_init.cpp for
// ephemeris libraries). The dynamic loader has no Windows process-attach
// entry point, so invoke the real exported callbacks: InitModule creates the
// InterpreterList and registers it with the engine (module callbacks +
// opc* interpreter API), ExitModule deletes it after the engine's
// clbkSimulationEnd teardown.
#include "OrbiterAPI.h"
extern "C" void InitModule(HINSTANCE);
extern "C" void ExitModule(HINSTANCE);
__attribute__((constructor)) static void initialiseLuaInlineModule()
{
    InitModule(nullptr);
}
__attribute__((destructor)) static void releaseLuaInlineModule()
{
    ExitModule(nullptr);
}
