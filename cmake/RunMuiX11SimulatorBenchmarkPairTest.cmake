if(NOT DEFINED MUI_NODE_LIST_SIMULATOR)
    message(FATAL_ERROR "MUI_NODE_LIST_SIMULATOR is required")
endif()
if(NOT DEFINED MUI_NODE_LIST_LEGACY_SIMULATOR)
    set(MUI_NODE_LIST_LEGACY_SIMULATOR "${MUI_NODE_LIST_SIMULATOR}")
endif()
if(NOT DEFINED MUI_NODE_LIST_VIRTUAL_SIMULATOR)
    set(MUI_NODE_LIST_VIRTUAL_SIMULATOR "${MUI_NODE_LIST_SIMULATOR}")
endif()

find_program(XVFB_EXECUTABLE Xvfb HINTS /opt/X11/bin /usr/X11/bin /usr/local/bin /opt/homebrew/bin)
if(NOT XVFB_EXECUTABLE)
    message(FATAL_ERROR "Xvfb is required for MuiX11SimulatorBenchmarkPair")
endif()

find_program(XDPYINFO_EXECUTABLE xdpyinfo HINTS /opt/X11/bin /usr/X11/bin /usr/local/bin /opt/homebrew/bin)
if(NOT XDPYINFO_EXECUTABLE)
    message(FATAL_ERROR "xdpyinfo is required for MuiX11SimulatorBenchmarkPair")
endif()

set(test_script "set -e
test_dir=\$(mktemp -d \"\${TMPDIR:-/tmp}/mui-x11-bench.XXXXXX\")
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
run_benchmark() {
    implementation=\"\$1\"
    simulator=\"\$2\"
    report=\"\${test_dir}/\${implementation}.report\"
    title=\"bench-\${implementation}\"
    DISPLAY=\"\${display_number}\" \"\${simulator}\" --implementation \"\${implementation}\" --nodes 250 --seed 42 --window-title \"\${title}\" --hardware-benchmark --tdeck-constrained --report \"\${report}\"
    test -s \"\${report}\"
    grep -q \"^implementation=\${implementation}$\" \"\${report}\"
    grep -q '^tdeck_constrained=1$' \"\${report}\"
    grep -q '^scope=tdeck-model constrained X11/LVGL simulator; not hardware timing$' \"\${report}\"
    grep -q '^tdeck_model_display_width=320$' \"\${report}\"
    grep -q '^tdeck_model_display_height=240$' \"\${report}\"
    grep -q '^tdeck_model_ui_period_ms=40$' \"\${report}\"
    grep -q '^tdeck_model_spi_hz=40000000$' \"\${report}\"
    grep -q '^tdeck_model_rgb565_frame_bytes=153600$' \"\${report}\"
    grep -q '^tdeck_model_frame_transfer_ms=30.72$' \"\${report}\"
    grep -q '^hardware_benchmark_fixtures=250$' \"\${report}\"
    grep -q '^hardware_benchmark_complete=1$' \"\${report}\"
    grep -q '^hardware_benchmark_report=MUI_NODE_LIST_HW_BENCH {' \"\${report}\"
    grep -q '\"n\":40' \"\${report}\"
    grep -q '\"e\":\"none\"' \"\${report}\"
}
start_xvfb
run_benchmark legacy \"${MUI_NODE_LIST_LEGACY_SIMULATOR}\"
run_benchmark virtual_candidate \"${MUI_NODE_LIST_VIRTUAL_SIMULATOR}\"
")
execute_process(COMMAND bash -c "${test_script}" RESULT_VARIABLE test_result)

if(NOT test_result EQUAL 0)
    message(FATAL_ERROR "MuiX11SimulatorBenchmarkPair failed with exit code ${test_result}")
endif()
