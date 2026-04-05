Include(FetchContent)

set(ORBITER_HAVE_LEGACY_TEST_TARGETS OFF)
if(TARGET ${OrbiterTgt} AND TARGET Orbitersdk AND TARGET LuaInterpreter)
	set(ORBITER_HAVE_LEGACY_TEST_TARGETS ON)
endif()

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.0.1
)

FetchContent_MakeAvailable(Catch2)

if(TARGET Catch2)
	target_compile_definitions(Catch2 PUBLIC CATCH_CONFIG_NO_POSIX_SIGNALS)
	if(NOT MSVC)
		target_compile_options(Catch2 PUBLIC -include cstdint)
	endif()
endif()

if(TARGET Catch2WithMain)
	target_compile_definitions(Catch2WithMain PUBLIC CATCH_CONFIG_NO_POSIX_SIGNALS)
	if(NOT MSVC)
		target_compile_options(Catch2WithMain PUBLIC -include cstdint)
	endif()
endif()

# Utility function
function(add_test_file test_name)
	add_executable(${test_name} "${test_name}.cpp")

	set_target_properties( ${test_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${ORBITER_BINARY_ROOT_DIR}" )

	target_include_directories(${test_name}
		PRIVATE ${ORBITER_SOURCE_SDK_INCLUDE_DIR}
		PRIVATE ${MODULE_COMMON_DIR}
		PRIVATE ${ORBITER_SOURCE_ROOT_DIR}/Src/Module/LuaScript/LuaInterpreter
	)

	target_link_libraries(${test_name}
		${ORBITER_LIB}
		${ORBITER_SDK_LIB}
		lua::lib
		${LUAINTERPRETER_LIB}
		Catch2::Catch2WithMain
	)

	add_dependencies(${test_name}
		${OrbiterTgt}
		Orbitersdk
		LuaInterpreter
	)

	add_test( 
		NAME ${test_name} 
		COMMAND $<TARGET_FILE:${test_name}>
		WORKING_DIRECTORY ${ORBITER_BINARY_ROOT_DIR}
	)
	set_tests_properties(${test_name} PROPERTIES TIMEOUT 30)
endfunction()

function(add_runtime_test_file test_name)
	add_executable(${test_name} "${test_name}.cpp")

	set_target_properties(${test_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${ORBITER_BINARY_ROOT_DIR}")

	target_include_directories(${test_name}
		PRIVATE ${CMAKE_SOURCE_DIR}/Src/Runtime
	)

	target_link_libraries(${test_name}
		PRIVATE
			OrbiterRuntime
			Catch2::Catch2WithMain
	)

	add_dependencies(${test_name}
		OrbiterRuntime
		CopyData
	)

	add_test(
		NAME ${test_name}
		COMMAND $<TARGET_FILE:${test_name}>
		WORKING_DIRECTORY ${ORBITER_BINARY_ROOT_DIR}
	)
	set_tests_properties(${test_name} PROPERTIES TIMEOUT 30)
endfunction()

if(ORBITER_HAVE_LEGACY_TEST_TARGETS)
	# Register unit tests
	add_test_file(Lua.Interpreter)
endif()

if(TARGET OrbiterRuntime)
	add_runtime_test_file(Runtime.Session)
endif()

if (BUILD_ORBITER_SERVER)

	add_test(
		NAME "Scenario.QuadcopterBootstrap"
		COMMAND $<TARGET_FILE:Orbiter_server>
			"--scenariox=${CMAKE_SOURCE_DIR}/Scenarios/Quadcopter/Quadcopter at KSC.scn"
			"--maxframes=60"
		WORKING_DIRECTORY ${ORBITER_BINARY_ROOT_DIR}
	)
	set_tests_properties(Scenario.QuadcopterBootstrap PROPERTIES TIMEOUT 60)

	add_test(
		NAME "Scenario.OrbiterCoreStubFails"
		COMMAND ${CMAKE_COMMAND}
			-DORBITER_SERVER=$<TARGET_FILE:Orbiter_server>
			-DSCENARIO_PATH=${CMAKE_SOURCE_DIR}/Scenarios/Quadcopter/Quadcopter\ at\ KSC.scn
			-DWORKING_DIRECTORY=${ORBITER_BINARY_ROOT_DIR}
			-DEXPECTED_ERROR=Orbiter\ core\ backend\ is\ not\ wired\ yet
			-P ${CMAKE_CURRENT_SOURCE_DIR}/RunOrbiterServerExpectFailure.cmake
		WORKING_DIRECTORY ${ORBITER_BINARY_ROOT_DIR}
	)
	set_tests_properties(Scenario.OrbiterCoreStubFails PROPERTIES TIMEOUT 60)

endif()
