if(NOT DEFINED MUI_NODE_LIST_SIMULATOR)
    message(FATAL_ERROR "MUI_NODE_LIST_SIMULATOR is required")
endif()

find_program(XVFB_EXECUTABLE Xvfb HINTS /opt/X11/bin /usr/X11/bin /usr/local/bin /opt/homebrew/bin)
if(NOT XVFB_EXECUTABLE)
    message(FATAL_ERROR "Xvfb is required for MuiX11SimulatorPairLaunch")
endif()

set(display_number ":98")
set(xvfb_log "${CMAKE_CURRENT_BINARY_DIR}/mui-x11-simulator-pair-xvfb.log")
set(test_script "set -e
\"${XVFB_EXECUTABLE}\" \"${display_number}\" -screen 0 320x240x24 > \"${xvfb_log}\" 2>&1 &
xvfb_pid=\$!
legacy_pid=
virtual_pid=
cleanup() {
    if [ -n \"\${legacy_pid}\" ]; then kill \"\${legacy_pid}\" >/dev/null 2>&1 || true; fi
    if [ -n \"\${virtual_pid}\" ]; then kill \"\${virtual_pid}\" >/dev/null 2>&1 || true; fi
    kill \"\${xvfb_pid}\" >/dev/null 2>&1 || true
}
trap cleanup EXIT
sleep 1
DISPLAY=\"${display_number}\" \"${MUI_NODE_LIST_SIMULATOR}\" --implementation legacy --nodes 40 --seed 42 --run-for-ms 250 &
legacy_pid=\$!
DISPLAY=\"${display_number}\" \"${MUI_NODE_LIST_SIMULATOR}\" --implementation virtual_candidate --nodes 40 --seed 42 --run-for-ms 250 &
virtual_pid=\$!
wait \"\${legacy_pid}\"
legacy_pid=
wait \"\${virtual_pid}\"
virtual_pid=
")
execute_process(COMMAND bash -c "${test_script}" RESULT_VARIABLE test_result)

if(NOT test_result EQUAL 0)
    message(FATAL_ERROR "MuiX11SimulatorPairLaunch failed with exit code ${test_result}")
endif()
