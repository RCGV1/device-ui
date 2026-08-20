# Task 6 Report

Status: DONE_WITH_CONCERNS

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
- Parallel full CTest runs for the two X11 build dirs collide because both Xvfb helper scripts use display `:97`; rerunning serially passed.
