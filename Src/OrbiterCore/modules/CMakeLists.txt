# Native macOS vessel modules for the OrbiterCore host.
# These are the real Src/Vessel modules built as .dylib against the engine
# executable's exported symbols (the host links -force_load OrbiterCore, so
# engine API symbols are exported from the main executable and dlopen'd
# modules resolve against them).

set(ORB_MODULE_DIR ${CMAKE_BINARY_DIR}/Modules)

# lua_SOURCE_DIR is scoped to Extern/Lua; resolve the FetchContent source tree
set(ORB_LUA_SRC ${lua_SOURCE_DIR})
if(NOT EXISTS ${ORB_LUA_SRC}/lapi.c)
	set(ORB_LUA_SRC ${FETCHCONTENT_BASE_DIR}/lua-src)
endif()

# Lua interpreter library (Quadcopter's Lua interface links it).
# Glob like Extern/Lua does — this vendored Lua is 5.1 (no lctype/lbitlib).
file(GLOB ORB_LUA_C "${ORB_LUA_SRC}/*.c")
list(REMOVE_ITEM ORB_LUA_C "${ORB_LUA_SRC}/lua.c" "${ORB_LUA_SRC}/luac.c" "${ORB_LUA_SRC}/print.c" "${ORB_LUA_SRC}/ltests.c")
add_library(orb-lua SHARED ${ORB_LUA_C})
target_include_directories(orb-lua PUBLIC ${ORB_LUA_SRC})
set_target_properties(orb-lua PROPERTIES OUTPUT_NAME lua5.1)
target_link_libraries(orb-lua PRIVATE m)

# Quadcopter — the simplest real vessel module
add_library(Quadcopter SHARED
	${CMAKE_SOURCE_DIR}/Src/Vessel/Quadcopter/Quadcopter.cpp
	${CMAKE_SOURCE_DIR}/Src/Vessel/Quadcopter/QuadcopterLua.cpp
	${CMAKE_SOURCE_DIR}/Src/Vessel/Quadcopter/QuadcopterSubsys.cpp
	${CMAKE_SOURCE_DIR}/Src/Vessel/Quadcopter/PropulsionSubsys.cpp
	${CMAKE_SOURCE_DIR}/Src/Vessel/Common/Instrument.cpp
)
target_include_directories(Quadcopter PRIVATE
	${CMAKE_SOURCE_DIR}/Orbitersdk/include
	${CMAKE_SOURCE_DIR}/Src/Vessel/Common
	${CMAKE_SOURCE_DIR}/Src/Vessel/Quadcopter
)
target_link_libraries(Quadcopter PRIVATE orb-lua)
# Engine looks for "Modules/<name>.dll" -> shim maps to <name>.dylib;
# macOS dlopen needs the exact filename, so drop the "lib" prefix.
set_target_properties(Quadcopter PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY ${ORB_MODULE_DIR}
	PREFIX ""
)

# Engine API symbols (oapi*, VESSEL base) are not in a linkable library here:
# they live in the host executable, which exports them via -force_load.
# Resolve them dynamically at dlopen time.
target_link_options(Quadcopter PRIVATE -undefined dynamic_lookup)
