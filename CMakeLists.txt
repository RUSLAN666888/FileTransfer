cmake_minimum_required(VERSION 3.20)
project(ft LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
find_package(GTest REQUIRED)

add_library(ft_core STATIC src/core_placeholder.cpp)
target_link_libraries(ft_core PUBLIC Qt6::Core)

add_executable(ft src/main.cpp)
target_link_libraries(ft PRIVATE ft_core Qt6::Widgets)

enable_testing()

add_executable(test_dummy tests/test_dummy.cpp)
target_link_libraries(test_dummy PRIVATE ft_core GTest::gtest GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(test_dummy)
