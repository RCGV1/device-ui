message(STATUS "Fetching protobufs ...")
if(ENABLE_MUI_HEADLESS_TESTS OR ENABLE_MUI_X11_SIMULATOR)
    set(PROTOBUF_TAG v2.7.25)
else()
    set(PROTOBUF_TAG v2.7.8)
endif()
FetchContent_Declare(
    Protobuf
    GIT_REPOSITORY "https://github.com/meshtastic/protobufs"
    GIT_TAG ${PROTOBUF_TAG}
)
FetchContent_MakeAvailable(Protobuf)
