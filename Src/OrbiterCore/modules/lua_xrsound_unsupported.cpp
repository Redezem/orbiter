// macOS port: the XRSound plugin is not ported yet, but the upstream
// Interpreter constructor calls LoadXRSoundAPI() unconditionally (no #ifdef
// at the call site), so the port must supply the entry points for
// LuaInterpreter to load at all. Upstream's own fallback semantics when the
// XRSound library is absent (lua_xrsound.cpp #ifndef XRSOUND branch:
// xrsound.create_instance raises a clean Lua error) are preserved here;
// registering an empty API namespace matches that build configuration.
#define INTERPRETER_IMPLEMENTATION

#include "Interpreter.h"

void Interpreter::LoadXRSoundAPI ()
{
	// No XRSound metatable or constants in this build.
}

int Interpreter::xrsound_create_instance (lua_State *L)
{
	return luaL_error (L, "Application not build with XRSound support");
}
