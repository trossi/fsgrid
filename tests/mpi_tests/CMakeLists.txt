cmake_minimum_required(VERSION 3.18)

find_package(MPI REQUIRED)

set(testname ${PROJECT_NAME}_mpi_tests)

add_executable(${testname}
    main.cpp
    foo.cpp
)

target_compile_options(${testname} PRIVATE
    ${MPI_CXX_COMPILE_OPTIONS}
)

target_compile_definitions(${testname} PRIVATE
    ${MPI_CXX_COMPILE_DEFINITIONS}
)

target_include_directories(${testname} PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
)

target_include_directories(${testname} SYSTEM PRIVATE
    ${MPI_CXX_INCLUDE_DIRS}
)

target_link_options(${testname} PRIVATE
    ${MPI_CXX_LINK_FLAGS}
)

target_link_libraries(
  ${testname}
  GTest::gtest_main
  ${MPI_CXX_LIBRARIES}
)

# Run the tests with mpiexec
# Need to add before gtest_add_tests to have effect

# !!!!
# N.B. using gtest_discover_tests instead of gtest_add_tests
# adds the tests X times,
# where X is the number of processes used for mpi
# In other words "${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} 4"
# runs all the discovered tests four times
# !!!!
set_property(TARGET ${testname}
    PROPERTY CROSSCOMPILING_EMULATOR
    ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} 4
    )

gtest_add_tests(TARGET ${testname})
