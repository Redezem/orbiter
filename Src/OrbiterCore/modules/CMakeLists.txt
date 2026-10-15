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

# meshc: mesh label scanner used to generate vessel meshres headers
add_executable(meshc
	${CMAKE_SOURCE_DIR}/Utils/meshc/meshc.cpp
	${CMAKE_SOURCE_DIR}/Utils/meshc/Mesh.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/D3dmath.cpp
)
target_include_directories(meshc PRIVATE
	${CMAKE_SOURCE_DIR}/Utils/meshc
	${CMAKE_SOURCE_DIR}/Src/Orbiter            # D3dmath.h, Vecmat.h
	${CMAKE_SOURCE_DIR}/Src/OrbiterCore/shim   # d3d.h types for Mesh.h
	${CMAKE_SOURCE_DIR}/Orbitersdk/include     # OrbiterPlatform.h for the shim
)

# DeltaGlider — the flagship vessel module (needs meshc-generated headers)
set(DG_SRC_DIR ${CMAKE_SOURCE_DIR}/Src/Vessel/DeltaGlider)
foreach (spec "deltaglider:-" "deltaglider_vc:_VC" "dg_2dpanel0:_P0" "dg_2dpanel1:_P1")
	string(REPLACE ":" ";" spec_parts ${spec})
	list(GET spec_parts 0 msh)
	list(GET spec_parts 1 suffix)
	if (suffix STREQUAL "-")
		set(out meshres.h)
	else()
		set(out meshres${suffix}.h)
	endif()
	add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${out}
		COMMAND $<TARGET_FILE:meshc> /I ${DG_SRC_DIR}/Meshes/${msh}.msh /P ${suffix} /O ${CMAKE_CURRENT_BINARY_DIR}/${out}
		DEPENDS ${DG_SRC_DIR}/Meshes/${msh}.msh meshc
	)
endforeach()

# Anchor the generated headers to a target so the custom commands run
add_custom_target(DeltaGlider_Prerequisites
	DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/meshres.h
	        ${CMAKE_CURRENT_BINARY_DIR}/meshres_VC.h
	        ${CMAKE_CURRENT_BINARY_DIR}/meshres_P0.h
	        ${CMAKE_CURRENT_BINARY_DIR}/meshres_P1.h
)

add_library(DeltaGlider SHARED
	${DG_SRC_DIR}/DeltaGlider.cpp
	${DG_SRC_DIR}/DGLua.cpp
	${DG_SRC_DIR}/AAPSubsys.cpp
	${DG_SRC_DIR}/AerodynSubsys.cpp
	${DG_SRC_DIR}/AvionicsSubsys.cpp
	${DG_SRC_DIR}/DGSubsys.cpp
	${DG_SRC_DIR}/DockingSubsys.cpp
	${DG_SRC_DIR}/FailureSubsys.cpp
	${DG_SRC_DIR}/GearSubsys.cpp
	${DG_SRC_DIR}/HoverSubsys.cpp
	${DG_SRC_DIR}/HudCtrl.cpp
	${DG_SRC_DIR}/LightSubsys.cpp
	${DG_SRC_DIR}/MainRetroSubsys.cpp
	${DG_SRC_DIR}/MfdSubsys.cpp
	${DG_SRC_DIR}/PressureSubsys.cpp
	${DG_SRC_DIR}/RcsSubsys.cpp
	${DG_SRC_DIR}/ScramSubsys.cpp
	${DG_SRC_DIR}/ThermalSubsys.cpp
	${DG_SRC_DIR}/DGSwitches.cpp
	${DG_SRC_DIR}/FuelMfd.cpp
	${DG_SRC_DIR}/Horizon.cpp
	${DG_SRC_DIR}/InstrAoa.cpp
	${DG_SRC_DIR}/InstrHsi.cpp
	${DG_SRC_DIR}/InstrVs.cpp
	${DG_SRC_DIR}/MomentInd.cpp
	${CMAKE_SOURCE_DIR}/Src/Vessel/Common/Instrument.cpp
)
target_include_directories(DeltaGlider PRIVATE
	${CMAKE_SOURCE_DIR}/Orbitersdk/include
	${CMAKE_SOURCE_DIR}/Src/OrbiterCore/shim   # Win32 types for the scenario-editor interface
	${CMAKE_SOURCE_DIR}/Src/Vessel/Common
	${CMAKE_SOURCE_DIR}/Src/Plugin/ScnEditor   # ScnEditorAPI.h
	${DG_SRC_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
	${ORB_IMGUI_SRC}
)
target_link_libraries(DeltaGlider PRIVATE orb-lua)
add_dependencies(DeltaGlider DeltaGlider_Prerequisites)
target_link_options(DeltaGlider PRIVATE -undefined dynamic_lookup)
set_target_properties(DeltaGlider PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY ${ORB_MODULE_DIR}
	PREFIX ""
)

# ShuttleA — legacy shuttle vessel (meshc-generated meshres)
set(SA_SRC_DIR ${CMAKE_SOURCE_DIR}/Src/Vessel/ShuttleA)
set(SA_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/shuttlea_gen)
file(MAKE_DIRECTORY ${SA_GEN_DIR})
add_custom_command(OUTPUT ${SA_GEN_DIR}/meshres.h
	COMMAND $<TARGET_FILE:meshc> /I ${SA_SRC_DIR}/Meshes/ShuttleA.msh /P - /O ${SA_GEN_DIR}/meshres.h
	DEPENDS ${SA_SRC_DIR}/Meshes/ShuttleA.msh meshc
)
add_custom_target(ShuttleA_Prerequisites DEPENDS ${SA_GEN_DIR}/meshres.h)

file(GLOB SA_SRCS ${SA_SRC_DIR}/*.cpp)
add_library(ShuttleA SHARED
	${SA_SRCS}
	${CMAKE_SOURCE_DIR}/Src/Vessel/Common/Instrument.cpp
)
target_include_directories(ShuttleA PRIVATE
	${CMAKE_SOURCE_DIR}/Orbitersdk/include
	${CMAKE_SOURCE_DIR}/Src/OrbiterCore/shim
	${CMAKE_SOURCE_DIR}/Src/Vessel/Common
	${CMAKE_SOURCE_DIR}/Src/Plugin/ScnEditor
	${SA_SRC_DIR}
	${SA_GEN_DIR}
)
target_link_libraries(ShuttleA PRIVATE orb-lua)
add_dependencies(ShuttleA ShuttleA_Prerequisites)
target_link_options(ShuttleA PRIVATE -undefined dynamic_lookup)
set_target_properties(ShuttleA PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY ${ORB_MODULE_DIR}
	PREFIX ""
)

# ShuttlePB — minimal payload-bay shuttle variant, no generated headers
add_library(ShuttlePB SHARED
	${CMAKE_SOURCE_DIR}/Src/Vessel/ShuttlePB/ShuttlePB.cpp
)
target_include_directories(ShuttlePB PRIVATE
	${CMAKE_SOURCE_DIR}/Orbitersdk/include
	${CMAKE_SOURCE_DIR}/Src/OrbiterCore/shim
)
target_link_options(ShuttlePB PRIVATE -undefined dynamic_lookup)
set_target_properties(ShuttlePB PROPERTIES
	LIBRARY_OUTPUT_DIRECTORY ${ORB_MODULE_DIR}
	PREFIX ""
)
