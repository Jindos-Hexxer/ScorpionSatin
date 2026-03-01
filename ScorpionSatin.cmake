cmake_minimum_required(VERSION 3.25)
project(ScorpionSatin LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Path Definitions
set(LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs")
set(MODULES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/modules")

# MsQuic Configuration
# tests and tools are disabled
set(QUIC_BUILD_TEST OFF CACHE INTERNAL "")
set(QUIC_BUILD_TOOLS OFF CACHE INTERNAL "")
set(QUIC_ENABLE_LOGGING ON CACHE INTERNAL "")
add_subdirectory(${LIBS_DIR}/msquic EXCLUDE_FROM_ALL)

# Flecs & Jolt
set(FLECS_STATIC ON CACHE INTERNAL "")
add_subdirectory(${LIBS_DIR}/flecs EXICLE_FROM_ALL)
set(TARGET_UNIT_TESTS OFF CACHE INTERNAL "")
add_subdirectory(${LIBS_DIR}/jolt/Build JOLT_BUILD)

# bgfx Trio
add_subdirectory(${LIBS_DIR}/bx)
add_subdirectory(${LIBS_DIR}/bimg)
add_subdirectory(${LIBS_DIR}/bgfx)

# Core Module
add_library(ScorpionSatin SHARED)

# Modularize the SDK using Target Sources
target_sources(ScorpionSatin
    PRIVATE
    ${MODULES_DIR}/net/MsQuicWrapper.cpp
    ${MODULES_DIR}/rhi/RtxgiBridge.cpp
    ${MODULES_DIR}/ecs/CoreSystems.cpp
)

# Linkage (Public interfaces for the Game Project)
target_link_libraries(ScorpionSatin
    PUBLIC
    msquic
    flecs
    jolt
    bgfx
    bx
    bimg
)

target_include_directories(ScorpionSatin
    PUBLIC
    $<BUILD_INTERFACE:${MODULES_DIR}>
    $<BUILD_INTERFACE:${LIBS_DIR}/msquic/src/inc>
    $<INSTALL_INTERFACE:include>
)

# Target-Specific Definitions
if(WIN32)
    target_compile_definitions(ScorpionSatin PUBLIC SPN_PLATFORM_WINDOWS)
else()
    target_compile_definitions(ScorpionSatin PUBLIC SPN_PLATFORM_LINUX)
endif()

message(STATUS "ScorpionSatin initialized.")
