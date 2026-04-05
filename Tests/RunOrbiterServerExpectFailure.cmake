if(NOT DEFINED ORBITER_SERVER)
	message(FATAL_ERROR "ORBITER_SERVER is required")
endif()

if(NOT DEFINED SCENARIO_PATH)
	message(FATAL_ERROR "SCENARIO_PATH is required")
endif()

if(NOT DEFINED EXPECTED_ERROR)
	message(FATAL_ERROR "EXPECTED_ERROR is required")
endif()

if(NOT DEFINED WORKING_DIRECTORY)
	set(WORKING_DIRECTORY ".")
endif()

execute_process(
	COMMAND "${ORBITER_SERVER}" "--scenariox=${SCENARIO_PATH}" "--backend=orbiter-core"
	WORKING_DIRECTORY "${WORKING_DIRECTORY}"
	RESULT_VARIABLE run_result
	OUTPUT_VARIABLE run_stdout
	ERROR_VARIABLE run_stderr
)

if(run_result EQUAL 0)
	message(FATAL_ERROR "Orbiter_server unexpectedly succeeded.\nstdout:\n${run_stdout}\nstderr:\n${run_stderr}")
endif()

string(CONCAT combined_output "${run_stdout}" "${run_stderr}")
string(FIND "${combined_output}" "${EXPECTED_ERROR}" expected_index)
if(expected_index EQUAL -1)
	message(FATAL_ERROR "Expected error text was not found.\nstdout:\n${run_stdout}\nstderr:\n${run_stderr}")
endif()
