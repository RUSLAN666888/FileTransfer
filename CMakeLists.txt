cmake_minimum_required(VERSION 3.20)
project(ft LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
find_package(GTest REQUIRED)

add_library(ft_core STATIC
    src/infra/crypto/sodium_random.cpp
)
target_include_directories(ft_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ft_core PUBLIC Qt6::Core)

if(WIN32)
    find_package(unofficial-sodium CONFIG REQUIRED)
    target_link_libraries(ft_core PRIVATE unofficial-sodium::sodium)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SODIUM REQUIRED libsodium)
    target_link_libraries(ft_core PRIVATE ${SODIUM_LIBRARIES})
    target_include_directories(ft_core PRIVATE ${SODIUM_INCLUDE_DIRS})
endif()

add_executable(ft src/main.cpp
    src/infra/crypto/sodium_random.h
    src/infra/crypto/sodium_random.cpp)
target_link_libraries(ft PRIVATE ft_core Qt6::Widgets)

enable_testing()

add_executable(test_infra
    tests/test_clock.cpp
    tests/test_random.cpp
)
target_link_libraries(test_infra PRIVATE ft_core GTest::gtest GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(test_infra)
