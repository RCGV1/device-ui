# Task 3 Report: Model and visual parity for virtual node-list candidate

## Summary

Specified and implemented legacy-compatible model/filter policy and virtual row presentation parity without integrating `VirtualNodeList` into normal TFT rendering.

Changes stayed scoped to:

- `include/graphics/common/VisibleNodeIndex.h`
- `source/graphics/common/VisibleNodeIndex.cpp`
- `include/graphics/view/TFT/VirtualNodeList.h`
- `source/graphics/view/TFT/VirtualNodeList.cpp`
- `tests/test_VisibleNodeIndex.cpp`
- `tests/test_VirtualNodeList.cpp`
- `tools/node_list_bench.cpp`
- `tools/node_list_video.cpp`

## What Changed

- Added explicit `NodeListFilterPolicy::LegacyCompatible` to `VisibleNodeIndex::rebuild()` and `isVisible()`.
- Preserved legacy filter semantics:
  - MQTT filter remains disabled.
  - Name search checks displayed long/short labels only.
  - Unknown nodes still use the existing fallback `Meshtastic xxxx` / `xxxx` display names.
  - Full hex ID and decimal ID strings do not match unless they are in displayed labels.
  - Existing last-heard order and own-node inclusion behavior remain unchanged.
- Extended `VirtualNodeList` reusable rows to render expanded legacy detail fields from `NodeRecord`:
  - role and unmessagable icons
  - short and long names
  - battery/voltage
  - last-heard age
  - hops/signal line
  - position latitude/longitude
  - altitude line
  - environment telemetry
  - IAQ telemetry
- Kept all expanded row label text in fixed `ReusableRow` buffers and continued using `lv_label_set_text_static()`.
- Did not make `VirtualNodeList` clickable in normal TFT, did not integrate it into `TFTView_320x240`, and did not alter legacy row presentation code.

## Tests Added

- `tests/test_VisibleNodeIndex.cpp`
  - MQTT filter remains disabled under the legacy-compatible policy.
  - Unknown displayed short hex still matches.
  - Full hex ID and decimal ID searches do not match non-displayed IDs.
  - All existing filter/order tests now pass the explicit legacy-compatible policy.
- `tests/test_VirtualNodeList.cpp`
  - 25, 100, and 250 node structural object counts remain identical.
  - Expanded virtual rows render all specified legacy detail labels without adding row objects.
  - Router and unmessagable role icons come from the model record.

## TDD Evidence

RED:

- Command:
  - `PATH="/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/.codex/task3-venv/bin:$PATH" cmake --build build-mui-node-list-headless --target tests -j 8 && ./build-mui-node-list-headless/bin/tests "--test-case=*visible index*"`
- Expected failures after adding the index parity tests:
  - MQTT filter expected all legacy-visible nodes but got only own node plus MQTT node.
  - Full hex ID search `1234abcd` matched a node when legacy displayed-label search should not.
- Command:
  - `PATH="/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/.codex/task3-venv/bin:$PATH" cmake --build build-mui-node-list-headless --target tests -j 8 && ./build-mui-node-list-headless/bin/tests "--test-case=*VirtualNodeList*"`
- Expected failures after adding virtual parity tests:
  - Expanded row detail labels for position line 2, environment telemetry, and IAQ were missing/hidden.
  - Router and unmessagable icon sources were still the default client image.

GREEN:

- Focused visible index:
  - `PATH="/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/.codex/task3-venv/bin:$PATH" cmake --build build-mui-node-list-headless --target tests -j 8 && ./build-mui-node-list-headless/bin/tests "--test-case=*visible index*"`
  - 2 test cases passed, 0 failed, 33 assertions passed.
- Focused virtual list:
  - `PATH="/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/.codex/task3-venv/bin:$PATH" cmake --build build-mui-node-list-headless --target tests -j 8 && ./build-mui-node-list-headless/bin/tests "--test-case=*VirtualNodeList*"`
  - 7 test cases passed, 0 failed, 48 assertions passed.
- Full CTest:
  - `PATH="/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/.codex/task3-venv/bin:$PATH" ctest --test-dir build-mui-node-list-headless --output-on-failure`
  - 9/9 tests passed.
  - Included `VirtualNodeListAllocator25`, `VirtualNodeListAllocator100`, and `VirtualNodeListAllocator250`.

## Formatting

- Scoped formatter passed:
  - `trunk fmt include/graphics/common/VisibleNodeIndex.h source/graphics/common/VisibleNodeIndex.cpp include/graphics/view/TFT/VirtualNodeList.h source/graphics/view/TFT/VirtualNodeList.cpp tests/test_VisibleNodeIndex.cpp tests/test_VirtualNodeList.cpp tools/node_list_bench.cpp tools/node_list_video.cpp`
  - Checked 8 files, no issues.

## Environment Notes

- The existing headless build initially could not regenerate nanopb outputs because `/usr/bin/env python3` could not import `google.protobuf`.
- Created an untracked local venv at `.codex/task3-venv` and used it only by prepending it to `PATH` for build/test commands.
- Existing compiler warnings in legacy TFT code remain unrelated to this task.

## Concerns

- `NodeListFilterPolicy` currently has one policy value, `LegacyCompatible`, because Task 3 only needed the compatibility contract made explicit.
- The virtual row now mirrors legacy metric strings for the metric/default display path; non-metric unit parity can be covered when virtual rows are promoted beyond candidate/test tooling.

## Review Fix Round 1

Addressed the Important review findings with event-driven legacy-vs-virtual parity tests.

### Changes

- Added `NodeSignalDisplayKind` to `NodeRecord` so `NodeStore::updateSignal()` and `NodeStore::updateHops()` preserve legacy event order: whichever event arrived last controls the signal label.
- Added `NodeListRenderContext` to virtual sync so tests/tools can provide own-node position and display units without wiring virtual rows into normal `TFTView`.
- Updated virtual row binding to match legacy-observable row text/style:
  - short-name fallback uses legacy `lv_txt_get_width(...) <= 4` then `%04x`;
  - distance appends to the short-name label and moves the short-name Y position to `-1`;
  - RSSI label includes SNR and only wins when the RSSI event was last;
  - role/unmessagable/public-key icon background, border, recolor, and recolor opacity follow the legacy color calculation.
- Added test harness row snapshots that read legacy labels and image styles from actual LVGL row objects after production `TFTView_320x240` node mutation/update methods run.
- Added virtual snapshots by rebuilding `VisibleNodeIndex` from the same `NodeStore` produced by the legacy event flow, then comparing observable row strings and icon styles.

### RED Evidence

- Command:
  - `PATH="/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/.codex/task3-venv/bin:$PATH" cmake --build build-mui-node-list-headless --target tests -j 8 && ./build-mui-node-list-headless/bin/tests "--test-case=*VirtualNodeList*"`
- Expected compile RED after adding review-driven tests:
  - `tests/test_VirtualNodeList.cpp:79:5: error: unknown type name 'NodeListRenderContext'`
  - role enum use in the new tests also required including `graphics/common/MeshtasticView.h`.
- After adding the production render context, the same focused command reached runtime REDs:
  - first event-driven distance test crashed due to the test helper deleting the LVGL parent before `VirtualNodeList` destroyed its pooled rows;
  - last-heard parity showed `2 min` vs legacy blank because parity helper used injected current time while legacy `lastHeardToString()` reads wall clock;
  - signal-order helper returned no virtual row when two harnesses were alive in one test scope;
  - unmessagable border compared as `0xff202020` vs legacy `0xff537272`, exposing a border override after the unmessagable style path.

### GREEN Evidence

- Focused virtual list:
  - `PATH="/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/.codex/task3-venv/bin:$PATH" cmake --build build-mui-node-list-headless --target tests -j 8 && ./build-mui-node-list-headless/bin/tests "--test-case=*VirtualNodeList*"`
  - 10 test cases passed, 0 failed, 74 assertions passed.
- Full CTest:
  - `PATH="/Users/benjaminfaershtein/Documents/device-ui-mui-node-list/.codex/task3-venv/bin:$PATH" ctest --test-dir build-mui-node-list-headless --output-on-failure`
  - 9/9 tests passed.
  - Included `VirtualNodeListAllocator25`, `VirtualNodeListAllocator100`, and `VirtualNodeListAllocator250`.
- Formatting/diff hygiene:
  - `trunk fmt include/graphics/common/NodeStore.h source/graphics/common/NodeStore.cpp include/graphics/view/TFT/VirtualNodeList.h source/graphics/view/TFT/VirtualNodeList.cpp tests/MuiTestHarness.h tests/MuiTestHarness.cpp tests/test_VirtualNodeList.cpp`
  - Checked 7 files, no issues.
  - `git diff --check` passed.

### Scope Notes

- `VirtualNodeList` remains test/benchmark/video-only; no normal `TFTView` integration or clickable default behavior was added.
- Static row-owned buffers remain in `ReusableRow`; the short-name buffer was widened to hold the legacy distance suffix without per-bind allocation.
- Untracked build directories and `.codex/task3-venv` were preserved and not staged.
