### Task 5 Report: Real interaction parity on recycled rows

Status: complete

Implementation:
- Added real virtual-row LVGL event routing for click, long press, focus, and expanded position-label clicks.
- Wired `TFTView_320x240` as the virtual `NodeListActionSink` using NodeId model helpers instead of retained row pointers.
- Preserved the default legacy path: virtual list behavior remains behind the existing default-off test/development gate.
- Kept selected semantic state as `NodeId`; recycled row pointers are only read at event dispatch time.
- In virtual gated mode, keep selection by semantic `NodeId` across reorder/filter and clear it only when the selected node leaves `NodeStore` through removal/purge.
- Allowed virtual-mode chat, signal scanner, traceroute, and map flows to route from `currentNode`/NodeId even without a retained legacy row.
- Reset test fixture static/UI state that could leak across singleton-backed harness instances.

Tests:
- RED: New LVGL event tests failed before implementation: click left selection at `0`, long press did not open chat, and virtual action routing was absent.
- GREEN: `PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/build-task4-python/lib/python3.14/site-packages build-task4-virtual/bin/tests` passed: 63/63 doctest cases, 1755/1755 assertions.
- GREEN: `PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/build-task4-python/lib/python3.14/site-packages ctest --test-dir build-task4-virtual --output-on-failure` passed: 9/9 CTest tests.

Formatting:
- Scoped `trunk fmt` on the changed task files passed.
- Plain `trunk fmt` still fails because it walks existing untracked build directories and reports prettier syntax errors in generated `compiler_depend.ts` files; no generated build output was staged.

Concerns:
- The fresh `build-task5-red` configure hit missing Python `grpc-tools`; verification used the existing `build-task4-virtual` tree with the local `build-task4-python` site-packages on `PYTHONPATH`.

Review fix round 1:
- Fixed pooled-row encoder/group traversal by giving the virtual list real LVGL group ownership when no default group exists, temporarily disabling group wrap, and translating edge focus movement to the next/previous logical `NodeId` before scroll/rebind/focus.
- Removed the focused-event test helper pre-call to `VirtualNodeList::focus(id)`; focus regressions now use rendered-row LVGL focus and `lv_group_focus_next`/`lv_group_focus_prev`.
- Scoped new selection-retention behavior behind the virtual gate. The retained legacy/default path no longer clears selection during virtual-only synchronization logic.

Review RED evidence:
- `cmake --build build-task4-virtual --target tests -j4` initially failed to compile the new regression because `VirtualNodeList::POOL_SIZE` was not included in `test_MuiNodeListIntegration.cpp`.
- `./build-task4-virtual/bin/tests --test-case="*keeps selection stable*"` failed before the fix: selected `NodeId` became `0` after filtering instead of staying `25`.
- `./build-task4-virtual/bin/tests --test-case="default legacy node list selection is unchanged by virtual selection handling"` failed before the fix: selected `NodeId` became `0` instead of `0x11111111`.
- `./build-task4-virtual/bin/tests --test-case="*group focus traverses*"` failed/hung before group ownership was added: `lv_group_focus_next` had no default group in the headless harness, and edge traversal did not reach the next logical `NodeId`.

Review GREEN evidence:
- `trunk fmt include/graphics/view/TFT/TFTView_320x240.h include/graphics/view/TFT/VirtualNodeList.h source/graphics/TFT/TFTView_320x240.cpp source/graphics/view/TFT/VirtualNodeList.cpp tests/MuiTestHarness.cpp tests/MuiTestHarness.h tests/test_MuiNodeListIntegration.cpp` passed.
- `PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/build-task4-python/lib/python3.14/site-packages cmake --build build-task4-virtual --target tests -j4 && PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/build-task4-python/lib/python3.14/site-packages ctest --test-dir build-task4-virtual --output-on-failure` passed: 9/9 CTest tests.
- `PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/build-task4-python/lib/python3.14/site-packages cmake --build build --target tests -j4 && PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/build-task4-python/lib/python3.14/site-packages ctest --test-dir build -R "^(tests|MuiNodeListIntegration)$" --output-on-failure` passed: 2/2 default legacy code-test targets. Full existing default-build CTest still has unrelated benchmark entries invoking `node_list_bench` without required args.

Review fix round 2:
- Replaced the round-1 shared-default-group hook with a private LVGL group owned by `VirtualNodeList`; pooled virtual rows no longer append to the shared default group and no longer replace its focus/edge callbacks or wrap policy.
- Kept virtual edge traversal in the private group with `wrap=false` and a private edge callback, so next/previous across pooled boundaries translates directly to adjacent logical `NodeId`s without transiently focusing unrelated controls.
- Wired TFT input-device group assignment to use the private virtual group while the virtual node screen is active, and to restore the default group when leaving the node screen.

Review round 2 RED evidence:
- `PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/build-task4-python/lib/python3.14/site-packages cmake --build build-task4-virtual --target tests -j4 && ./build-task4-virtual/bin/tests --test-case="*does not take over the shared default group*"` failed before the fix: default group wrap became `false`, focus/edge callbacks were replaced, and the virtual navigation group equaled the shared default group.

Review round 2 GREEN evidence:
- `./build-task4-virtual/bin/tests --test-case="*does not take over the shared default group*" && ./build-task4-virtual/bin/tests --test-case="*group focus traverses*" && ./build-task4-virtual/bin/tests --test-case="*keeps selection stable*" && ./build-task4-virtual/bin/tests --test-case="default legacy node list selection is unchanged by virtual selection handling"` passed: 4/4 targeted doctest cases.
- `trunk fmt include/graphics/view/TFT/TFTView_320x240.h include/graphics/view/TFT/VirtualNodeList.h source/graphics/TFT/TFTView_320x240.cpp source/graphics/view/TFT/VirtualNodeList.cpp tests/MuiTestHarness.cpp tests/MuiTestHarness.h tests/test_MuiNodeListIntegration.cpp` passed after formatting `source/graphics/TFT/TFTView_320x240.cpp`.
- `PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/build-task4-python/lib/python3.14/site-packages cmake --build build-task4-virtual --target tests -j4 && PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/build-task4-python/lib/python3.14/site-packages ctest --test-dir build-task4-virtual --output-on-failure` passed: 9/9 CTest tests.
- `PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/build-task4-python/lib/python3.14/site-packages cmake --build build --target tests -j4 && PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/build-task4-python/lib/python3.14/site-packages ctest --test-dir build -R "^(tests|MuiNodeListIntegration)$" --output-on-failure` passed: 2/2 default legacy code-test targets.

Review fix round 3:
- Added causal LVGL input-device assignment coverage by configuring headless-test keyboard/encoder/pointer indevs and asserting keyboard/encoder `lv_indev_get_group()` moves to the private virtual group while the virtual nodes screen is active.
- Asserted keyboard/encoder input groups restore to the shared default group after leaving the nodes screen.
- Added the same input-assignment assertions immediately before pooled edge traversal, so the edge regression fails if the production virtual `setInputGroup(virtualNodeList->navigationGroup())` mapping is removed.
- Verified LVGL pointer behavior explicitly: `lv_indev_set_group` only stores groups for keypad/encoder indevs, so the configured pointer remains `nullptr` for `lv_indev_get_group()` in headless LVGL.

Review round 3 RED evidence:
- With the production virtual assignment line temporarily changed from `setInputGroup(virtualNodeList->navigationGroup())` to `setInputGroup()`, `./build-task4-virtual/bin/tests --test-case="*assigns configured input devices*"` failed: keyboard and encoder groups did not equal the private virtual navigation group.
- With the production restore line temporarily removed when leaving the virtual nodes panel, the same test failed: keyboard and encoder groups did not restore to the shared default group.

Review round 3 GREEN evidence:
- `./build-task4-virtual/bin/tests --test-case="*assigns configured input devices*" && ./build-task4-virtual/bin/tests --test-case="*group focus traverses*" && ./build-task4-virtual/bin/tests --test-case="*does not take over the shared default group*" && ./build-task4-virtual/bin/tests --test-case="*keeps selection stable*" && ./build-task4-virtual/bin/tests --test-case="default legacy node list selection is unchanged by virtual selection handling"` passed: 5/5 targeted doctest cases.
- `trunk fmt include/input/InputDriver.h source/input/InputDriver.cpp include/graphics/view/TFT/TFTView_320x240.h include/graphics/view/TFT/VirtualNodeList.h source/graphics/TFT/TFTView_320x240.cpp source/graphics/view/TFT/VirtualNodeList.cpp tests/MuiTestHarness.cpp tests/MuiTestHarness.h tests/test_MuiNodeListIntegration.cpp` passed.
- `PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/build-task4-python/lib/python3.14/site-packages cmake --build build-task4-virtual --target tests -j4 && PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/build-task4-python/lib/python3.14/site-packages ctest --test-dir build-task4-virtual --output-on-failure` passed: 9/9 CTest tests.
- `PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/build-task4-python/lib/python3.14/site-packages cmake --build build --target tests -j4 && PYTHONPATH=/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/build-task4-python/lib/python3.14/site-packages ctest --test-dir build -R "^(tests|MuiNodeListIntegration)$" --output-on-failure` passed: 2/2 default legacy code-test targets.
