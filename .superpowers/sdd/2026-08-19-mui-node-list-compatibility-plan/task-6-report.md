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
