if(NOT DEFINED NODE_LIST_BENCH OR NOT EXISTS "${NODE_LIST_BENCH}")
    message(FATAL_ERROR "NODE_LIST_BENCH must name the node_list_bench executable")
endif()

set(OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/node-list-scroll-telemetry-production.json")
file(REMOVE "${OUTPUT_PATH}")
execute_process(
    COMMAND "${NODE_LIST_BENCH}" --nodes 250 --trials 1 --seed 42 --implementation production --json "${OUTPUT_PATH}"
    RESULT_VARIABLE RESULT
)
if(NOT RESULT EQUAL 0)
    message(FATAL_ERROR "node_list_bench failed: ${RESULT}")
endif()

file(READ "${OUTPUT_PATH}" JSON)
string(JSON TELEMETRY_TYPE ERROR_VARIABLE ERROR TYPE "${JSON}" scroll_telemetry)
if(ERROR OR NOT TELEMETRY_TYPE STREQUAL "OBJECT")
    message(FATAL_ERROR "missing scroll_telemetry: ${ERROR}")
endif()
foreach(FIELD cycles rows_per_cycle sample_count frame_count elapsed_ns average_fps worst_frame_ns)
    string(JSON FIELD_TYPE ERROR_VARIABLE ERROR TYPE "${JSON}" scroll_telemetry "${FIELD}")
    if(ERROR OR NOT FIELD_TYPE STREQUAL "NUMBER")
        message(FATAL_ERROR "scroll_telemetry.${FIELD} must be numeric: ${ERROR}")
    endif()
endforeach()

string(JSON CYCLES GET "${JSON}" scroll_telemetry cycles)
string(JSON ROWS GET "${JSON}" scroll_telemetry rows_per_cycle)
string(JSON SAMPLES GET "${JSON}" scroll_telemetry sample_count)
string(JSON FRAMES GET "${JSON}" scroll_telemetry frame_count)
string(JSON ELAPSED GET "${JSON}" scroll_telemetry elapsed_ns)
string(JSON FPS GET "${JSON}" scroll_telemetry average_fps)
string(JSON WORST_FRAME GET "${JSON}" scroll_telemetry worst_frame_ns)
math(EXPR EXPECTED_SAMPLES "${CYCLES} * ${ROWS}")
if(NOT CYCLES EQUAL 2 OR NOT ROWS EQUAL 250 OR NOT SAMPLES EQUAL EXPECTED_SAMPLES OR NOT FRAMES EQUAL SAMPLES OR
   ELAPSED LESS_EQUAL 0 OR FPS LESS_EQUAL 0 OR WORST_FRAME LESS_EQUAL 0 OR WORST_FRAME GREATER ELAPSED)
    message(FATAL_ERROR "scroll telemetry is not a complete 250-row frame sample")
endif()
file(REMOVE "${OUTPUT_PATH}")
