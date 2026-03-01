cmake_minimum_required(VERSION 3.25)
project(ScorpionSatin LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Path Definitions
set(LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs")
set(MODULES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/modules")

# MsQuic Configuration
set(QUIC_BUILD_TEST OFF CACHE INTERNAL "")
set(QUIC_BUILD_TOOLS OFF CACHE INTERNAL "")
set(QUIC_ENABLE_LOGGING ON CACHE INTERNAL "")
add_subdirectory(${LIBS_DIR}/msquic EXCLUDE_FROM_ALL)

# Flecs
set(FLECS_STATIC ON CACHE INTERNAL "")
add_subdirectory(${LIBS_DIR}/flecs EXCLUDE_FROM_ALL)

# Jolt Physics
set(TARGET_UNIT_TESTS OFF CACHE BOOL "")
set(TARGET_HELLO_WORLD OFF CACHE BOOL "")
set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "")
set(TARGET_SAMPLES OFF CACHE BOOL "")
set(TARGET_VIEWER OFF CACHE BOOL "")
set(GENERATE_DEBUG_SYMBOLS OFF CACHE BOOL "")
add_subdirectory(${LIBS_DIR}/jolt-physics/Build ${CMAKE_BINARY_DIR}/jolt_build EXCLUDE_FROM_ALL)

# bgfx
add_subdirectory(${LIBS_DIR}/bgfx-cmake EXCLUDE_FROM_ALL)

# Core Module - ScorpionSatin Engine Library
add_library(ScorpionSatin SHARED)

# Modularize the SDK using Target Sources
target_sources(ScorpionSatin
    PRIVATE
    ${MODULES_DIR}/net/MsQuicWrapper.cpp
    ${MODULES_DIR}/rhi/RtxgiBridge.cpp
    ${MODULES_DIR}/core/CoreSystems.cpp
)

# Linkage (Public interfaces for the Game Project)
target_link_libraries(ScorpionSatin
    PUBLIC
    msquic
    flecs
    Jolt
    bgfx
    bx
    bimg
)

target_include_directories(ScorpionSatin
    PUBLIC
    $<BUILD_INTERFACE:${MODULES_DIR}>
    $<BUILD_INTERFACE:${LIBS_DIR}/msquic/src/inc>
    $<BUILD_INTERFACE:${LIBS_DIR}/flecs/include>
    $<BUILD_INTERFACE:${LIBS_DIR}/jolt-physics/Source>
    $<BUILD_INTERFACE:${LIBS_DIR}/rtxgi/rtxgi/include>
    $<INSTALL_INTERFACE:include>
)

# Platform Definitions
if(WIN32)
    target_compile_definitions(ScorpionSatin PUBLIC SPN_PLATFORM_WINDOWS)
else()
    target_compile_definitions(ScorpionSatin PUBLIC SPN_PLATFORM_LINUX)
endif()

message(STATUS "ScorpionSatin Engine initialized.")