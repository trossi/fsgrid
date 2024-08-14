cmake_minimum_required(VERSION 3.18)

project(${project_name} VERSION 1.0 DESCRIPTION "Fsgrid testing with ctest and googletest" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# TODO: add compiler flags for more warnings

option(package_tests "Build tests" ON)
if(package_tests)
    include(CTest)
    add_subdirectory(tests)
    enable_testing()
endif()
