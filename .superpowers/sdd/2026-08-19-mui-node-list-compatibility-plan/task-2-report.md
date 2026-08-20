# Task 2 Report: NodeId semantic boundary for legacy behavior

## Summary

Implemented a NodeId/model semantic boundary for legacy 320x240 node-list behavior without enabling or integrating `VirtualNodeList`.

Changes stayed scoped to:

- `include/graphics/view/TFT/TFTView_320x240.h`
- `source/graphics/TFT/TFTView_320x240.cpp`
- `tests/MuiTestHarness.h`
- `tests/MuiTestHarness.cpp`
- `tests/test_MuiNodeSemanticBoundary.cpp`

## What Changed

- Added NodeId helpers on `TFTView_320x240` for:
  - message eligibility
  - channel lookup
  - public-key availability
  - hops lookup
  - display/short name lookup
  - position lookup
  - purge candidate selection
  - hop-limit calculation
  - panel lookup and selection adaptation
- Updated legacy callbacks/action paths so semantic decisions now use the model helpers instead of retained `nodes[id]` panel metadata:
  - node click/long-press selection and direct-message eligibility
  - map node navigation and position navigation
  - signal scanner target display
  - traceroute target/channel/hop-limit handling
  - direct message channel/key/hop-limit handling
  - detector, packet-log, restored/new message naming paths
  - active-chat marking for model purge protection
  - purge candidate selection
- Kept legacy row rendering and row-update presentation code panel-based.
- Did not integrate `VirtualNodeList`, change defaults, alter filters/order, edit standalone UI, or perform hardware/PR/push work.

## Tests Added

Created `tests/test_MuiNodeSemanticBoundary.cpp` with coverage for:

- NodeId semantic helpers reading `NodeStore` fields even after retained legacy panel metadata is deliberately corrupted.
- Missing-node defaults.
- Active direct chat protection in model purge-candidate selection.

Added narrow `MuiTestHarness` accessors and a test-only legacy-panel corruption hook to make stale panel semantic reads observable.

## TDD Evidence

RED:

- Command:
  - `PYTHONPATH=/Users/benjaminfaershtein/.codex/venvs/device-ui-native/lib/python3.14/site-packages cmake --build build-mui-node-list-headless --target tests -j4`
- Expected failure after adding tests/harness hooks:
  - link failed with undefined symbols for `TFTView_320x240::nodeIsMessagableForTesting`, `nodeChannelForTesting`, `nodeHasKeyForTesting`, `nodeHopsForTesting`, `nodeDisplayNameForTesting`, `nodeShortNameForTesting`, `nodePositionForTesting`, `nodePurgeCandidateForTesting`, and `corruptLegacyNodePanelForTesting`.
- Note:
  - The first attempted RED run was blocked by nanopb generator Python path setup, then by a missing test include. Those were fixed before recording the meaningful RED above.

GREEN:

- Focused semantic tests:
  - `./build-mui-node-list-headless/bin/tests --test-case="*semantic*"`
  - 2 passed, 0 failed, 43 skipped, 19 assertions passed.
  - `./build-mui-node-list-headless/bin/tests --test-case="active direct chats protect model purge candidate selection"`
  - 1 passed, 0 failed, 44 skipped, 1 assertion passed.
- Full doctest executable:
  - `./build-mui-node-list-headless/bin/tests`
  - 45 passed, 0 failed, 1544 assertions passed.
- Full CTest:
  - `ctest --test-dir build-mui-node-list-headless --output-on-failure`
  - 9/9 tests passed.

## Formatting

- Scoped formatter passed:
  - `trunk fmt include/graphics/view/TFT/TFTView_320x240.h source/graphics/TFT/TFTView_320x240.cpp tests/MuiTestHarness.h tests/MuiTestHarness.cpp tests/test_MuiNodeSemanticBoundary.cpp`
  - Checked 5 files, no issues.
- Full `trunk fmt` was attempted first and failed because it walked untracked build directories and reported Prettier syntax errors in generated `compiler_depend.ts` files. No untracked build directories were staged.

## Concerns

- The repository's untracked build directories remain untracked. The accidental full `trunk fmt` invocation touched generated files inside those untracked directories before failing; none are part of the commit.
- Existing legacy filter/highlight row code still reads legacy panel state by design, preserving Task 2's "do not alter filters/order/presentation" constraint.

## Review Fix Round 1

Addressed reviewer findings within Task 2 scope only:

- Signal scanner channel lookup no longer reads `currentPanel->user_data`; it uses `nodeChannel(currentNode)` and therefore falls back safely to channel `0` when the selected node is missing from the model.
- Trace-route route-node callbacks now carry the `NodeId` as callback user data. The callback resolves the retained panel only at the final navigation/presentation step and skips navigation when the legacy panel is missing.
- Removed the unused `nodeIdForPanel()` helper.
- Extended headless coverage to exercise selection, legacy callback adaptation, direct-message action behavior, hop-limit calculation, and signal-scanner/traceroute channel use after retained legacy panel corruption/removal.

RED evidence:

- Command:
  - `PYTHONPATH=/Users/benjaminfaershtein/.codex/venvs/device-ui-native/lib/python3.14/site-packages cmake --build build-mui-node-list-headless --target tests -j4 && ./build-mui-node-list-headless/bin/tests --test-case="legacy action callbacks use NodeId model semantics after panel corruption"`
- Expected failures before production fix:
  - `position.channel == 3` failed with `7 == 3`, proving signal-scanner channel selection still depended on corrupted retained panel state.
  - `traceRouteNodeCallbackPayload(...) == 0x1234abcd` failed with a panel pointer value, proving trace-route callback user data still carried a retained panel pointer.
- Note:
  - The initial RED test also exposed a fixture setup issue for hop-limit expectations (`2 == 3` while the test LoRa hop limit was unset). The fixture was corrected with `setLoRaHopLimit(7)` before the final GREEN run.

GREEN evidence after fix:

- Scoped format:
  - `trunk fmt include/graphics/view/TFT/TFTView_320x240.h source/graphics/TFT/TFTView_320x240.cpp tests/MuiTestHarness.h tests/MuiTestHarness.cpp tests/test_MuiNodeSemanticBoundary.cpp`
  - Checked 5 files, no issues.
- Focused regression:
  - `PYTHONPATH=/Users/benjaminfaershtein/.codex/venvs/device-ui-native/lib/python3.14/site-packages cmake --build build-mui-node-list-headless --target tests -j4 && ./build-mui-node-list-headless/bin/tests --test-case="legacy action callbacks use NodeId model semantics after panel corruption"`
  - 1 passed, 0 failed, 45 skipped, 14 assertions passed.
- Full doctest executable:
  - `./build-mui-node-list-headless/bin/tests`
  - 46 passed, 0 failed, 1558 assertions passed.
- Full CTest:
  - `ctest --test-dir build-mui-node-list-headless --output-on-failure`
  - 9/9 tests passed.
