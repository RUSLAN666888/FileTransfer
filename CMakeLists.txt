cmake_minimum_required(VERSION 3.20)
project(ft LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
find_package(GTest REQUIRED)

add_library(ft_core INTERFACE
    #infra/time
    src/infra/time/i_clock.h
    src/infra/time/system_clock.h
    src/infra/time/fake_clock.h

    #infra/crypto
    src/infra/crypto/i_random_source.h
    src/infra/crypto/fake_random.h
)
target_include_directories(ft_core INTERFACE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ft_core INTERFACE Qt6::Core)

add_executable(ft src/main.cpp)
target_link_libraries(ft PRIVATE ft_core Qt6::Widgets)

enable_testing()

add_executable(test_infra
    tests/test_clock.cpp
    tests/test_random.cpp
)
target_link_libraries(test_infra PRIVATE ft_core GTest::gtest GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(test_infra)
