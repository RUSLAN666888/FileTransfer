cmake_minimum_required(VERSION 3.20)
project(ft LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
find_package(GTest REQUIRED)

add_library(ft_core STATIC
    src/infra/crypto/sodium_random.cpp
    src/infra/crypto/sodium_crypto.cpp
    src/infra/file_system/file_system.cpp
    src/infra/keystore/file_keystore.cpp

    src/domain/value/peer_id.cpp
    src/domain/value/transfer_id.cpp
    src/domain/identity_service.cpp
    src/domain/fsm/session_fsm.cpp
)
target_include_directories(ft_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ft_core PUBLIC Qt6::Core)

if(WIN32)
    find_package(unofficial-sodium CONFIG REQUIRED)
    target_link_libraries(ft_core PRIVATE unofficial-sodium::sodium)
    target_include_directories(ft_core PRIVATE "${unofficial-sodium_DIR}/../../include")
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SODIUM REQUIRED libsodium)
    target_link_libraries(ft_core PRIVATE ${SODIUM_LIBRARIES})
    target_include_directories(ft_core PRIVATE ${SODIUM_INCLUDE_DIRS})
endif()

add_executable(ft src/main.cpp)
target_link_libraries(ft PRIVATE ft_core Qt6::Widgets)

enable_testing()
include(GoogleTest)

add_executable(test_infra
    tests/infra/test_clock.cpp
    tests/infra/test_random.cpp
    tests/infra/test_crypto.cpp
    tests/infra/test_result.cpp
    tests/infra/test_file_system.cpp
    tests/infra/test_file_keystore.cpp
)
target_link_libraries(test_infra PRIVATE ft_core GTest::gtest GTest::gtest_main)
gtest_discover_tests(test_infra)

add_executable(test_domain
    tests/domain/test_peer_id.cpp
    tests/domain/test_transfer_id.cpp
    tests/domain/test_identity_service.cpp
    tests/domain/test_session_fsm.cpp
)
target_link_libraries(test_domain PRIVATE ft_core GTest::gtest GTest::gtest_main)
gtest_discover_tests(test_domain)
