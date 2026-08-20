if(NOT DEFINED TESTS_EXECUTABLE)
    message(FATAL_ERROR "TESTS_EXECUTABLE is required")
endif()

find_program(XVFB_EXECUTABLE Xvfb HINTS /opt/X11/bin /usr/X11/bin /usr/local/bin /opt/homebrew/bin)
if(NOT XVFB_EXECUTABLE)
    message(FATAL_ERROR "Xvfb is required for MuiX11SimulatorInput")
endif()

set(display_number ":97")
set(xvfb_log "${CMAKE_CURRENT_BINARY_DIR}/mui-x11-simulator-xvfb.log")
set(test_script "set -e
\"${XVFB_EXECUTABLE}\" \"${display_number}\" -screen 0 320x240x24 > \"${xvfb_log}\" 2>&1 &
xvfb_pid=\$!
trap 'kill \${xvfb_pid} >/dev/null 2>&1 || true' EXIT
sleep 1
DISPLAY=\"${display_number}\" \"${TESTS_EXECUTABLE}\" '--test-case=*X11 simulator*'
")
execute_process(COMMAND bash -c "${test_script}" RESULT_VARIABLE test_result)

if(NOT test_result EQUAL 0)
    message(FATAL_ERROR "MuiX11SimulatorInput failed with exit code ${test_result}")
endif()
