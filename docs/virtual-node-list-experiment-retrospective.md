# Virtual Node-List Experiment Retrospective

**Status:** stopped; do not use as a release candidate.  The implementation
worktree remains on `candidate/virtual-node-list-improvement-20260824`, based
on `aeef0a14f4a116f10523299569a81858cff8445b`, with its full uncommitted state
intentionally preserved for reference.

## Goal and approaches tried

- Replace the TFT node-list panel path with a pooled/virtual row list.
- Preserve legacy presentation behavior while reducing visible LVGL objects and
  update cost.
- Add host/headless and X11 simulator coverage, benchmark helpers, large-node
  cases, and parity tests.
- Subsequent fixes deferred row ordering/group membership until scroll-end,
  removed duplicate scroll callback lifetime risk, and reduced repeated touch
  PowerFSM notifications.

## Evidence retained

- Focused headless suites were green after the last changes:
  `MuiNodeListIntegration`, `NodeStore`, `VisibleNodeIndex`, and
  `VirtualNodeList` (4/4 CTest tests).
- The T-Deck target built and flashed successfully.  The boot log showed no
  panic or watchdog before the observed screen-dark event.
- The device processed a large post-boot/replay burst: approximately 181 known
  nodes and hundreds of `DeviceUI handleFromRadio` updates by the first minute.
  This is the hardware workload that any performance benchmark must include.
- The recorded screen-dark transition was normal power saving:
  `enter powersave`, `disable touch, enable button input`, then PowerFSM armed
  GPIO45 wake.  It is not evidence of a scroll crash, but wake behavior still
  needs an independent device test.

## What worked

- The simulator, headless tests, node-store tests, and target build made it
  possible to detect several correctness problems before flashing.
- Small data-layer fixes, including sorted visible-membership lookup and channel
  sentinel handling, are independently useful and have focused tests.
- The real-device capture demonstrated that replay/update pressure is a more
  representative performance case than an idle list.

## What failed or remains unproven

- The virtual list did not demonstrate a repeatable real T-Deck improvement
  over legacy.  Reports instead showed worse scrolling and freezes/stalls.
- Host simulator results are not enough to establish ESP32-S3 responsiveness.
- No captured panic, watchdog, or heap-exhaustion log proves a single root
  cause for the freeze reports.
- The broad consolidation and benchmark/test restructuring increased scope far
  beyond a safe performance experiment.

## Do not repeat

- Do not continue to fix the virtual path forward for a release.
- Do not treat an idle simulator scroll benchmark as representative hardware
  evidence; replay/node-discovery churn and the production logging level matter.
- Do not combine architecture replacement, parity work, test-harness rewrites,
  generated UI changes, and performance tuning in one candidate.
- Do not enable LVGL multithreaded rendering, direct/double buffering, or a
  hardware draw backend on T-Deck without a separate measured hardware design.

## Recommended restart path

1. Keep this worktree as the experiment archive; make a clean legacy baseline.
2. Run the existing simulator against identical legacy and candidate workloads:
   idle scroll and replay/node-insert scroll at the same node count.
3. Validate the legacy T-Deck behavior with the same logging configuration.
4. If the profile identifies one redundant legacy invalidation, layout refresh,
   or row bind, make only that shared-path change and one focused regression
   test.  Reject changes without a measurable benefit on both simulator and
   device.

## Relevant artifacts

- `source/graphics/view/TFT/VirtualNodeList.cpp` and
  `include/graphics/view/TFT/VirtualNodeList.h`: archived virtual experiment.
- `tests/test_VirtualNodeList.cpp`, `tests/test_MuiNodeListIntegration.cpp`,
  `tests/test_NodeListBenchmark.cpp`: experiment coverage and benchmark inputs.
- `tools/node_list_bench.cpp`, `tools/mui_node_list_simulator.cpp`: existing
  comparison tools; reuse them rather than adding another harness.
- `include/lv_conf.h`, `include/graphics/driver/LGFXDriver.h`: LVGL/T-Deck
  configuration.  T-Deck currently uses RGB565 partial rendering with a
  153600-byte PSRAM buffer and one software draw unit.

