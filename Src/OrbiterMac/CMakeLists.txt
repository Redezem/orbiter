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
		${CMAKE_SOURCE_DIR}/Src/Runtime
		${CMAKE_SOURCE_DIR}/Src/Bootstrap
)

target_link_libraries(Orbiter_macOS
	PRIVATE
		OrbiterRuntime
		SDL3::SDL3
		zlib
		${FOUNDATION_FRAMEWORK}
		${METAL_FRAMEWORK}
		${METALKIT_FRAMEWORK}
		${QUARTZCORE_FRAMEWORK}
)

add_dependencies(Orbiter_macOS CopyData)

# Bundle the SDL3 shared library into the app so it launches without any
# system-wide SDL install, then ad-hoc re-sign the bundle.
add_custom_command(TARGET Orbiter_macOS POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_DIR:Orbiter_macOS>/Contents/Frameworks"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"$<TARGET_FILE:SDL3::SDL3>"
		"$<TARGET_BUNDLE_DIR:Orbiter_macOS>/Contents/Frameworks/libSDL3.0.dylib"
	COMMAND codesign --force -s - "$<TARGET_BUNDLE_DIR:Orbiter_macOS>/Contents/Frameworks/libSDL3.0.dylib"
	COMMAND codesign --force -s - "$<TARGET_BUNDLE_DIR:Orbiter_macOS>"
)

set_target_properties(Orbiter_macOS
	PROPERTIES
		BUILD_RPATH "@executable_path/../Frameworks"
		INSTALL_RPATH "@executable_path/../Frameworks"
		RUNTIME_OUTPUT_DIRECTORY ${ORBITER_BINARY_ROOT_DIR}
		BUNDLE_OUTPUT_DIRECTORY ${ORBITER_BINARY_ROOT_DIR}
		OUTPUT_NAME Orbiter
		FOLDER Core
)

install(TARGETS Orbiter_macOS
	BUNDLE DESTINATION .
	RUNTIME DESTINATION ${ORBITER_INSTALL_ROOT_DIR}
)
