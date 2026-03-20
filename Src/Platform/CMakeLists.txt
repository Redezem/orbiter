# Copyright (c) Martin Schweiger
# Licensed under the MIT License

add_library(OrbiterPlatformRuntime STATIC
	DynamicLibrary.cpp
	Paths.cpp
)

target_include_directories(OrbiterPlatformRuntime
	PUBLIC
		${CMAKE_SOURCE_DIR}/Orbitersdk/include
		${CMAKE_CURRENT_SOURCE_DIR}
)

if(UNIX AND NOT APPLE)
	target_link_libraries(OrbiterPlatformRuntime PUBLIC dl)
endif()

set_target_properties(OrbiterPlatformRuntime
	PROPERTIES
	FOLDER Core
)
