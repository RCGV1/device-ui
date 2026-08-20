### Task 5 Report: Real interaction parity on recycled rows

Status: complete

Implementation:
- Added real virtual-row LVGL event routing for click, long press, focus, and expanded position-label clicks.
- Wired `TFTView_320x240` as the virtual `NodeListActionSink` using NodeId model helpers instead of retained row pointers.
- Preserved the default legacy path: virtual list behavior remains behind the existing default-off test/development gate.
- Kept selected semantic state as `NodeId`; recycled row pointers are only read at event dispatch time.
- Cleared selection when the selected node disappears from the visible model through filter or purge.
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
