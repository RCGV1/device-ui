#define DOCTEST_CONFIG_IMPLEMENT
#include "X11MuiSimulator.h"
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
struct Options {
    X11MuiSimulator::Implementation implementation = X11MuiSimulator::Implementation::Legacy;
    size_t nodes = 100;
    uint32_t seed = 42;
    uint32_t runForMs = 0;
    bool exerciseX11Input = false;
    std::string reportPath;
    std::string windowTitle;
};

constexpr std::string_view usage =
    "usage: mui_node_list_simulator --implementation legacy|virtual_candidate [--nodes N] [--seed N] [--run-for-ms N] "
    "[--window-title TITLE] [--exercise-x11-input] [--report PATH]";

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

struct X11ExerciseReport {
    bool openedDisplay = false;
    bool foundWindow = false;
    bool dragSent = false;
    bool wheelSent = false;
    bool clickSent = false;
    bool keySent = false;
    int32_t scrollBefore = 0;
    int32_t scrollAfter = 0;
    uint32_t selectedBefore = 0;
    uint32_t selectedAfter = 0;
};

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

    XRaiseWindow(display, window);
    XSetInputFocus(display, window, RevertToParent, CurrentTime);
    XWarpPointer(display, None, window, 0, 0, 0, 0, 160, 210);
    XFlush(display);
    pumpFor(simulator, 40);

    XTestFakeButtonEvent(display, Button1, True, CurrentTime);
    for (int y : {180, 140, 100, 60, 40}) {
        XTestFakeMotionEvent(display, DefaultScreen(display), 160, y, CurrentTime);
        XFlush(display);
        pumpFor(simulator, 30);
    }
    XTestFakeButtonEvent(display, Button1, False, CurrentTime);
    XFlush(display);
    report.dragSent = true;
    pumpFor(simulator, 120);

    for (int i = 0; i < 4; ++i) {
        XTestFakeButtonEvent(display, Button5, True, CurrentTime);
        XTestFakeButtonEvent(display, Button5, False, CurrentTime);
    }
    XFlush(display);
    report.wheelSent = true;
    pumpFor(simulator, 120);

    XWarpPointer(display, None, window, 0, 0, 0, 0, 160, 82);
    XTestFakeButtonEvent(display, Button1, True, CurrentTime);
    XTestFakeButtonEvent(display, Button1, False, CurrentTime);
    XFlush(display);
    report.clickSent = true;
    pumpFor(simulator, 120);

    const KeyCode key = XKeysymToKeycode(display, XK_Next);
    if (key != 0) {
        XTestFakeKeyEvent(display, key, True, CurrentTime);
        XTestFakeKeyEvent(display, key, False, CurrentTime);
        XFlush(display);
        report.keySent = true;
        pumpFor(simulator, 120);
    }

    report.scrollAfter = simulator.nodeListScrollYForTesting();
    report.selectedAfter = simulator.selectedNodeForTesting();
    XCloseDisplay(display);
    return report;
}

bool writeReport(const Options &options, const X11MuiSimulator &simulator, const X11ExerciseReport &exercise)
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
    output << "x11_display_opened=" << (exercise.openedDisplay ? 1 : 0) << '\n';
    output << "x11_window_found=" << (exercise.foundWindow ? 1 : 0) << '\n';
    output << "drag_sent=" << (exercise.dragSent ? 1 : 0) << '\n';
    output << "wheel_sent=" << (exercise.wheelSent ? 1 : 0) << '\n';
    output << "click_sent=" << (exercise.clickSent ? 1 : 0) << '\n';
    output << "key_sent=" << (exercise.keySent ? 1 : 0) << '\n';
    output << "scroll_before=" << exercise.scrollBefore << '\n';
    output << "scroll_after=" << exercise.scrollAfter << '\n';
    output << "scroll_changed=" << (exercise.scrollBefore != exercise.scrollAfter ? 1 : 0) << '\n';
    output << "selected_before=" << exercise.selectedBefore << '\n';
    output << "selected_after=" << exercise.selectedAfter << '\n';
    output << "scope=host-relative X11/LVGL interaction; not hardware timing\n";
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
    if (options.exerciseX11Input) {
        const std::string title = options.windowTitle.empty() ? "Meshtastic (320x240)" : options.windowTitle;
        exercise = exerciseX11Input(simulator, title);
        if (!exercise.openedDisplay || !exercise.foundWindow || !exercise.dragSent || !exercise.wheelSent ||
            !exercise.clickSent || !exercise.keySent || exercise.scrollBefore == exercise.scrollAfter) {
            writeReport(options, simulator, exercise);
            std::cerr << "failed to exercise X11 input through LVGL\n";
            return 1;
        }
    }
    if (!writeReport(options, simulator, exercise)) {
        std::cerr << "failed to write simulator report\n";
        return 1;
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
