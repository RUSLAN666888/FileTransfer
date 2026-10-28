cmake_minimum_required(VERSION 3.5)

project(FileTransfer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(FileTransfer main.cpp
    docs/ARCHITECTURE.md
    docs/PLAN.md
    docs/SPECIFICATION.md
    docs/PROTOCOL.md)

include(GNUInstallDirs)
install(TARGETS FileTransfer
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
