# Copyright (c) Martin Schweiger
# Licensed under the MIT License

enable_language(OBJCXX)

find_package(SDL3 CONFIG QUIET)

if(NOT SDL3_FOUND)
	message(WARNING "SDL3 not found. Skipping Orbiter_macOS target.")
	return()
endif()

find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
find_library(METAL_FRAMEWORK Metal REQUIRED)
find_library(METALKIT_FRAMEWORK MetalKit REQUIRED)
find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)

add_executable(Orbiter_macOS MACOSX_BUNDLE
	main.cpp
	OrbiterMacApp.mm
)

target_include_directories(Orbiter_macOS
	PRIVATE
		${CMAKE_SOURCE_DIR}/Orbitersdk/include
		${CMAKE_SOURCE_DIR}/Src/Bootstrap
)

target_link_libraries(Orbiter_macOS
	PRIVATE
		OrbiterBootstrap
		SDL3::SDL3
		${FOUNDATION_FRAMEWORK}
		${METAL_FRAMEWORK}
		${METALKIT_FRAMEWORK}
		${QUARTZCORE_FRAMEWORK}
)

add_dependencies(Orbiter_macOS CopyData)

set_target_properties(Orbiter_macOS
	PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY ${ORBITER_BINARY_ROOT_DIR}
		BUNDLE_OUTPUT_DIRECTORY ${ORBITER_BINARY_ROOT_DIR}
		OUTPUT_NAME Orbiter
		FOLDER Core
)

install(TARGETS Orbiter_macOS
	BUNDLE DESTINATION .
	RUNTIME DESTINATION ${ORBITER_INSTALL_ROOT_DIR}
)
