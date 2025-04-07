cmake_minimum_required(VERSION 3.25)
project(eventbus-benchmarks)

CPMAddPackage(
    NAME nanobench
    GITHUB_REPOSITORY martinus/nanobench
    VERSION 4.3.11
    GIT_SHALLOW
)

file(GLOB benchmark_sources CONFIGURE_DEPENDS src/*.cpp)
add_executable(${PROJECT_NAME} ${benchmark_sources})
target_link_libraries(
    ${PROJECT_NAME} 
    PUBLIC 
        dp::eventbus
        doctest::doctest
        nanobench
        project_options
        project_warnings
)
