# Task 4 Report: Gated TFT Virtual Node List Integration

## Status

Implemented.

The virtual node list integration is available only when `ENABLE_MUI_VIRTUAL_NODE_LIST=ON`, which is rejected unless `ENABLE_MUI_HEADLESS_TESTS=ON`. Even in gate-enabled builds, the TFT view defaults to the retained legacy node rows; tests must explicitly opt into the runtime virtual host.

## Implementation

- Added `ENABLE_MUI_VIRTUAL_NODE_LIST` with default `OFF`.
- Added a `DEVICE_UI_MUI_VIRTUAL_NODE_LIST` compile definition only for headless test/development builds.
- Added a dedicated virtual host container owned by `TFTView_320x240`; the generated `objects.nodes_panel` remains the legacy retained-row container and production default.
- Added model/presentation sync helpers that rebuild `VisibleNodeIndex` and, when explicitly enabled, sync `VirtualNodeList`.
- Synced the visible index after node insertion, unknown/user updates, filter change, active-chat changes, last-heard reorder, presentation resync, and purge/removal paths.
- Kept Task 2 NodeId semantics and static fixed-label buffers intact.
- Left clickable input disabled at the TFT integration layer by using no-op `NodeListActionSink` callbacks.

## Tests

### RED

Before implementation, the focused integration tests failed to compile because the harness expected Task 4 hooks that did not exist yet:

```text
tests/MuiTestHarness.cpp: error: no member named 'setOfflineFilterForTesting' in 'TFTView_320x240'
tests/MuiTestHarness.cpp: error: no member named 'virtualNodeListEnabledForTesting' in 'TFTView_320x240'
tests/MuiTestHarness.cpp: error: no member named 'legacyRetainedNodeCountForTesting' in 'TFTView_320x240'
```

### GREEN

Default-off build:

```text
PATH=build-task4-python/bin:$PATH cmake --build build-task4-red3 --target tests node_list_bench
PATH=build-task4-python/bin:$PATH ctest --test-dir build-task4-red3 --output-on-failure
100% tests passed, 0 tests failed out of 9
```

Gate-on build:

```text
PATH=build-task4-python/bin:$PATH cmake --build build-task4-virtual --target tests node_list_bench
PATH=build-task4-python/bin:$PATH ctest --test-dir build-task4-virtual --output-on-failure
100% tests passed, 0 tests failed out of 9
```

Focused coverage includes:

- default production path retains 25 legacy rows and reports virtual disabled;
- visible index resync after insertion, user update, filter change, active chat, last-heard reorder, presentation resync, and purge;
- gated virtual path for 25, 100, and 250 nodes with zero legacy retained rows and bounded node-list object count.

## Notes / Concerns

- `trunk fmt` was run, but it also traversed untracked in-tree `build-*` directories and failed on generated `compiler_depend.ts` files. The scoped tracked source/header changes were formatted; generated/untracked build outputs were not staged.
- Local CMake needed a task-local Python venv with `protobuf` and `grpcio-tools` for nanopb generation:
  `build-task4-python/bin/python`.

## Review Fix Round 1

### RED

Added gate-on structural and virtual-render mutation tests before changing the host placement.

Initial RED:

```text
cmake --build build-task4-virtual --target tests
error: no member named 'legacyNodeListRootForTesting' in 'MuiTestHarness'
```

After adding the test hook, the structural test exposed the review failure:

```text
build-task4-virtual/bin/tests "--test-case=*gated virtual node list uses a dedicated bounded host*"
CHECK(lv_obj_get_parent(harness.nodeListRootForTesting()) ==
      lv_obj_get_parent(harness.legacyNodeListRootForTesting())) is NOT correct
```

### GREEN

Moved the runtime virtual host from a child of `objects.nodes_panel` to a sibling under the legacy panel's parent. The generated legacy panel root remains separate and its retained-row behavior stays default-off.

Focused gate-on tests:

```text
build-task4-virtual/bin/tests "--test-case=*gated virtual node list uses a dedicated bounded host*,*gated virtual node list renders mutation resyncs*"
test cases: 2 | 2 passed | 0 failed
assertions: 49 | 49 passed | 0 failed
```

Default-off full suite:

```text
PATH=build-task4-python/bin:$PATH cmake --build build-task4-red3 --target tests node_list_bench
PATH=build-task4-python/bin:$PATH ctest --test-dir build-task4-red3 --output-on-failure
100% tests passed, 0 tests failed out of 9
```

Gate-on full suite:

```text
PATH=build-task4-python/bin:$PATH cmake --build build-task4-virtual --target tests node_list_bench
PATH=build-task4-python/bin:$PATH ctest --test-dir build-task4-virtual --output-on-failure
100% tests passed, 0 tests failed out of 9
```

### Added coverage

- Structural assertion: virtual host is not `objects.nodes_panel` and is a sibling under the same parent.
- Gate-on virtual rendered/rebound assertions for:
  - user update row text;
  - offline filter row rebinding;
  - active-chat model mutation through purge protection and subsequent virtual rebinding;
  - last-heard reorder of the first rendered row;
  - presentation resync preserving and re-rendering the virtual host;
  - purge removing the oldest node and rebinding visible rows.
