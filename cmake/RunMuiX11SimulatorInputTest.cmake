if(NOT DEFINED TESTS_EXECUTABLE)
    message(FATAL_ERROR "TESTS_EXECUTABLE is required")
endif()

find_program(XVFB_EXECUTABLE Xvfb HINTS /opt/X11/bin /usr/X11/bin /usr/local/bin /opt/homebrew/bin)
if(NOT XVFB_EXECUTABLE)
    message(FATAL_ERROR "Xvfb is required for MuiX11SimulatorInput")
endif()

find_program(XDPYINFO_EXECUTABLE xdpyinfo HINTS /opt/X11/bin /usr/X11/bin /usr/local/bin /opt/homebrew/bin)
if(NOT XDPYINFO_EXECUTABLE)
    message(FATAL_ERROR "xdpyinfo is required for MuiX11SimulatorInput")
endif()

set(test_script "set -e
test_dir=\$(mktemp -d \"\${TMPDIR:-/tmp}/mui-x11-input.XXXXXX\")
base_display=\$((190 + (\$\$ % 100)))
display_number=
for offset in \$(seq 0 99); do
    candidate=\":\$((base_display + offset))\"
    if [ ! -e \"/tmp/.X11-unix/X\${candidate#:}\" ]; then
        display_number=\"\${candidate}\"
        break
    fi
done
if [ -z \"\${display_number}\" ]; then
    echo \"could not allocate an Xvfb display\" >&2
    exit 1
fi
xvfb_log=\"\${test_dir}/xvfb.log\"
\"${XVFB_EXECUTABLE}\" \"\${display_number}\" -screen 0 320x240x24 > \"\${xvfb_log}\" 2>&1 &
xvfb_pid=\$!
cleanup() {
    kill \"\${xvfb_pid}\" >/dev/null 2>&1 || true
    wait \"\${xvfb_pid}\" >/dev/null 2>&1 || true
    rm -rf \"\${test_dir}\"
}
trap cleanup EXIT INT TERM
for attempt in \$(seq 1 50); do
    if ! kill -0 \"\${xvfb_pid}\" >/dev/null 2>&1; then
        echo \"Xvfb exited before accepting clients\" >&2
        cat \"\${xvfb_log}\" >&2 || true
        exit 1
    fi
    if DISPLAY=\"\${display_number}\" \"${XDPYINFO_EXECUTABLE}\" >/dev/null 2>&1; then
        break
    fi
    if [ \"\${attempt}\" -eq 50 ]; then
        echo \"Xvfb did not become ready\" >&2
        cat \"\${xvfb_log}\" >&2 || true
        exit 1
    fi
    sleep 0.1
done
DISPLAY=\"\${display_number}\" \"${TESTS_EXECUTABLE}\" '--test-case=*X11 simulator*'
")
execute_process(COMMAND bash -c "${test_script}" RESULT_VARIABLE test_result)

if(NOT test_result EQUAL 0)
    message(FATAL_ERROR "MuiX11SimulatorInput failed with exit code ${test_result}")
endif()
