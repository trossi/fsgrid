cmake_minimum_required(VERSION 3.18)

project(${project_name} VERSION 1.0 DESCRIPTION "Fsgrid testing with ctest and googletest" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# TODO: add compiler flags for more warnings

# Use Cmake module FetchContent to download googletest
# Use the full hash of the git commit of the version of googletest you want to use
# f8d7d77c06936315286eb55f8de22cd23c188571 is the full hash of googletest version 1.14.0

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/f8d7d77c06936315286eb55f8de22cd23c188571.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)

include(CTest)
add_subdirectory(tests)
enable_testing()
