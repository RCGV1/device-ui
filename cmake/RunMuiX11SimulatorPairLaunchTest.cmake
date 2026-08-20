if(NOT DEFINED MUI_NODE_LIST_SIMULATOR)
    message(FATAL_ERROR "MUI_NODE_LIST_SIMULATOR is required")
endif()

find_program(XVFB_EXECUTABLE Xvfb HINTS /opt/X11/bin /usr/X11/bin /usr/local/bin /opt/homebrew/bin)
if(NOT XVFB_EXECUTABLE)
    message(FATAL_ERROR "Xvfb is required for MuiX11SimulatorPairLaunch")
endif()

find_program(XDPYINFO_EXECUTABLE xdpyinfo HINTS /opt/X11/bin /usr/X11/bin /usr/local/bin /opt/homebrew/bin)
if(NOT XDPYINFO_EXECUTABLE)
    message(FATAL_ERROR "xdpyinfo is required for MuiX11SimulatorPairLaunch")
endif()

set(test_script "set -e
test_dir=\$(mktemp -d \"\${TMPDIR:-/tmp}/mui-x11-pair.XXXXXX\")
base_display=\$((90 + (\$\$ % 100)))
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
\"${XVFB_EXECUTABLE}\" \"\${display_number}\" -screen 0 640x240x24 > \"\${xvfb_log}\" 2>&1 &
xvfb_pid=\$!
legacy_pid=
virtual_pid=
cleanup() {
    for pid in \"\${legacy_pid}\" \"\${virtual_pid}\"; do
        if [ -n \"\${pid}\" ]; then
            kill \"\${pid}\" >/dev/null 2>&1 || true
            wait \"\${pid}\" >/dev/null 2>&1 || true
        fi
    done
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
legacy_report=\"\${test_dir}/legacy.report\"
virtual_report=\"\${test_dir}/virtual.report\"
input_lock=\"\${test_dir}/x11-input.lock\"
DISPLAY=\"\${display_number}\" DEVICE_UI_X11_INPUT_LOCK=\"\${input_lock}\" \"${MUI_NODE_LIST_SIMULATOR}\" --implementation legacy --nodes 40 --seed 42 --run-for-ms 900 --window-title task6-legacy --exercise-x11-input --report \"\${legacy_report}\" &
legacy_pid=\$!
DISPLAY=\"\${display_number}\" DEVICE_UI_X11_INPUT_LOCK=\"\${input_lock}\" \"${MUI_NODE_LIST_SIMULATOR}\" --implementation virtual_candidate --nodes 40 --seed 42 --run-for-ms 900 --window-title task6-virtual --exercise-x11-input --report \"\${virtual_report}\" &
virtual_pid=\$!
wait \"\${legacy_pid}\"
legacy_pid=
wait \"\${virtual_pid}\"
virtual_pid=
for report in \"\${legacy_report}\" \"\${virtual_report}\"; do
    test -s \"\${report}\"
    grep -q '^drag_sent=1$' \"\${report}\"
    grep -q '^wheel_sent=1$' \"\${report}\"
    grep -q '^click_sent=1$' \"\${report}\"
    grep -q '^key_sent=1$' \"\${report}\"
    grep -q '^scroll_changed=1$' \"\${report}\"
done
grep -q '^implementation=legacy$' \"\${legacy_report}\"
grep -q '^virtual_enabled=0$' \"\${legacy_report}\"
grep -q '^implementation=virtual_candidate$' \"\${virtual_report}\"
grep -q '^virtual_enabled=1$' \"\${virtual_report}\"
")
execute_process(COMMAND bash -c "${test_script}" RESULT_VARIABLE test_result)

if(NOT test_result EQUAL 0)
    message(FATAL_ERROR "MuiX11SimulatorPairLaunch failed with exit code ${test_result}")
endif()
