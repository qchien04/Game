cmake_minimum_required(VERSION 3.15)
project(GameServer VERSION 1.0)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Threads
find_package(Threads REQUIRED)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/GameLogic
)

# Source files
set(GAME_LOGIC_SOURCES
)

set(SERVER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/server.cpp
)

set(ALL_SOURCES
    ${GAME_LOGIC_SOURCES}
    ${SERVER_SOURCES}
)

# Executable
add_executable(game_server ${ALL_SOURCES})

# Link libraries
target_link_libraries(game_server PRIVATE Threads::Threads)

# Windows-specific
if(WIN32)
    target_link_libraries(game_server PRIVATE ws2_32 mswsock)
    target_compile_definitions(game_server PRIVATE 
        _WIN32_WINNT=0x0601
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
        _WINSOCK_DEPRECATED_NO_WARNINGS
    )
endif()

# Linux-specific
if(UNIX)
    target_link_libraries(game_server PRIVATE pthread)
endif()

# Compiler flags
if(MSVC)
    target_compile_options(game_server PRIVATE /W4 /permissive-)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(game_server PRIVATE /O2 /GL)
        target_link_options(game_server PRIVATE /LTCG)
    endif()
else()
    target_compile_options(game_server PRIVATE -Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(game_server PRIVATE -O3 -march=native)
        target_link_options(game_server PRIVATE)
    endif()
endif()

# Info
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "System: ${CMAKE_SYSTEM_NAME}")
