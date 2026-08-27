if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR is required")
endif()

set(files
    "${SOURCE_DIR}/tools/launch_node_list_simulator.sh"
    "${SOURCE_DIR}/cmake/RunMuiX11SimulatorInputTest.cmake"
    "${SOURCE_DIR}/cmake/AssertMuiX11WheelRequired.cmake"
)

foreach(file IN LISTS files)
    file(READ "${file}" content)
    if(content MATCHES "(^|[ \t;])wait[ \t]+-n([^A-Za-z0-9_-]|$)")
        message(FATAL_ERROR "${file} uses wait -n, which is unavailable in macOS Bash 3.2")
    endif()
endforeach()

file(READ "${SOURCE_DIR}/tools/launch_node_list_simulator.sh" launcher_content)
if(NOT launcher_content MATCHES "--implementation[ \t]+production")
    message(FATAL_ERROR "tools/launch_node_list_simulator.sh must launch the production simulator")
endif()
if(launcher_content MATCHES "--implementation[ \t]+(legacy|virtual_candidate)")
    message(FATAL_ERROR "tools/launch_node_list_simulator.sh must not launch legacy or virtual simulator modes")
endif()
