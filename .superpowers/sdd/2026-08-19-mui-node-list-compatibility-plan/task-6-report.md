# Task 6 Report

Status: DONE

## Summary

- Extended `mui_node_list_simulator` to accept `--implementation legacy|virtual_candidate` only when `DEVICE_UI_MUI_VIRTUAL_NODE_LIST` is compiled.
- Added deterministic fixture controls shared by both modes: `--nodes`, `--seed`, and bounded automation via `--run-for-ms`.
- Added a gated Xvfb CTest that starts two separate simulator processes, one legacy and one virtual candidate, using the same fixture controls.
- Updated the launcher with `--process-mode legacy|virtual_candidate|pair`; pair mode builds with the real virtual gate and launches distinct legacy/virtual X11 processes.
- Kept existing X11/LVGL input path intact: simulator test hooks inject through LVGL input devices while normal X11 callbacks still flow through the original X11 driver callbacks when no test event is pending.
- Added a display guard to the aggregate doctest so full CTest does not segfault when `DISPLAY` is intentionally absent; the dedicated Xvfb test still exercises the X11 input path.

## TDD Evidence

- RED: `cmake -S . -B build-task6-red -DENABLE_MUI_X11_SIMULATOR=ON -DENABLE_DOCTESTS=ON -DENABLE_MUI_VIRTUAL_NODE_LIST=ON` failed with the old CMake guard requiring `ENABLE_MUI_HEADLESS_TESTS`.
- GREEN: after implementation, the same gated configure/build passed and `MuiX11SimulatorPairLaunch` passed under Xvfb.
- Regression RED found during full CTest: aggregate `tests` target segfaulted when the X11 doctest ran without `DISPLAY`.
- Regression GREEN: aggregate doctest now returns early without `DISPLAY`; dedicated `MuiX11SimulatorInput` continues to run under Xvfb.

## Verification

- `PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/build-task4-python/lib/python3.14/site-packages cmake -S . -B build-task6-red -DENABLE_MUI_X11_SIMULATOR=ON -DENABLE_DOCTESTS=ON -DENABLE_MUI_VIRTUAL_NODE_LIST=ON` passed.
- `PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/build-task4-python/lib/python3.14/site-packages cmake --build build-task6-red --target mui_node_list_simulator tests -j4` passed.
- `PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/build-task4-python/lib/python3.14/site-packages ctest --test-dir build-task6-red --output-on-failure` passed: 3/3.
- `PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/build-task4-python/lib/python3.14/site-packages cmake -S . -B build-task6-gateoff -DENABLE_MUI_X11_SIMULATOR=ON -DENABLE_DOCTESTS=ON` passed.
- `PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/build-task4-python/lib/python3.14/site-packages ctest --test-dir build-task6-gateoff --output-on-failure` passed: 3/3.
- `PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/build-task4-python/lib/python3.14/site-packages DISPLAY=:99 tools/launch_node_list_simulator.sh build-task6-red --process-mode pair --nodes 40 --seed 42 --run-for-ms 100` passed under Xvfb.
- `git diff --check -- CMakeLists.txt cmake/AssertMuiX11SimulatorRejectsVirtualCandidate.cmake cmake/RunMuiX11SimulatorPairLaunchTest.cmake tests/X11MuiSimulator.cpp tests/X11MuiSimulator.h tests/test_HeadlessDisplayDriver.cpp tools/mui_node_list_simulator.cpp tools/launch_node_list_simulator.sh` passed.
- Output label scan found only the existing benchmark label: `Host-relative structural, CPU, and allocator comparison; not hardware timing.`

## Concerns

- `trunk fmt` was run as requested, but it traversed untracked build directories and failed on generated `compiler_depend.ts` files. It formatted task files before failing; scoped `git diff --check` is clean. I did not stage or remove any untracked build directories.
- Original hard-coded Xvfb display collision concern is resolved in Fix Round 1 below.

## Fix Round 1 Evidence

Review findings addressed:

- P1: `MuiX11SimulatorPairLaunch` now launches both legacy and `virtual_candidate` as separate simulator processes, sends real X11 events through XTest for mouse drag, wheel/encoder-equivalent scroll, click, and keyboard input, and asserts per-process reports for observable LVGL state changes. The pair proof no longer relies on direct test-hook injection.
- P1: the pair report asserts `implementation=virtual_candidate` with `virtual_enabled=1`, while legacy reports `virtual_enabled=0`, so the test proves the real virtual host gate is compiled and active for the virtual process.
- P2: `RunMuiX11SimulatorInputTest.cmake` and `RunMuiX11SimulatorPairLaunchTest.cmake` no longer use fixed `:97`/`:98`; each creates an isolated temp dir, chooses a process-scoped display, verifies Xvfb is alive and accepting clients via `xdpyinfo`, and cleans up Xvfb/temp state.
- P2: `tools/launch_node_list_simulator.sh` pair mode now tracks all child PIDs and traps `EXIT`, `INT`, and `TERM` to terminate/reap children on errors or interrupts.
- Root cause for the first GREEN failure: both concurrently running simulator processes were sending XTest events to the same X display at the same time, racing focus/pointer ownership. The pair CTest now provides a shared temp-dir lock only around the XTest input exercise; both processes/windows are still launched together, and each receives real X11/LVGL input without cross-process pointer/focus trampling.
- Final verification caught formatter-induced include reordering that exposed X11's `None` macro before doctest; the simulator tool now keeps doctest before X11 headers and the fresh build/CTest chain passes.

TDD RED/GREEN:

- RED: after adding pair-report assertions for real X11 drag/wheel/click/key and `virtual_enabled=1`, `ctest --test-dir build-task6-red -R '^MuiX11SimulatorPairLaunch$' --output-on-failure` failed because the simulator did not yet support `--window-title`, `--exercise-x11-input`, or `--report`.
- GREEN: after adding the real XTest input exercise, per-process report output, unique Xvfb helpers, and PID cleanup, `MuiX11SimulatorPairLaunch` passed.

Fix-round verification:

- `trunk fmt CMakeLists.txt cmake/RunMuiX11SimulatorInputTest.cmake cmake/RunMuiX11SimulatorPairLaunchTest.cmake source/graphics/driver/X11Driver.cpp tests/X11MuiSimulator.cpp tests/X11MuiSimulator.h tools/mui_node_list_simulator.cpp tools/launch_node_list_simulator.sh` passed.
- `git diff --check` passed.
- `PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/build-task4-python/lib/python3.14/site-packages cmake --build build-task6-red --target mui_node_list_simulator tests -j4` passed.
- `PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/build-task4-python/lib/python3.14/site-packages ctest --test-dir build-task6-red -R '^MuiX11SimulatorPairLaunch$' --output-on-failure` passed: 1/1.
- `PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/build-task4-python/lib/python3.14/site-packages ctest --test-dir build-task6-red --output-on-failure` passed: 3/3.
- `PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/build-task4-python/lib/python3.14/site-packages cmake -S . -B build-task6-gateoff -DENABLE_MUI_X11_SIMULATOR=ON -DENABLE_DOCTESTS=ON && PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/build-task4-python/lib/python3.14/site-packages cmake --build build-task6-gateoff --target mui_node_list_simulator tests -j4 && PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/build-task4-python/lib/python3.14/site-packages ctest --test-dir build-task6-gateoff --output-on-failure` passed: 3/3.
- `PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/build-task4-python/lib/python3.14/site-packages DISPLAY=:190 tools/launch_node_list_simulator.sh build-task6-red --process-mode pair --nodes 8 --seed 42 --run-for-ms 50` passed under a throwaway Xvfb display.

Remaining caveat:

- Generated/build warnings from existing source remain noisy in build output. No standalone UI, hardware, or docs outside this SDD report were changed.

## Fix Round 2 Evidence

Review findings addressed:

- Pair proof now asserts each real XTest event class has its own before/after LVGL observable in both simulator processes:
  - drag: `drag_xtest_ok=1`, `drag_scroll_before`, `drag_scroll_after`, `drag_scroll_changed=1`
  - wheel/encoder-equivalent scroll: `wheel_xtest_ok=1`, `wheel_scroll_before`, `wheel_scroll_after`, `wheel_scroll_changed=1`
  - click: `click_xtest_ok=1`, nonzero `click_target`, selected/focus before/after fields, and `click_observable_changed=1`
  - key: `key_xtest_ok=1`, `key_focus_before`, `key_focus_after`, `key_focus_changed=1`
- Click target selection now finds a fully visible node-row button and avoids the current focused/selected row when possible. This keeps the proof on the real X11 pointer path while ensuring the click causes a selection/focus transition instead of landing on unrelated controls.
- Both CMake Xvfb helpers now use atomic display lock directories under `${TMPDIR:-/tmp}/mui-xvfb-display-locks`, start Xvfb on the reserved display, verify it is alive/ready via `xdpyinfo`, and retry/cleanup on start collision.
- Pair CMake launcher logic and `tools/launch_node_list_simulator.sh` both monitor child processes with `wait -n`, fail fast on either child error, and terminate/reap the sibling process through retained traps.

TDD RED/GREEN:

- RED: after tightening `MuiX11SimulatorPairLaunch` to require per-event report fields and not rely on unvalidated sent flags, `ctest --test-dir build-task6-red -R '^MuiX11SimulatorPairLaunch$' --output-on-failure` failed because the reports lacked the required per-event evidence.
- RED: `ctest --test-dir build-task6-red -R 'MuiX11Simulator(Input|PairLaunch)' -j2 --output-on-failure` exposed the concurrent Xvfb allocation/interference failure before the atomic reservation fix.
- GREEN: focused manual pair repro under throwaway Xvfb passed with both simulator processes returning 0. Example report evidence included legacy `click_selected_before=0`, `click_selected_after=2684354569`, `click_target=4310327432`, `click_observable_changed=1`; virtual `click_selected_before=2684354563`, `click_selected_after=2684354574`, `click_target=4305604784`, `click_observable_changed=1`; both also had `drag_xtest_ok=1`, `wheel_xtest_ok=1`, `key_xtest_ok=1`, and the corresponding changed fields.
- GREEN: after atomic Xvfb reservation, `ctest --test-dir build-task6-red -R 'MuiX11Simulator(Input|PairLaunch)' -j2 --output-on-failure` passed: 2/2.

Fix-round verification:

- `trunk fmt cmake/RunMuiX11SimulatorInputTest.cmake cmake/RunMuiX11SimulatorPairLaunchTest.cmake tests/X11MuiSimulator.cpp tests/X11MuiSimulator.h tools/mui_node_list_simulator.cpp tools/launch_node_list_simulator.sh` passed.
- `git diff --check` passed.
- `PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/.codex/task6-venv/lib/python3.14/site-packages cmake --build build-task6-red --target mui_node_list_simulator tests -j4` passed.
- `PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/.codex/task6-venv/lib/python3.14/site-packages ctest --test-dir build-task6-red -R '^MuiX11SimulatorPairLaunch$' --output-on-failure` passed: 1/1.
- `PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/.codex/task6-venv/lib/python3.14/site-packages ctest --test-dir build-task6-red --output-on-failure` passed: 3/3.
- `PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/.codex/task6-venv/lib/python3.14/site-packages ctest --test-dir build-task6-red -R 'MuiX11Simulator(Input|PairLaunch)' -j2 --output-on-failure` passed: 2/2.
- `cmake -S . -B build-task6-gateoff -DENABLE_MUI_X11_SIMULATOR=ON -DENABLE_DOCTESTS=ON -DPython_EXECUTABLE=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/.codex/task6-venv/bin/python && PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/.codex/task6-venv/lib/python3.14/site-packages cmake --build build-task6-gateoff --target mui_node_list_simulator tests -j4 && PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/.codex/task6-venv/lib/python3.14/site-packages ctest --test-dir build-task6-gateoff --output-on-failure` passed: 3/3.
- `PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/.codex/task6-venv/lib/python3.14/site-packages DISPLAY=<throwaway Xvfb> tools/launch_node_list_simulator.sh build-task6-red --process-mode pair --nodes 8 --seed 42 --run-for-ms 50` passed.

Remaining caveat:

- Build output still contains existing warnings in generated/common UI code. A disposable untracked venv under `.codex/task6-venv` was used only so nanopb could import `google.protobuf`; no untracked build or workspace directories were staged.
