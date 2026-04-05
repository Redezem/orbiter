# Copyright (c) Martin Schweiger
# Licensed under the MIT License

add_library(OrbiterRuntime STATIC
	Runtime.cpp
)

target_include_directories(OrbiterRuntime
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}
	PRIVATE
		${CMAKE_SOURCE_DIR}/Src/Bootstrap
		${CMAKE_SOURCE_DIR}/Src/Platform
)

target_link_libraries(OrbiterRuntime
	PRIVATE
		OrbiterBootstrap
		OrbiterPlatformRuntime
)

set_target_properties(OrbiterRuntime
	PROPERTIES
	FOLDER Core
)
