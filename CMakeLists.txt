cmake_minimum_required(VERSION 3.25)

project(eventbus)

set(CXX_STANDARD 17)
set(CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(CPM)

option(EVENTBUS_BUILD_TESTS "Build unit tests." ON)
option(EVENTBUS_BUILD_BENCHMARKS "Build benchmarks." OFF)
option(EVENTBUS_BUILD_EXAMPLES "Build examples" OFF)

# set up warnings interface project to re-use
include(CompilerWarnings)
add_library(project_warnings INTERFACE)
set_project_warnings(project_warnings)

# set up options interface project to re-use
add_library(project_options INTERFACE)
# this project requires C++17
target_compile_features(project_options INTERFACE cxx_std_17)

if(EVENTBUS_BUILD_TESTS)
    CPMAddPackage(
        NAME doctest
        GITHUB_REPOSITORY onqtam/doctest
        # bump CMake version to 3.5
        VERSION 2.4.12
    )

    # this needs to be in the top level directory
    # before any calls to `add_subdirectory()` for
    # ctest to be set up properly
    enable_testing()
endif()

add_subdirectory(eventbus)

if(EVENTBUS_BUILD_EXAMPLES)
    add_subdirectory(demo)
endif()

if(EVENTBUS_BUILD_BENCHMARKS)
    add_subdirectory(benchmark)
endif()
