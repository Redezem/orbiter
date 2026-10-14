# Copyright (c) Martin Schweiger
# Licensed under the MIT License

add_library(OrbiterBootstrap STATIC
	OrbiterMesh.cpp
	SimulationSession.cpp
	TerrainArchive.cpp
)

target_include_directories(OrbiterBootstrap
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_SOURCE_DIR}/Src/Runtime
)

target_link_libraries(OrbiterBootstrap
	PUBLIC OrbiterPlatformRuntime
	PRIVATE zlib
)

set_target_properties(OrbiterBootstrap
	PROPERTIES
	FOLDER Core
)
