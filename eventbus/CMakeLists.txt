cmake_minimum_required(VERSION 3.25)

project(eventbus VERSION "0.11.0")

set(project_headers
    include/eventbus/detail/function_traits.hpp
    include/eventbus/detail/storage_policy.hpp
    include/eventbus/detail/value_traits.hpp include/eventbus/event_bus.hpp)

# System threading library Required for multithreading testing
find_package(Threads REQUIRED)

# create library and aliases
add_library(${PROJECT_NAME} INTERFACE)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
add_library(dp::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} INTERFACE include)

target_link_libraries(${PROJECT_NAME} INTERFACE project_options
                                                Threads::Threads)

if(EVENTBUS_BUILD_TESTS)
  file(GLOB_RECURSE project_test_sources CONFIGURE_DEPENDS test/*.cpp)
  set(project_test_name ${PROJECT_NAME}-tests)
  add_executable(${project_test_name} ${project_test_sources})
  target_link_libraries(${project_test_name} PUBLIC doctest::doctest
                                                    ${PROJECT_NAME})

  # Note: doctest and similar testing frameworks can automatically configure
  # CMake tests. For other
  include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
  doctest_discover_tests(${project_test_name})
endif()
