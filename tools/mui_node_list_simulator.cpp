#define DOCTEST_CONFIG_IMPLEMENT
#include "X11MuiSimulator.h"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <thread>
// clang-format off
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
// clang-format on

namespace
{
int x11ErrorCount = 0;

struct Options {
    X11MuiSimulator::Implementation implementation = X11MuiSimulator::Implementation::Legacy;
    size_t nodes = 100;
    uint32_t seed = 42;
    uint32_t runForMs = 0;
    bool exerciseX11Input = false;
    bool hardwareBenchmark = false;
    bool tdeckConstrained = false;
    std::string reportPath;
    std::string windowTitle;
};

constexpr std::string_view usage =
    "usage: mui_node_list_simulator --implementation legacy|virtual_candidate [--nodes N] [--seed N] [--run-for-ms N] "
    "[--window-title TITLE] [--exercise-x11-input] [--hardware-benchmark] [--tdeck-constrained] [--report PATH]";

constexpr uint32_t tdeckModelDisplayWidth = 320;
constexpr uint32_t tdeckModelDisplayHeight = 240;
constexpr uint32_t tdeckModelUiPeriodMs = 40;
constexpr uint32_t tdeckModelSpiHz = 40000000;
constexpr uint32_t tdeckModelRgb565FrameBytes = tdeckModelDisplayWidth * tdeckModelDisplayHeight * 2;
constexpr std::string_view tdeckModelFrameTransferMs = "30.72";

const char *implementationName(X11MuiSimulator::Implementation implementation)
{
    return implementation == X11MuiSimulator::Implementation::VirtualCandidate ? "virtual_candidate" : "legacy";
}

bool displayAvailable()
{
    const char *display = std::getenv("DISPLAY");
    return display && display[0] != '\0';
}

bool parseUnsigned(std::string_view value, uint64_t &parsed)
{
    auto begin = value.data();
    auto end = value.data() + value.size();
    auto result = std::from_chars(begin, end, parsed);
    return result.ec == std::errc{} && result.ptr == end;
}

bool parseOptions(int argc, char **argv, Options &options)
{
    bool sawImplementation = false;
    for (int index = 1; index < argc;) {
        const std::string_view flag(argv[index]);
        uint64_t parsed = 0;
        if (flag == "--exercise-x11-input") {
            options.exerciseX11Input = true;
            ++index;
            continue;
        }
        if (flag == "--hardware-benchmark") {
            options.hardwareBenchmark = true;
            ++index;
            continue;
        }
        if (flag == "--tdeck-constrained") {
            options.tdeckConstrained = true;
            ++index;
            continue;
        }
        if (index + 1 >= argc) {
            return false;
        }

        const std::string_view value(argv[index + 1]);
        if (flag == "--implementation" && !sawImplementation) {
            sawImplementation = true;
            if (value == "legacy") {
                options.implementation = X11MuiSimulator::Implementation::Legacy;
            } else if (value == "virtual_candidate") {
#ifdef DEVICE_UI_MUI_VIRTUAL_NODE_LIST
                options.implementation = X11MuiSimulator::Implementation::VirtualCandidate;
#else
                std::cerr << "virtual_candidate requires ENABLE_MUI_VIRTUAL_NODE_LIST\n";
                return false;
#endif
            } else {
                return false;
            }
        } else if (flag == "--nodes" && parseUnsigned(value, parsed) && parsed > 0 &&
                   parsed <= static_cast<uint64_t>(std::numeric_limits<size_t>::max())) {
            options.nodes = static_cast<size_t>(parsed);
        } else if (flag == "--seed" && parseUnsigned(value, parsed) &&
                   parsed <= static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
            options.seed = static_cast<uint32_t>(parsed);
        } else if (flag == "--run-for-ms" && parseUnsigned(value, parsed) &&
                   parsed <= static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
            options.runForMs = static_cast<uint32_t>(parsed);
        } else if (flag == "--report") {
            options.reportPath = std::string(value);
        } else if (flag == "--window-title") {
            options.windowTitle = std::string(value);
        } else {
            return false;
        }
        index += 2;
    }
    return sawImplementation;
}

bool findWindowByTitle(Display *display, Window root, const std::string &title, Window &found)
{
    char *name = nullptr;
    if (XFetchName(display, root, &name) > 0 && name) {
        const bool matches = title == name;
        XFree(name);
        if (matches) {
            found = root;
            return true;
        }
    }

    Window parent = 0;
    Window *children = nullptr;
    Window unusedRoot = 0;
    unsigned int childCount = 0;
    if (XQueryTree(display, root, &unusedRoot, &parent, &children, &childCount) == 0) {
        return false;
    }

    for (unsigned int i = 0; i < childCount; ++i) {
        if (findWindowByTitle(display, children[i], title, found)) {
            XFree(children);
            return true;
        }
    }
    if (children) {
        XFree(children);
    }
    return false;
}

Window waitForWindow(Display *display, const std::string &title)
{
    for (int attempt = 0; attempt < 50; ++attempt) {
        Window window = 0;
        if (findWindowByTitle(display, DefaultRootWindow(display), title, window)) {
            return window;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return 0;
}

void pumpFor(X11MuiSimulator &simulator, uint32_t ms)
{
    for (uint32_t elapsed = 0; elapsed < ms; elapsed += 10) {
        simulator.pump(10);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

bool envEnabled(const char *name)
{
    const char *value = std::getenv(name);
    return value && value[0] != '\0' && value[0] != '0';
}

bool settleNodeListScroll(X11MuiSimulator &simulator, int32_t &before, int32_t &after)
{
    simulator.stopNodeListScrollForTesting();
    pumpFor(simulator, 80);
    before = simulator.nodeListScrollYForTesting();
    pumpFor(simulator, 160);
    after = simulator.nodeListScrollYForTesting();
    return before == after;
}

struct X11ExerciseReport {
    bool openedDisplay = false;
    bool foundWindow = false;
    bool dragSent = false;
    bool wheelSent = false;
    bool clickSent = false;
    bool keySent = false;
    bool dragXTestOk = false;
    bool wheelXTestOk = false;
    bool clickXTestOk = false;
    bool keyXTestOk = false;
    std::string wheelInput;
    std::string keyInput;
    bool dragMomentumDisabled = false;
    int32_t scrollBefore = 0;
    int32_t dragScrollBefore = 0;
    int32_t dragScrollAfter = 0;
    int32_t wheelSettleBefore = 0;
    int32_t wheelSettleAfter = 0;
    bool wheelScrollStableBefore = false;
    int32_t wheelScrollBefore = 0;
    int32_t wheelScrollAfter = 0;
    uint32_t wheelSelectedBefore = 0;
    uint32_t wheelSelectedAfter = 0;
    uintptr_t wheelFocusBefore = 0;
    uintptr_t wheelFocusAfter = 0;
    int32_t scrollAfterAll = 0;
    uint32_t selectedBefore = 0;
    uint32_t clickSelectedBefore = 0;
    uint32_t clickSelectedAfter = 0;
    uint32_t selectedAfterAll = 0;
    uintptr_t clickFocusBefore = 0;
    uintptr_t clickFocusAfter = 0;
    uintptr_t clickTarget = 0;
    int16_t clickX = 0;
    int16_t clickY = 0;
    int32_t keyScrollBefore = 0;
    int32_t keyScrollAfter = 0;
    uint32_t keySelectedBefore = 0;
    uint32_t keySelectedAfter = 0;
    uintptr_t keyFocusBefore = 0;
    uintptr_t keyFocusAfter = 0;
};

int captureX11Error(Display *, XErrorEvent *)
{
    ++x11ErrorCount;
    return 0;
}

bool xTestSucceeded(Display *display, bool callStatus, int errorCountBefore)
{
    XSync(display, False);
    return callStatus && x11ErrorCount == errorCountBefore;
}

class ScopedDirectoryLock
{
  public:
    explicit ScopedDirectoryLock(std::filesystem::path lockPath) : path(std::move(lockPath)), locked(false)
    {
        if (path.empty()) {
            return;
        }
        for (int attempt = 0; attempt < 200; ++attempt) {
            std::error_code error;
            if (std::filesystem::create_directory(path, error)) {
                locked = true;
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    ~ScopedDirectoryLock()
    {
        if (locked) {
            std::error_code error;
            std::filesystem::remove(path, error);
        }
    }

    bool acquired() const { return path.empty() || locked; }

  private:
    std::filesystem::path path;
    bool locked;
};

X11ExerciseReport exerciseX11Input(X11MuiSimulator &simulator, const std::string &windowTitle)
{
    X11ExerciseReport report{};
    report.scrollBefore = simulator.nodeListScrollYForTesting();
    report.selectedBefore = simulator.selectedNodeForTesting();

    Display *display = XOpenDisplay(nullptr);
    if (!display) {
        return report;
    }
    report.openedDisplay = true;

    const Window window = waitForWindow(display, windowTitle);
    if (window == 0) {
        XCloseDisplay(display);
        return report;
    }
    report.foundWindow = true;

    const char *lockPath = std::getenv("DEVICE_UI_X11_INPUT_LOCK");
    ScopedDirectoryLock inputLock(lockPath ? std::filesystem::path(lockPath) : std::filesystem::path());
    if (!inputLock.acquired()) {
        XCloseDisplay(display);
        return report;
    }

    XErrorHandler previousErrorHandler = XSetErrorHandler(captureX11Error);
    const bool restoreScrollMomentum = simulator.disableNodeListScrollMomentumForTesting();
    report.dragMomentumDisabled = true;

    XRaiseWindow(display, window);
    XSetInputFocus(display, window, RevertToParent, CurrentTime);
    XWarpPointer(display, None, window, 0, 0, 0, 0, 160, 210);
    XFlush(display);
    pumpFor(simulator, 40);

    report.dragScrollBefore = simulator.nodeListScrollYForTesting();
    int errorCountBefore = x11ErrorCount;
    bool dragStatus = XTestFakeButtonEvent(display, Button1, True, CurrentTime);
    for (int y : {180, 140, 100, 60, 40}) {
        dragStatus = XTestFakeMotionEvent(display, DefaultScreen(display), 160, y, CurrentTime) && dragStatus;
        XFlush(display);
        pumpFor(simulator, 30);
    }
    dragStatus = XTestFakeButtonEvent(display, Button1, False, CurrentTime) && dragStatus;
    report.dragXTestOk = xTestSucceeded(display, dragStatus, errorCountBefore);
    report.dragSent = true;
    pumpFor(simulator, 120);
    report.dragScrollAfter = simulator.nodeListScrollYForTesting();

    report.clickSelectedBefore = simulator.selectedNodeForTesting();
    report.clickFocusBefore = simulator.focusedObjectForTesting();
    int16_t clickX = 160;
    int16_t clickY = 82;
    uintptr_t clickTarget = 0;
    if (simulator.nodeListClickTargetForTesting(clickX, clickY, clickTarget)) {
        clickX = std::max<int16_t>(5, std::min<int16_t>(314, clickX));
        clickY = std::max<int16_t>(5, std::min<int16_t>(234, clickY));
    }
    report.clickTarget = clickTarget;
    report.clickX = clickX;
    report.clickY = clickY;
    errorCountBefore = x11ErrorCount;
    bool clickStatus = XTestFakeMotionEvent(display, DefaultScreen(display), clickX, clickY, CurrentTime);
    XWarpPointer(display, None, window, 0, 0, 0, 0, clickX, clickY);
    XFlush(display);
    pumpFor(simulator, 40);
    clickStatus = XTestFakeButtonEvent(display, Button1, True, CurrentTime) && clickStatus;
    XFlush(display);
    pumpFor(simulator, 80);
    clickStatus = XTestFakeButtonEvent(display, Button1, False, CurrentTime) && clickStatus;
    report.clickXTestOk = xTestSucceeded(display, clickStatus, errorCountBefore);
    report.clickSent = true;
    pumpFor(simulator, 320);
    report.clickSelectedAfter = simulator.selectedNodeForTesting();
    report.clickFocusAfter = simulator.focusedObjectForTesting();

    report.wheelScrollStableBefore = settleNodeListScroll(simulator, report.wheelSettleBefore, report.wheelSettleAfter);
    report.wheelScrollBefore = report.wheelSettleAfter;
    report.wheelSelectedBefore = simulator.selectedNodeForTesting();
    report.wheelFocusBefore = simulator.focusedObjectForTesting();
    report.wheelInput = "xtest_button5_mouse_wheel_encoder";
    XTestFakeMotionEvent(display, DefaultScreen(display), clickX, clickY, CurrentTime);
    XWarpPointer(display, None, window, 0, 0, 0, 0, clickX, clickY);
    XFlush(display);
    pumpFor(simulator, 40);
    errorCountBefore = x11ErrorCount;
    bool wheelStatus = !envEnabled("DEVICE_UI_X11_SUPPRESS_WHEEL_XTEST");
    if (wheelStatus) {
        for (int i = 0; i < 4; ++i) {
            wheelStatus = XTestFakeButtonEvent(display, Button5, True, CurrentTime) && wheelStatus;
            wheelStatus = XTestFakeButtonEvent(display, Button5, False, CurrentTime) && wheelStatus;
        }
    }
    report.wheelXTestOk = xTestSucceeded(display, wheelStatus, errorCountBefore);
    report.wheelSent = true;
    pumpFor(simulator, 350);
    report.wheelScrollAfter = simulator.nodeListScrollYForTesting();
    report.wheelSelectedAfter = simulator.selectedNodeForTesting();
    report.wheelFocusAfter = simulator.focusedObjectForTesting();

    report.keyScrollBefore = simulator.nodeListScrollYForTesting();
    report.keySelectedBefore = simulator.selectedNodeForTesting();
    report.keyFocusBefore = simulator.focusedObjectForTesting();
    report.keyInput = "xtest_page_down_key";
    const KeyCode key = XKeysymToKeycode(display, XK_Next);
    if (key != 0) {
        errorCountBefore = x11ErrorCount;
        bool keyStatus = XTestFakeKeyEvent(display, key, True, CurrentTime);
        keyStatus = XTestFakeKeyEvent(display, key, False, CurrentTime) && keyStatus;
        report.keyXTestOk = xTestSucceeded(display, keyStatus, errorCountBefore);
        report.keySent = true;
        pumpFor(simulator, 120);
    }
    report.keyScrollAfter = simulator.nodeListScrollYForTesting();
    report.keySelectedAfter = simulator.selectedNodeForTesting();
    report.keyFocusAfter = simulator.focusedObjectForTesting();

    report.scrollAfterAll = simulator.nodeListScrollYForTesting();
    report.selectedAfterAll = simulator.selectedNodeForTesting();
    simulator.restoreNodeListScrollMomentumForTesting(restoreScrollMomentum);
    XSetErrorHandler(previousErrorHandler);
    XCloseDisplay(display);
    return report;
}

bool writeReport(const Options &options, const X11MuiSimulator &simulator, const X11ExerciseReport &exercise,
                 const std::string &hardwareBenchmarkReport, bool hardwareBenchmarkComplete)
{
    if (options.reportPath.empty()) {
        return true;
    }
    std::ofstream output(options.reportPath);
    if (!output) {
        return false;
    }
    output << "implementation=" << implementationName(options.implementation) << '\n';
    output << "virtual_enabled=" << (simulator.virtualNodeListEnabledForTesting() ? 1 : 0) << '\n';
    output << "rendered_nodes=" << simulator.renderedNodeCountForTesting() << '\n';
    output << "tdeck_constrained=" << (options.tdeckConstrained ? 1 : 0) << '\n';
    if (options.tdeckConstrained) {
        output << "scope=tdeck-model constrained X11/LVGL simulator; not hardware timing\n";
        output << "tdeck_model_display_width=" << tdeckModelDisplayWidth << '\n';
        output << "tdeck_model_display_height=" << tdeckModelDisplayHeight << '\n';
        output << "tdeck_model_ui_period_ms=" << tdeckModelUiPeriodMs << '\n';
        output << "tdeck_model_spi_hz=" << tdeckModelSpiHz << '\n';
        output << "tdeck_model_rgb565_frame_bytes=" << tdeckModelRgb565FrameBytes << '\n';
        output << "tdeck_model_frame_transfer_ms=" << tdeckModelFrameTransferMs << '\n';
    }
    if (options.hardwareBenchmark) {
        output << "hardware_benchmark_fixtures=250\n";
        output << "hardware_benchmark_complete=" << (hardwareBenchmarkComplete ? 1 : 0) << '\n';
        output << "hardware_benchmark_report=" << hardwareBenchmarkReport << '\n';
    }
    output << "x11_display_opened=" << (exercise.openedDisplay ? 1 : 0) << '\n';
    output << "x11_window_found=" << (exercise.foundWindow ? 1 : 0) << '\n';
    output << "drag_sent=" << (exercise.dragSent ? 1 : 0) << '\n';
    output << "wheel_sent=" << (exercise.wheelSent ? 1 : 0) << '\n';
    output << "click_sent=" << (exercise.clickSent ? 1 : 0) << '\n';
    output << "key_sent=" << (exercise.keySent ? 1 : 0) << '\n';
    output << "drag_xtest_ok=" << (exercise.dragXTestOk ? 1 : 0) << '\n';
    output << "wheel_xtest_ok=" << (exercise.wheelXTestOk ? 1 : 0) << '\n';
    output << "click_xtest_ok=" << (exercise.clickXTestOk ? 1 : 0) << '\n';
    output << "key_xtest_ok=" << (exercise.keyXTestOk ? 1 : 0) << '\n';
    output << "wheel_input=" << exercise.wheelInput << '\n';
    output << "key_input=" << exercise.keyInput << '\n';
    output << "drag_momentum_disabled=" << (exercise.dragMomentumDisabled ? 1 : 0) << '\n';
    output << "scroll_before=" << exercise.scrollBefore << '\n';
    output << "scroll_after=" << exercise.scrollAfterAll << '\n';
    output << "scroll_changed=" << (exercise.scrollBefore != exercise.scrollAfterAll ? 1 : 0) << '\n';
    output << "drag_scroll_before=" << exercise.dragScrollBefore << '\n';
    output << "drag_scroll_after=" << exercise.dragScrollAfter << '\n';
    output << "drag_scroll_changed=" << (exercise.dragScrollBefore != exercise.dragScrollAfter ? 1 : 0) << '\n';
    output << "wheel_settle_before=" << exercise.wheelSettleBefore << '\n';
    output << "wheel_settle_after=" << exercise.wheelSettleAfter << '\n';
    output << "wheel_scroll_stable_before=" << (exercise.wheelScrollStableBefore ? 1 : 0) << '\n';
    output << "wheel_scroll_before=" << exercise.wheelScrollBefore << '\n';
    output << "wheel_scroll_after=" << exercise.wheelScrollAfter << '\n';
    output << "wheel_scroll_changed=" << (exercise.wheelScrollBefore != exercise.wheelScrollAfter ? 1 : 0) << '\n';
    output << "wheel_selected_before=" << exercise.wheelSelectedBefore << '\n';
    output << "wheel_selected_after=" << exercise.wheelSelectedAfter << '\n';
    output << "wheel_selected_changed=" << (exercise.wheelSelectedBefore != exercise.wheelSelectedAfter ? 1 : 0) << '\n';
    output << "wheel_focus_before=" << exercise.wheelFocusBefore << '\n';
    output << "wheel_focus_after=" << exercise.wheelFocusAfter << '\n';
    output << "wheel_focus_changed=" << (exercise.wheelFocusBefore != exercise.wheelFocusAfter ? 1 : 0) << '\n';
    output << "wheel_observable_changed="
           << (exercise.wheelScrollBefore != exercise.wheelScrollAfter ||
                       exercise.wheelSelectedBefore != exercise.wheelSelectedAfter ||
                       exercise.wheelFocusBefore != exercise.wheelFocusAfter
                   ? 1
                   : 0)
           << '\n';
    output << "selected_before=" << exercise.selectedBefore << '\n';
    output << "selected_after=" << exercise.selectedAfterAll << '\n';
    output << "click_selected_before=" << exercise.clickSelectedBefore << '\n';
    output << "click_selected_after=" << exercise.clickSelectedAfter << '\n';
    output << "click_selected_changed=" << (exercise.clickSelectedBefore != exercise.clickSelectedAfter ? 1 : 0) << '\n';
    output << "click_focus_before=" << exercise.clickFocusBefore << '\n';
    output << "click_focus_after=" << exercise.clickFocusAfter << '\n';
    output << "click_focus_changed=" << (exercise.clickFocusBefore != exercise.clickFocusAfter ? 1 : 0) << '\n';
    output << "click_target=" << exercise.clickTarget << '\n';
    output << "click_x=" << exercise.clickX << '\n';
    output << "click_y=" << exercise.clickY << '\n';
    output << "click_observable_changed="
           << (exercise.clickSelectedBefore != exercise.clickSelectedAfter ||
                       exercise.clickFocusBefore != exercise.clickFocusAfter
                   ? 1
                   : 0)
           << '\n';
    output << "key_focus_before=" << exercise.keyFocusBefore << '\n';
    output << "key_focus_after=" << exercise.keyFocusAfter << '\n';
    output << "key_focus_changed=" << (exercise.keyFocusBefore != exercise.keyFocusAfter ? 1 : 0) << '\n';
    output << "key_selected_before=" << exercise.keySelectedBefore << '\n';
    output << "key_selected_after=" << exercise.keySelectedAfter << '\n';
    output << "key_selected_changed=" << (exercise.keySelectedBefore != exercise.keySelectedAfter ? 1 : 0) << '\n';
    output << "key_scroll_before=" << exercise.keyScrollBefore << '\n';
    output << "key_scroll_after=" << exercise.keyScrollAfter << '\n';
    output << "key_scroll_changed=" << (exercise.keyScrollBefore != exercise.keyScrollAfter ? 1 : 0) << '\n';
    output << "key_observable_changed="
           << (exercise.keyFocusBefore != exercise.keyFocusAfter || exercise.keySelectedBefore != exercise.keySelectedAfter ||
                       exercise.keyScrollBefore != exercise.keyScrollAfter
                   ? 1
                   : 0)
           << '\n';
    if (!options.tdeckConstrained) {
        output << "scope=host-relative X11/LVGL interaction; not hardware timing\n";
    }
    return true;
}
} // namespace

int main(int argc, char **argv)
{
    Options options;
    if (!parseOptions(argc, argv, options)) {
        std::cerr << usage << '\n';
        return 2;
    }

    if (!displayAvailable()) {
        std::cerr << "DISPLAY is required to launch the X11 simulator\n";
        return 1;
    }

    X11MuiSimulator simulator;
    if (!options.windowTitle.empty()) {
        setenv("DEVICE_UI_X11_WINDOW_TITLE", options.windowTitle.c_str(), 1);
    }
    if (!simulator.initialize(options.implementation)) {
        std::cerr << "failed to initialize X11 MUI simulator\n";
        return 1;
    }
    simulator.populateLegacyNodeFixtures(options.nodes, options.seed);
    X11ExerciseReport exercise{};
    std::string hardwareBenchmarkReport;
    bool hardwareBenchmarkComplete = false;
    if (options.exerciseX11Input) {
        const std::string title = options.windowTitle.empty() ? "Meshtastic (320x240)" : options.windowTitle;
        exercise = exerciseX11Input(simulator, title);
        if (!exercise.openedDisplay || !exercise.foundWindow || !exercise.dragSent || !exercise.wheelSent ||
            !exercise.clickSent || !exercise.keySent || !exercise.dragXTestOk || !exercise.wheelXTestOk ||
            !exercise.clickXTestOk || !exercise.keyXTestOk || exercise.dragScrollBefore == exercise.dragScrollAfter ||
            !exercise.wheelScrollStableBefore ||
            (exercise.wheelScrollBefore == exercise.wheelScrollAfter &&
             exercise.wheelSelectedBefore == exercise.wheelSelectedAfter &&
             exercise.wheelFocusBefore == exercise.wheelFocusAfter) ||
            exercise.clickTarget == 0 ||
            (exercise.clickSelectedBefore == exercise.clickSelectedAfter &&
             exercise.clickFocusBefore == exercise.clickFocusAfter) ||
            (exercise.keyFocusBefore == exercise.keyFocusAfter && exercise.keySelectedBefore == exercise.keySelectedAfter &&
             exercise.keyScrollBefore == exercise.keyScrollAfter)) {
            writeReport(options, simulator, exercise, hardwareBenchmarkReport, hardwareBenchmarkComplete);
            std::cerr << "failed to exercise X11 input through LVGL\n";
            return 1;
        }
    }
    if (options.hardwareBenchmark) {
        const uint32_t benchmarkTimeoutMs = options.tdeckConstrained ? 60000U : 20000U;
        hardwareBenchmarkComplete =
            simulator.runNodeListHardwareBenchmark(benchmarkTimeoutMs, options.tdeckConstrained, hardwareBenchmarkReport);
        if (!hardwareBenchmarkReport.empty()) {
            std::cout << hardwareBenchmarkReport << '\n';
        }
        if (!hardwareBenchmarkComplete) {
            writeReport(options, simulator, exercise, hardwareBenchmarkReport, hardwareBenchmarkComplete);
            std::cerr << "failed to run node-list hardware benchmark through simulator\n";
            return 1;
        }
    }
    if (!writeReport(options, simulator, exercise, hardwareBenchmarkReport, hardwareBenchmarkComplete)) {
        std::cerr << "failed to write simulator report\n";
        return 1;
    }
    if (options.hardwareBenchmark) {
        return 0;
    }
    if (options.runForMs > 0) {
        for (uint32_t elapsed = 0; elapsed < options.runForMs; elapsed += 10) {
            simulator.pump(10);
        }
    } else {
        simulator.pumpUntilClosed();
    }
    return 0;
}
