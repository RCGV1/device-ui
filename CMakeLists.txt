cmake_minimum_required(VERSION 3.15)

################################################################################
##                             D E V I C E   U I                               #
################################################################################
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/version.txt ver)
project(device_ui VERSION ${ver} HOMEPAGE_URL https://github.com/meshtastic/device-ui LANGUAGES C CXX)

message(STATUS "device-ui version: ${PROJECT_VERSION}")

set(MAIN_PROJECT OFF)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(MAIN_PROJECT ON)
endif()

option(ENABLE_DOCTESTS "Include tests in the library. Setting this to OFF will remove all doctest related code.
                        Tests in tests/*.cpp will still be enabled." ${MAIN_PROJECT})
option(ENABLE_DEBUG_LOG "Enable debug log" OFF)
option(ENABLE_MUI_HEADLESS_TESTS "Use deterministic headless display and input support for MUI integration tests" OFF)
option(MUI_NODE_LIST_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer for native node-list tests" OFF)

if(ENABLE_MUI_HEADLESS_TESTS AND NOT ENABLE_DOCTESTS)
    message(FATAL_ERROR "ENABLE_MUI_HEADLESS_TESTS requires ENABLE_DOCTESTS")
endif()

if(MUI_NODE_LIST_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    else()
        message(FATAL_ERROR "MUI_NODE_LIST_SANITIZERS requires Clang or GCC")
    endif()
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL ON) # with newer cmake versions put all find_package in global scope
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(GENERATED_VIEW ${VIEW} "ui_320x240")

message(STATUS "Using C++${CMAKE_CXX_STANDARD}")
message(STATUS "Using Generated View: " ${GENERATED_VIEW})

add_compile_definitions(ARCH_PORTDUINO)
add_compile_definitions(ARDUINO)
add_compile_definitions(VIEW_320x240)

if(ENABLE_MUI_HEADLESS_TESTS)
    add_compile_definitions(DEVICE_UI_HEADLESS_TEST)
    add_compile_definitions(HAS_TFT=1)
    add_compile_definitions(RAM_SIZE=16384)
    set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "Build LVGL demos" FORCE)
    set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "Build LVGL examples" FORCE)
else()
    add_compile_definitions(USE_X11=1)
endif()

include(FetchContent)
if(ENABLE_MUI_HEADLESS_TESTS)
    message(STATUS "Fetching headless Portduino support ...")
    FetchContent_Declare(
        Portduino
        GIT_REPOSITORY "https://github.com/mverch67/framework-portduino"
        GIT_TAG "ce0ed7e9c62f3082b30ee9a5c369f30c1e55194a"
        SOURCE_SUBDIR cmake/device-ui-headless
    )
    FetchContent_MakeAvailable(Portduino)
    set(PORTDUINO_INCLUDE_DIRS
        ${portduino_SOURCE_DIR}/cores/arduino/api
        ${portduino_SOURCE_DIR}/cores/portduino
        ${portduino_SOURCE_DIR}/cores/portduino/FS
        ${portduino_SOURCE_DIR}/cores/arduino
        ${portduino_SOURCE_DIR}/libraries/SPI/src
        ${portduino_SOURCE_DIR}/libraries/Wire/src
        ${portduino_SOURCE_DIR}/libraries/WiFi/src
        ${portduino_SOURCE_DIR}/ArduinoCore-API/api
    )
    file(GLOB_RECURSE PORTDUINO_SOURCES
        ${portduino_SOURCE_DIR}/cores/portduino/*.c*
        ${portduino_SOURCE_DIR}/cores/portduino/linux/*.c*
        ${portduino_SOURCE_DIR}/cores/portduino/linux/gpio/*.c*
        ${portduino_SOURCE_DIR}/cores/portduino/FS/*.c*
        ${portduino_SOURCE_DIR}/cores/portduino/simulated/*.c*
        ${portduino_SOURCE_DIR}/cores/arduino/*.c*
        ${portduino_SOURCE_DIR}/ArduinoCore-API/api/*.c*
    )
    list(FILTER PORTDUINO_SOURCES EXCLUDE REGEX "cores/portduino/main\\.cpp$")
    list(FILTER PORTDUINO_SOURCES EXCLUDE REGEX "cores/portduino/linux/LinuxHardwareI2C\\.cpp$")
    list(FILTER PORTDUINO_SOURCES EXCLUDE REGEX "cores/portduino/simulated/SimHardwareI2C\\.cpp$")
    add_library(Portduino STATIC ${PORTDUINO_SOURCES})
else()
    include(Portduino)
endif()
if(ENABLE_MUI_HEADLESS_TESTS AND APPLE)
    find_path(ARGP_INCLUDE_DIR argp.h REQUIRED PATHS /opt/homebrew/include /usr/local/include)
    list(APPEND PORTDUINO_INCLUDE_DIRS ${ARGP_INCLUDE_DIR})
    foreach(PORTDUINO_INCLUDE_DIR IN LISTS PORTDUINO_INCLUDE_DIRS)
        target_compile_options(Portduino PRIVATE "-idirafter${PORTDUINO_INCLUDE_DIR}")
    endforeach()
endif()
if(ENABLE_MUI_HEADLESS_TESTS)
    add_library(LovyanGFX INTERFACE)
else()
    include(LovyanGFX)
endif()
include(LVGL)
include(protobuf)
include(nanopb)
include(PNGdec)
include(Doctest)

find_package(CURL REQUIRED)

if(ENABLE_MUI_HEADLESS_TESTS)
    file(GLOB_RECURSE sources source/* portduino/* locale/* generated/${GENERATED_VIEW}/*)
else()
    file(GLOB_RECURSE sources source/* generated/* portduino/* locale/* generated/${GENERATED_VIEW}/*)
endif()
file(GLOB_RECURSE sources_test tests/*.cpp)

if(ENABLE_MUI_HEADLESS_TESTS)
    list(APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/src/mesh-pb-constants.cpp)
    list(FILTER sources EXCLUDE REGEX "source/comms/")
    list(FILTER sources EXCLUDE REGEX "source/graphics/driver/(DisplayDriverFactory|FBDriver|X11Driver)\\.cpp$")
    list(FILTER sources EXCLUDE REGEX "portduino/Light_PWM\\.cpp$")
endif()

add_library(DeviceUI ${sources})
target_link_libraries(DeviceUI PRIVATE lvgl::lvgl LovyanGFX Portduino Protobufs CURL::libcurl)

if(ENABLE_MUI_HEADLESS_TESTS)
    execute_process(
        COMMAND git rev-parse --verify HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE DEVICE_UI_GIT_REVISION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT DEVICE_UI_GIT_REVISION)
        set(DEVICE_UI_GIT_REVISION unknown)
    endif()
    execute_process(
        COMMAND git diff --quiet --ignore-submodules --
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE DEVICE_UI_GIT_DIRTY
        ERROR_QUIET
    )

    set(NODE_LIST_BENCHMARK_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/tools
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/locale
        ${CMAKE_CURRENT_SOURCE_DIR}/portduino
        ${CMAKE_CURRENT_SOURCE_DIR}/generated/${GENERATED_VIEW}
        ${GENERATED_FILES_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${nanopb_SOURCE_DIR}
    )
    set(NODE_LIST_BENCHMARK_LIBRARIES
        DeviceUI
        lvgl::lvgl
        LovyanGFX
        Portduino
        Protobufs
        PNGdec
        protobuf-nanopb-static
        doctest::doctest
    )

    add_library(NodeListBenchmark STATIC tools/node_list_bench.cpp)
    target_include_directories(NodeListBenchmark PRIVATE ${NODE_LIST_BENCHMARK_INCLUDES})
    target_link_libraries(NodeListBenchmark PRIVATE ${NODE_LIST_BENCHMARK_LIBRARIES})
    target_compile_definitions(NodeListBenchmark PRIVATE
        DEVICE_UI_SOURCE_REVISION="${DEVICE_UI_GIT_REVISION}"
        DEVICE_UI_SOURCE_DIRTY=${DEVICE_UI_GIT_DIRTY}
    )

    add_executable(node_list_bench
        tools/node_list_bench.cpp
        tests/HeadlessDisplayDriver.cpp
        tests/HeadlessDisplayDriverFactory.cpp
        tests/HeadlessLinuxHardwareI2C.cpp
        tests/HeadlessPlatform.cpp
        tests/MuiTestHarness.cpp
    )
    target_include_directories(node_list_bench PRIVATE ${NODE_LIST_BENCHMARK_INCLUDES})
    target_link_libraries(node_list_bench PRIVATE ${NODE_LIST_BENCHMARK_LIBRARIES})
    target_compile_definitions(node_list_bench PRIVATE
        NODE_LIST_BENCH_EXECUTABLE
        UNIT_TEST
        DEVICE_UI_SOURCE_REVISION="${DEVICE_UI_GIT_REVISION}"
        DEVICE_UI_SOURCE_DIRTY=${DEVICE_UI_GIT_DIRTY}
    )
    if(APPLE)
        foreach(PORTDUINO_INCLUDE_DIR IN LISTS PORTDUINO_INCLUDE_DIRS)
            target_compile_options(NodeListBenchmark PRIVATE "-idirafter${PORTDUINO_INCLUDE_DIR}")
            target_compile_options(node_list_bench PRIVATE "-idirafter${PORTDUINO_INCLUDE_DIR}")
        endforeach()
    endif()
    set_target_properties(node_list_bench PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

# Handle Windows Static linking dependencies if applicable
if(WIN32)
    target_compile_definitions(DeviceUI PRIVATE CURL_STATICLIB)
    target_link_libraries(DeviceUI PRIVATE ws2_32 crypt32 wldap32 normaliz)
endif()

target_compile_options(DeviceUI PUBLIC -Wall -Wno-format -Wfloat-conversion)
if(ENABLE_MUI_HEADLESS_TESTS AND APPLE)
    foreach(PORTDUINO_INCLUDE_DIR IN LISTS PORTDUINO_INCLUDE_DIRS)
        target_compile_options(DeviceUI PRIVATE "-idirafter${PORTDUINO_INCLUDE_DIR}")
    endforeach()
    target_compile_options(DeviceUI PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-includeArduino.h>")
endif()

target_include_directories(DeviceUI PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(ENABLE_MUI_HEADLESS_TESTS)
    target_include_directories(DeviceUI PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
endif()
target_include_directories(DeviceUI PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(DeviceUI PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/locale)
target_include_directories(DeviceUI PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/portduino)
target_include_directories(DeviceUI PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/generated/${GENERATED_VIEW})
target_include_directories(DeviceUI PRIVATE ${GENERATED_FILES_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${nanopb_SOURCE_DIR})

if(ENABLE_DOCTESTS)
    target_include_directories(DeviceUI PRIVATE ${doctest_SOURCE_DIR})
    target_compile_definitions(DeviceUI PRIVATE UNIT_TEST)
    if(ENABLE_DEBUG_LOG)
        target_compile_definitions(DeviceUI PRIVATE DEBUG_UNIT_TEST)
    endif()
endif()

#
# Unit Tests
#
if(ENABLE_DOCTESTS)
    include(CTest)
    enable_testing()
    add_executable(tests ${sources_test})
    target_link_libraries(tests PRIVATE DeviceUI doctest::doctest lvgl::lvgl LovyanGFX Portduino Protobufs)
    if(ENABLE_MUI_HEADLESS_TESTS)
        target_link_libraries(tests PRIVATE PNGdec protobuf-nanopb-static NodeListBenchmark)
        target_compile_definitions(tests PRIVATE UNIT_TEST)
        if(APPLE)
            foreach(PORTDUINO_INCLUDE_DIR IN LISTS PORTDUINO_INCLUDE_DIRS)
                target_compile_options(tests PRIVATE "-idirafter${PORTDUINO_INCLUDE_DIR}")
            endforeach()
        endif()
    endif()
    target_include_directories(tests PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/locale
        ${CMAKE_CURRENT_SOURCE_DIR}/portduino
        ${CMAKE_CURRENT_SOURCE_DIR}/generated/${GENERATED_VIEW}
        ${CMAKE_CURRENT_SOURCE_DIR}/tools
    )
    if(ENABLE_MUI_HEADLESS_TESTS)
        target_include_directories(tests PRIVATE ${GENERATED_FILES_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${nanopb_SOURCE_DIR})
    endif()
    set_target_properties(tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    add_test(NAME tests COMMAND tests)
    if(ENABLE_MUI_HEADLESS_TESTS)
        add_test(NAME MuiNodeListIntegration COMMAND tests "--test-case=*deterministic headless display*")
        add_test(NAME NodeListBenchmark COMMAND tests "--test-case=*baseline benchmark emits list diagnostics*")
        add_test(NAME NodeStore COMMAND tests "--test-case=*node store*")
    endif()
endif()
