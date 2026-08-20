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
xvfb_log=\"\${test_dir}/xvfb.log\"
xvfb_pid=
display_lock=
legacy_pid=
virtual_pid=
cleanup() {
    for pid in \"\${legacy_pid}\" \"\${virtual_pid}\"; do
        if [ -n \"\${pid}\" ]; then
            kill \"\${pid}\" >/dev/null 2>&1 || true
            wait \"\${pid}\" >/dev/null 2>&1 || true
        fi
    done
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
        \"${XVFB_EXECUTABLE}\" \"\${display_number}\" -screen 0 640x240x24 > \"\${xvfb_log}\" 2>&1 &
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
wait_for_pair() {
    status=0
    remaining=2
    check_child() {
        child_name=\"\$1\"
        child_pid=\"\$2\"
        if [ -z \"\${child_pid}\" ]; then
            return 0
        fi
        if jobs -pr | grep -Fx \"\${child_pid}\" >/dev/null 2>&1; then
            return 0
        fi
        if wait \"\${child_pid}\"; then
            child_status=0
        else
            child_status=\$?
        fi
        if [ \"\${child_name}\" = legacy ]; then
            legacy_pid=
        else
            virtual_pid=
        fi
        remaining=\$((remaining - 1))
        if [ \"\${child_status}\" -ne 0 ]; then
            status=\"\${child_status}\"
            return 1
        fi
        return 0
    }
    while [ \"\${remaining}\" -gt 0 ]; do
        check_child legacy \"\${legacy_pid}\" || break
        check_child virtual \"\${virtual_pid}\" || break
        if [ \"\${remaining}\" -gt 0 ]; then
            sleep 0.05
        fi
    done
    if [ \"\${status}\" -ne 0 ]; then
        kill \"\${legacy_pid}\" \"\${virtual_pid}\" >/dev/null 2>&1 || true
        wait \"\${legacy_pid}\" >/dev/null 2>&1 || true
        wait \"\${virtual_pid}\" >/dev/null 2>&1 || true
        legacy_pid=
        virtual_pid=
        exit \"\${status}\"
    fi
    legacy_pid=
    virtual_pid=
}
legacy_report=\"\${test_dir}/legacy.report\"
virtual_report=\"\${test_dir}/virtual.report\"
input_lock=\"\${test_dir}/x11-input.lock\"
DISPLAY=\"\${display_number}\" DEVICE_UI_X11_INPUT_LOCK=\"\${input_lock}\" \"${MUI_NODE_LIST_SIMULATOR}\" --implementation legacy --nodes 40 --seed 42 --run-for-ms 900 --window-title task6-legacy --exercise-x11-input --report \"\${legacy_report}\" &
legacy_pid=\$!
DISPLAY=\"\${display_number}\" DEVICE_UI_X11_INPUT_LOCK=\"\${input_lock}\" \"${MUI_NODE_LIST_SIMULATOR}\" --implementation virtual_candidate --nodes 40 --seed 42 --run-for-ms 900 --window-title task6-virtual --exercise-x11-input --report \"\${virtual_report}\" &
virtual_pid=\$!
wait_for_pair
for report in \"\${legacy_report}\" \"\${virtual_report}\"; do
    test -s \"\${report}\"
    grep -q '^drag_xtest_ok=1$' \"\${report}\"
    grep -q '^drag_momentum_disabled=1$' \"\${report}\"
    grep -q '^drag_scroll_before=' \"\${report}\"
    grep -q '^drag_scroll_after=' \"\${report}\"
    grep -q '^drag_scroll_changed=1$' \"\${report}\"
    grep -q '^wheel_xtest_ok=1$' \"\${report}\"
    grep -q '^wheel_input=xtest_button5_mouse_wheel_encoder$' \"\${report}\"
    grep -q '^wheel_settle_before=' \"\${report}\"
    grep -q '^wheel_settle_after=' \"\${report}\"
    grep -q '^wheel_scroll_stable_before=1$' \"\${report}\"
    grep -q '^wheel_scroll_before=' \"\${report}\"
    grep -q '^wheel_scroll_after=' \"\${report}\"
    grep -q '^wheel_selected_before=' \"\${report}\"
    grep -q '^wheel_selected_after=' \"\${report}\"
    grep -q '^wheel_focus_before=' \"\${report}\"
    grep -q '^wheel_focus_after=' \"\${report}\"
    grep -q '^wheel_observable_changed=1$' \"\${report}\"
    grep -q '^click_xtest_ok=1$' \"\${report}\"
    grep -q '^click_selected_before=' \"\${report}\"
    grep -q '^click_selected_after=' \"\${report}\"
    grep -q '^click_focus_before=' \"\${report}\"
    grep -q '^click_focus_after=' \"\${report}\"
    grep -Eq '^click_target=[1-9][0-9]*$' \"\${report}\"
    grep -q '^click_observable_changed=1$' \"\${report}\"
    grep -q '^key_xtest_ok=1$' \"\${report}\"
    grep -q '^key_input=xtest_page_down_key$' \"\${report}\"
    grep -q '^key_focus_before=' \"\${report}\"
    grep -q '^key_focus_after=' \"\${report}\"
    grep -q '^key_selected_before=' \"\${report}\"
    grep -q '^key_selected_after=' \"\${report}\"
    grep -q '^key_scroll_before=' \"\${report}\"
    grep -q '^key_scroll_after=' \"\${report}\"
    grep -q '^key_observable_changed=1$' \"\${report}\"
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
