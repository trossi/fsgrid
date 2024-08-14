cmake_minimum_required(VERSION 3.18)

set(interface_lib "${PROJECT_NAME}-header-only")
set(install_include_dir "${install_dir}/include")

add_library(${interface_lib} INTERFACE)
target_include_directories(${interface_lib} INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:${install_include_dir}>
    )

# Install the header only library to a specified directory
install(
    TARGETS ${interface_lib}
    CONFIGURATIONS Release ReleaseWithDebInfo
    PUBLIC_HEADER DESTINATION ${install_include_dir}
)
