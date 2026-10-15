// Native counterpart of the SDK DllMain callbacks for ephemeris libraries.
// The native loader has no Windows SDK process-attach entry point, including
// for support libraries loaded transitively by dyld. Invoke the real exported
// callbacks, which initialise stock theory data/caches (no handle is used).
#include "OrbiterAPI.h"
extern "C" void InitModule(HINSTANCE);
__attribute__((constructor)) static void initialiseEphemerisDependency()
{
    InitModule(nullptr);
}
#ifdef ORB_EPHEMERIS_HAS_EXIT
extern "C" void ExitModule(HINSTANCE);
__attribute__((destructor)) static void releaseEphemerisDependency()
{
    ExitModule(nullptr);
}
#endif
