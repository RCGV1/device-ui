if(NOT DEFINED MUI_NODE_LIST_SIMULATOR)
    message(FATAL_ERROR "MUI_NODE_LIST_SIMULATOR is required")
endif()

find_program(XVFB_EXECUTABLE Xvfb HINTS /opt/X11/bin /usr/X11/bin /usr/local/bin /opt/homebrew/bin)
if(NOT XVFB_EXECUTABLE)
    message(FATAL_ERROR "Xvfb is required for MuiX11SimulatorWheelRequired")
endif()

find_program(XDPYINFO_EXECUTABLE xdpyinfo HINTS /opt/X11/bin /usr/X11/bin /usr/local/bin /opt/homebrew/bin)
if(NOT XDPYINFO_EXECUTABLE)
    message(FATAL_ERROR "xdpyinfo is required for MuiX11SimulatorWheelRequired")
endif()

set(test_script "set -e
test_dir=\$(mktemp -d \"\${TMPDIR:-/tmp}/mui-x11-wheel-required.XXXXXX\")
xvfb_log=\"\${test_dir}/xvfb.log\"
xvfb_pid=
display_lock=
cleanup() {
    if [ -n \"\${xvfb_pid}\" ]; then
        kill \"\${xvfb_pid}\" >/dev/null 2>&1 || true
        wait \"\${xvfb_pid}\" >/dev/null 2>&1 || true
    fi
    if [ -n \"\${display_lock}\" ]; then
        rmdir \"\${display_lock}\" >/dev/null 2>&1 || true
    fi
    rm -rf \"\${test_dir}\"
}
trap cleanup EXIT INT TERM
reserve_display() {
    lock_root=\"\${TMPDIR:-/tmp}/mui-xvfb-display-locks\"
    mkdir -p \"\${lock_root}\"
    for reserve_attempt in \$(seq 1 100); do
        candidate=\$((100 + ((\$\$ + \${RANDOM} + \${reserve_attempt}) % 800)))
        candidate_lock=\"\${lock_root}/X\${candidate}.lock\"
        if mkdir \"\${candidate_lock}\" 2>/dev/null; then
            display_number=\":\${candidate}\"
            display_lock=\"\${candidate_lock}\"
            return 0
        fi
    done
    return 1
}
start_xvfb() {
    for start_attempt in \$(seq 1 20); do
        if ! reserve_display; then
            echo \"failed to reserve an Xvfb display\" >&2
            exit 1
        fi
        \"${XVFB_EXECUTABLE}\" \"\${display_number}\" -screen 0 320x240x24 > \"\${xvfb_log}\" 2>&1 &
        xvfb_pid=\$!
        for ready_attempt in \$(seq 1 50); do
            if ! kill -0 \"\${xvfb_pid}\" >/dev/null 2>&1; then
                break
            fi
            if DISPLAY=\"\${display_number}\" \"${XDPYINFO_EXECUTABLE}\" >/dev/null 2>&1; then
                return 0
            fi
            sleep 0.1
        done
        kill \"\${xvfb_pid}\" >/dev/null 2>&1 || true
        wait \"\${xvfb_pid}\" >/dev/null 2>&1 || true
        xvfb_pid=
        rmdir \"\${display_lock}\" >/dev/null 2>&1 || true
        display_lock=
    done
    echo \"Xvfb did not become ready\" >&2
    cat \"\${xvfb_log}\" >&2 || true
    exit 1
}
start_xvfb
report=\"\${test_dir}/wheel.report\"
set +e
DISPLAY=\"\${display_number}\" DEVICE_UI_X11_SUPPRESS_WHEEL_XTEST=1 \"${MUI_NODE_LIST_SIMULATOR}\" --implementation legacy --nodes 40 --seed 42 --run-for-ms 500 --window-title task6-wheel-required --exercise-x11-input --report \"\${report}\"
sim_status=\$?
set -e
if [ \"\${sim_status}\" -eq 0 ]; then
    echo \"simulator passed even though wheel XTest delivery was suppressed\" >&2
    cat \"\${report}\" >&2 || true
    exit 1
fi
test -s \"\${report}\"
grep -q '^wheel_xtest_ok=0$' \"\${report}\"
grep -q '^wheel_scroll_stable_before=1$' \"\${report}\"
grep -q '^wheel_observable_changed=0$' \"\${report}\"
")
execute_process(COMMAND bash -c "${test_script}" RESULT_VARIABLE test_result)

if(NOT test_result EQUAL 0)
    message(FATAL_ERROR "MuiX11SimulatorWheelRequired failed with exit code ${test_result}")
endif()
