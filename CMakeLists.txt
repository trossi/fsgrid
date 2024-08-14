cmake_minimum_required(VERSION 3.18...3.24)

project(${project_name} VERSION 1.0 DESCRIPTION "Fsgrid testing with ctest and googletest" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# TODO: add compiler flags for more warnings

# Download googletest as a dependency
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        f8d7d77c06936315286eb55f8de22cd23c188571 # release-1.14.0
)
FetchContent_MakeAvailable(googletest)

include(CTest)
add_subdirectory(tests)
enable_testing()
