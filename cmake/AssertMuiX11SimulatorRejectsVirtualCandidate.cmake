execute_process(
    COMMAND "${MUI_NODE_LIST_SIMULATOR}" --implementation virtual_candidate
    RESULT_VARIABLE simulator_result
    OUTPUT_VARIABLE simulator_output
    ERROR_VARIABLE simulator_error
)

if(simulator_result EQUAL 0)
    message(FATAL_ERROR "expected virtual_candidate to be rejected")
endif()

set(simulator_combined_output "${simulator_output}${simulator_error}")
if(NOT simulator_combined_output MATCHES "virtual_candidate is not integrated")
    message(FATAL_ERROR "missing virtual_candidate not-integrated diagnostic: ${simulator_combined_output}")
endif()

message(STATUS "${simulator_combined_output}")
