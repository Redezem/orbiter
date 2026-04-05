# Copyright (c) Martin Schweiger
# Licensed under the MIT License

add_subdirectory(Platform)
add_subdirectory(Bootstrap)
add_subdirectory(Runtime)

if(BUILD_ORBITER_SERVER)
	add_subdirectory(OrbiterServer)
endif()

if(APPLE AND ORBITER_BUILD_MACOS_APP)
	add_subdirectory(OrbiterMac)
endif()

if(ORBITER_BUILD_LEGACY_WIN32)
	add_subdirectory(Orbiter)
	add_subdirectory(Orbitersdk)
	add_subdirectory(DlgCtrl)
	add_subdirectory(Celbody)
	add_subdirectory(Vessel)
	add_subdirectory(Module)
	add_subdirectory(Plugin)
endif()

# ==========================================================================
# SDK Sample Installation
# ==========================================================================

set(IgnoreFiles
	PATTERN CMakeLists.txt EXCLUDE
)

if(ORBITER_BUILD_LEGACY_WIN32)
	install(DIRECTORY Module/LuaScript/LuaInline/ DESTINATION ${ORBITER_INSTALL_SDK_DIR}/samples/LuaInline ${IgnoreFiles})
	install(DIRECTORY Module/LuaScript/LuaInterpreter/ DESTINATION ${ORBITER_INSTALL_SDK_DIR}/samples/LuaInterpreter/ ${IgnoreFiles})
endif()
