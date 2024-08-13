cmake_minimum_required(VERSION 3.18)

# Get MPI
find_package(MPI REQUIRED)

# Set variables
set(lib ${PROJECT_NAME})
set(objs objs)

# Build a static library, object library
add_library(${lib} STATIC)
add_library(${objs} OBJECT "")

# List the sources of the object library
target_sources(
    ${objs}
    PRIVATE
    main.cpp
    )

# Add any MPI options and definitions
target_compile_options(${objs}
    PRIVATE
    ${MPI_C_COMPILE_OPTIONS}
    )

target_compile_definitions(${objs}
    PRIVATE
    ${MPI_C_COMPILE_DEFINITIONS}
    )

# Add some include directories with the SYSTEM flag.
# This means our own compile options don't trigger warnings/errors
# from those header files
target_include_directories(${objs}
    SYSTEM
    PRIVATE
    ${MPI_C_INCLUDE_DIRS}
    )

# Our own header files
target_include_directories(${objs}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    )

# The static library links with MPI and the object library
target_link_options(${lib}
    PRIVATE
    ${MPI_C_LINK_FLAGS}
    )

target_link_libraries(${lib}
    PRIVATE
    ${objs}
    ${MPI_C_LIBRARIES}
    )

install(
    TARGETS ${lib}
    CONFIGURATIONS Release ReleaseWithDebInfo
    ARCHIVE DESTINATION ${install_dir}/lib
    LIBRARY DESTINATION ${install_dir}/lib
    PUBLIC_HEADER DESTINATION ${install_dir}/include
    )
