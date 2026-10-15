# Spike: compile the real Orbiter engine core natively on macOS.
# Strategy: curated source list + a small shim include dir that satisfies the
# handful of Win32 function calls the engine makes (timeGetTime, GetLastError,
# MAX_PATH, ...). Win32 *types* are already provided by OrbiterPlatform.h.

set(ORB_CORE_SRC
	# Math / astronomy / time
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Vecmat.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Astro.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Element.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Psys.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/TimeData.cpp

	# Celestial bodies
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Body.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/BodyIntegrator.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/PinesGrav.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Celbody.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Planet.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Rigidbody.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Star.cpp

	# Vessels
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Vessel.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Vesselbase.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Vesselstatus.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/SuperVessel.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/FlightRecorder.cpp

	# Surface bases
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Base.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Baseobj.cpp

	# Camera / mesh / terrain
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Camera.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Mesh.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/ZTreeMgr.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/elevmgr.cpp

	# Config / state / utils
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Config.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/State.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Util.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Log.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/cmdline.cpp

	# API implementations
	${CMAKE_SOURCE_DIR}/Src/Orbiter/OrbiterAPI.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/GraphicsAPI.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/DrawAPI.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/ModuleAPI.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/MFDAPI.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/CelSphereAPI.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/CamAPI.cpp
)

add_library(OrbiterCore STATIC ${ORB_CORE_SRC})

target_include_directories(OrbiterCore PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/shim
	${CMAKE_SOURCE_DIR}/Src/Orbiter
	${CMAKE_SOURCE_DIR}/Orbitersdk/include
	${CMAKE_SOURCE_DIR}/OVP
	${CMAKE_SOURCE_DIR}/Src/Platform
)

target_link_libraries(OrbiterCore PRIVATE
	OrbiterPlatformRuntime
	Orbitersdk
	imgui
	implot
	zlib
)
