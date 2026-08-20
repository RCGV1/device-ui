if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR is required")
endif()

set(files
    "${SOURCE_DIR}/tools/launch_node_list_simulator.sh"
    "${SOURCE_DIR}/cmake/RunMuiX11SimulatorPairLaunchTest.cmake"
)

foreach(file IN LISTS files)
    file(READ "${file}" content)
    if(content MATCHES "(^|[ \t;])wait[ \t]+-n([^A-Za-z0-9_-]|$)")
        message(FATAL_ERROR "${file} uses wait -n, which is unavailable in macOS Bash 3.2")
    endif()
endforeach()
