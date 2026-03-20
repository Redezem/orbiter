# Copyright (c) Martin Schweiger
# Licensed under the MIT License

add_library(OrbiterBootstrap STATIC
	Bootstrap.cpp
)

target_include_directories(OrbiterBootstrap
	PUBLIC
		${CMAKE_SOURCE_DIR}/Orbitersdk/include
		${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(OrbiterBootstrap
	PUBLIC OrbiterPlatformRuntime
)

set_target_properties(OrbiterBootstrap
	PROPERTIES
	FOLDER Core
)
