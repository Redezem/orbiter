# Spike: compile the real Orbiter engine core natively on macOS.
# Strategy: curated source list + a small shim include dir that satisfies the
# handful of Win32 function calls the engine makes (timeGetTime, GetLastError,
# MAX_PATH, ...). Win32 *types* are already provided by OrbiterPlatform.h.

# Resolve the actual FetchContent source trees (imgui_SOURCE_DIR may point at
# the Extern staging dir which only holds headers).
set(ORB_IMGUI_SRC ${imgui_SOURCE_DIR})
if(NOT EXISTS ${ORB_IMGUI_SRC}/imgui.cpp)
	set(ORB_IMGUI_SRC ${FETCHCONTENT_BASE_DIR}/imgui-src)
endif()
set(ORB_IMPLOT_SRC ${implot_SOURCE_DIR})
if(NOT EXISTS ${ORB_IMPLOT_SRC}/implot.cpp)
	set(ORB_IMPLOT_SRC ${FETCHCONTENT_BASE_DIR}/implot-src)
endif()

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
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Keymap.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Memstat.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/ConsoleManager.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Help.cpp

	# Input / navigation / rendering support
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Input.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Nav.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Shadow.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/LightEmitter.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/PlaybackEd.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/VectorMap.cpp

	# Panes / panels / MFDs
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Pane.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Panel.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Panel2D.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Defpanel.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/hud.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/MenuInfoBar.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/VCockpit.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Mfd.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/MfdAlign.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/MfdComms.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/MfdDocking.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/MfdHsi.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/MfdLanding.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/MfdMap.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/MfdMap_old.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/MfdOrbit.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/MfdSurface.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/MfdSync.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/MfdTransfer.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/MfdUser.cpp

	# Dialog manager + ImGui dialogs (the engine's ImGui front end)
	${CMAKE_SOURCE_DIR}/Src/Orbiter/DlgMgr.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Select.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/DialogWin.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/CustomControls.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/DlgCamera.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/DlgCapture.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/DlgFocus.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/DlgFunction.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/DlgHelp.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/DlgInfo.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/DlgMap.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/DlgMenuCfg.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/DlgOptions.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/DlgRecorder.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/DlgTacc.cpp
	# NOTE: console_ng.cpp (Win32 console server mode) is excluded from the
	# macOS core for now; a native headless console mode is a later task.

	# NOTE: the legacy Win32 launchpad shell (Launchpad, LpadTab, Tab*,
	# OptionsPages) is deliberately EXCLUDED from the macOS core — it is the
	# Windows-only launcher front end. Orbiter.cpp guards its references
	# under ORBITER_NO_WIN32_SHELL; the macOS front end is the Metal client
	# + the ImGui dialogs above.

	# Engine main + module system
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Orbiter.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Script.cpp

	# macOS input bridge backing the DirectInput shim
	${CMAKE_CURRENT_SOURCE_DIR}/shim/shim_input.cpp

	# Fallbacks for the Win32 custom-control library (DlgCtrl gauge/switch
	# entry points that vessel modules reference; inert on the native port)
	${CMAKE_CURRENT_SOURCE_DIR}/shim/shim_dlgctrl.cpp

	# Graphics utils (engine-side helpers shared with clients)
	${CMAKE_SOURCE_DIR}/Src/Orbiter/D3d7util.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/D3dmath.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/Di7frame.cpp

	# API implementations
	${CMAKE_SOURCE_DIR}/Src/Orbiter/OrbiterAPI.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/GraphicsAPI.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/DrawAPI.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/ModuleAPI.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/MFDAPI.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/CelSphereAPI.cpp
	${CMAKE_SOURCE_DIR}/Src/Orbiter/CamAPI.cpp

	# ImGui + ImPlot implementation (imgui/implot targets are INTERFACE-only;
	# the engine archive needs the actual sources). imgui_SOURCE_DIR may point
	# at the Extern staging dir, so resolve the FetchContent source tree.
	${ORB_IMGUI_SRC}/imgui.cpp
	${ORB_IMGUI_SRC}/imgui_draw.cpp
	${ORB_IMGUI_SRC}/imgui_tables.cpp
	${ORB_IMGUI_SRC}/imgui_widgets.cpp
	${ORB_IMGUI_SRC}/misc/cpp/imgui_stdlib.cpp
	${ORB_IMPLOT_SRC}/implot.cpp
	${ORB_IMPLOT_SRC}/implot_items.cpp
)

add_library(OrbiterCore STATIC ${ORB_CORE_SRC})

# Engine core builds without the Win32 launchpad/dialog shell (macOS path)
target_compile_definitions(OrbiterCore PRIVATE ORBITER_NO_WIN32_SHELL)

# Generate about.hpp (build signature header) like the Windows Orbiter target does
string(TIMESTAMP ORB_DATE "%d %b %Y")
string(TIMESTAMP ORB_VERSION "%y%m%d")
configure_file(${CMAKE_SOURCE_DIR}/Src/Orbiter/about.hpp.in ${CMAKE_CURRENT_BINARY_DIR}/about.hpp @ONLY)

target_include_directories(OrbiterCore PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/shim
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/Src/Orbiter
	${CMAKE_SOURCE_DIR}/Orbitersdk/include
	${CMAKE_SOURCE_DIR}/OVP
	${CMAKE_SOURCE_DIR}/Src/Platform
	${imgui_SOURCE_DIR}
	${implot_SOURCE_DIR}
	${TRACY_CLIENT_INCLUDE}
)

target_link_libraries(OrbiterCore PRIVATE
	OrbiterPlatformRuntime
	imgui
	implot
	zlib
)

# Headless engine smoke harness: links the whole OrbiterCore archive and
# runs the real engine lifecycle with no graphics client.
add_executable(orbiter-core-smoke ${CMAKE_CURRENT_SOURCE_DIR}/smoke/main.cpp)
target_compile_definitions(orbiter-core-smoke PRIVATE ORBITER_NO_WIN32_SHELL)
target_include_directories(orbiter-core-smoke PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/shim
	${CMAKE_SOURCE_DIR}/Src/Orbiter
	${CMAKE_SOURCE_DIR}/Orbitersdk/include
	${CMAKE_SOURCE_DIR}/OVP
	${CMAKE_SOURCE_DIR}/Src/Platform
	${imgui_SOURCE_DIR}
	${implot_SOURCE_DIR}
	${TRACY_CLIENT_INCLUDE}
)
target_link_libraries(orbiter-core-smoke PRIVATE
	-Wl,-force_load,$<TARGET_FILE:OrbiterCore>
	OrbiterCore
	OrbiterPlatformRuntime
	zlib
)

# dlopen'd vessel modules resolve @rpath dependencies against the CWD
# (the content root) and the executable's directory.
target_link_options(orbiter-core-smoke PRIVATE
	-Wl,-rpath,.
	-Wl,-rpath,@executable_path
)

# Native vessel modules (real Src/Vessel sources built as .dylib against the
# engine host's exported symbols)
add_subdirectory(modules)

# Opt-in native Metal callback/lifecycle probe, independent of the bootstrap app.
option(ORBITER_BUILD_CORE_METAL "Build the real-core hidden Metal lifecycle probe" OFF)
if(APPLE AND ORBITER_BUILD_CORE_METAL)
	add_subdirectory(metal)
endif()
