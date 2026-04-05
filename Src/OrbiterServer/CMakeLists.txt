# Copyright (c) Martin Schweiger
# Licensed under the MIT License

add_executable(Orbiter_server
	main.cpp
)

target_include_directories(Orbiter_server
	PRIVATE
		${CMAKE_SOURCE_DIR}/Src/Runtime
)

target_link_libraries(Orbiter_server
	PRIVATE OrbiterRuntime
)

add_dependencies(Orbiter_server CopyData)

set_target_properties(Orbiter_server
	PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY ${ORBITER_BINARY_ROOT_DIR}
		FOLDER Core
)

install(TARGETS Orbiter_server
	RUNTIME DESTINATION ${ORBITER_INSTALL_ROOT_DIR}
)
