cmake_minimum_required(VERSION 3.25)
project(ScorpionSatin LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Path Definitions
set(LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs")
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(SHADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")

# Build Options
option(ENGINE_HEADLESS "Build headless server (no Vulkan/ImGui/GLFW)" OFF)
option(ENGINE_EDITOR "Include ImGui editor panels" ON)
option(ENGINE_RTXGI "Enable RTXGI global illumination" ON)

# Always-built dependencies (Client and Headless)
set(QUIC_BUILD_TEST OFF CACHE INTERNAL "")
set(QUIC_BUILD_TOOLS OFF CACHE INTERNAL "")
set(QUIC_ENABLE_LOGGING ON CACHE INTERNAL "")
add_subdirectory(${LIBS_DIR}/msquic EXCLUDE_FROM_ALL)

set(FLECS_STATIC ON CACHE INTERNAL "")
set(FLECS_SHARED OFF CACHE BOOL "")
add_subdirectory(${LIBS_DIR}/flecs EXCLUDE_FROM_ALL)

# glm - math for camera and ECS
include(FetchContent)
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
set(GLM_BUILD_TESTS OFF CACHE BOOL "")
set(GLM_BUILD_INFO_ONLY ON CACHE BOOL "")
FetchContent_MakeAvailable(glm)

# cgltf - glTF 2.0 loader (header-only with CGLTF_IMPLEMENTATION in MeshLoader.cpp)
FetchContent_Declare(cgltf
    GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
    GIT_TAG v1.14
)
FetchContent_Populate(cgltf)

if(NOT TARGET cgltf)
    # Generate a tiny .c file in the build directory automatically!
    set(CGLTF_IMPL_FILE "${CMAKE_CURRENT_BINARY_DIR}/cgltf_impl.c")
    file(WRITE ${CGLTF_IMPL_FILE} "#define CGLTF_IMPLEMENTATION\n#include \"cgltf.h\"\n")

    # Compile that generated file into a static library
    add_library(cgltf STATIC ${CGLTF_IMPL_FILE})
    
    # Expose the header directory to whoever links this target
    target_include_directories(cgltf PUBLIC ${cgltf_SOURCE_DIR})
    
    # Suppress third-party warnings so they don't clutter your build output
    if(MSVC)
        target_compile_options(cgltf PRIVATE /w)
    else()
        target_compile_options(cgltf PRIVATE -w)
    endif()
endif()

# Recast/Detour - used by both Client and Headless
set(RECASTNAVIGATION_DEMO OFF CACHE BOOL "")
set(RECASTNAVIGATION_TESTS OFF CACHE BOOL "")
set(RECASTNAVIGATION_EXAMPLES OFF CACHE BOOL "")
add_subdirectory(${LIBS_DIR}/recastnavigation EXCLUDE_FROM_ALL)

# Client-only dependencies (Vulkan, windowing, editor)
set(VULKAN_FOUND OFF)
if(NOT ENGINE_HEADLESS)
    find_package(Vulkan)
    if(Vulkan_FOUND)
        set(VULKAN_FOUND ON)
    else()
        message(WARNING "Vulkan not found - client build disabled. Install Vulkan SDK or set Vulkan_ROOT.")
    endif()
endif()

if(VULKAN_FOUND)
    # VMA - Vulkan memory management
    add_subdirectory(${LIBS_DIR}/VulkanMemoryAllocator EXCLUDE_FROM_ALL)

    # vk-bootstrap - Vulkan init
    set(VK_BOOTSTRAP_TEST OFF CACHE BOOL "")
    add_subdirectory(${LIBS_DIR}/vk-bootstrap EXCLUDE_FROM_ALL)

    # GLFW - Windowing
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "")
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "")
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "")
    set(GLFW_INSTALL OFF CACHE BOOL "")
    add_subdirectory(${LIBS_DIR}/glfw EXCLUDE_FROM_ALL)

    # ImGui - Editor UI
    add_library(imgui STATIC
        ${LIBS_DIR}/imgui/imgui.cpp
        ${LIBS_DIR}/imgui/imgui_draw.cpp
        ${LIBS_DIR}/imgui/imgui_tables.cpp
        ${LIBS_DIR}/imgui/imgui_widgets.cpp
        ${LIBS_DIR}/imgui/backends/imgui_impl_glfw.cpp
        ${LIBS_DIR}/imgui/backends/imgui_impl_vulkan.cpp
    )
    target_include_directories(imgui PUBLIC
        ${LIBS_DIR}/imgui
        ${LIBS_DIR}/imgui/backends
    )
    target_link_libraries(imgui PUBLIC glfw Vulkan::Vulkan)
endif()

# =============================================================================
# Engine (Shared Library) - Engine.dll / Engine.so
# =============================================================================
add_library(Engine SHARED "")

set_target_properties(Engine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

add_subdirectory(${SRC_DIR}/ecs)
add_subdirectory(${SRC_DIR}/network)
add_subdirectory(${SRC_DIR}/collision)
add_subdirectory(${SRC_DIR}/navmesh)
add_subdirectory(${SRC_DIR}/mesh)
target_include_directories(Engine PRIVATE ${cgltf_SOURCE_DIR})
if(VULKAN_FOUND)
    add_subdirectory(${SRC_DIR}/render)
endif()

target_include_directories(Engine
    PUBLIC
    $<BUILD_INTERFACE:${INCLUDE_DIR}>
    $<BUILD_INTERFACE:${SRC_DIR}>
    $<BUILD_INTERFACE:${LIBS_DIR}/msquic/src/inc>
    $<BUILD_INTERFACE:${LIBS_DIR}/flecs/include>
    $<BUILD_INTERFACE:${LIBS_DIR}/recastnavigation/Detour/Include>
    $<BUILD_INTERFACE:${LIBS_DIR}/recastnavigation/Recast/Include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(Engine
    PUBLIC
    msquic
    flecs_static
    Recast
    Detour
    glm::glm
)

if(VULKAN_FOUND)
    target_include_directories(Engine PUBLIC
        $<BUILD_INTERFACE:${LIBS_DIR}/VulkanMemoryAllocator/include>
        $<BUILD_INTERFACE:${LIBS_DIR}/vk-bootstrap/src>
    )
    target_link_libraries(Engine PUBLIC
        Vulkan::Vulkan
        GPUOpen::VulkanMemoryAllocator
        vk-bootstrap::vk-bootstrap
        glfw
    )
    if(ENGINE_RTXGI)
        target_compile_definitions(Engine PUBLIC ENGINE_RTXGI=1)
        target_include_directories(Engine PUBLIC $<BUILD_INTERFACE:${LIBS_DIR}/rtxgi/Samples/Pathtracer>)
    endif()
    target_compile_definitions(Engine PUBLIC ENGINE_REVERSE_Z=1)
endif()

if(WIN32)
    target_compile_definitions(Engine PUBLIC SPN_PLATFORM_WINDOWS)
else()
    target_compile_definitions(Engine PUBLIC SPN_PLATFORM_LINUX)
endif()

# =============================================================================
# Editor (Executable) - Editor.exe - Client only
# =============================================================================
if(VULKAN_FOUND AND ENGINE_EDITOR)
    add_executable(Editor ${SRC_DIR}/core/Main.cpp)
    target_include_directories(Editor PRIVATE
        ${INCLUDE_DIR}
        ${LIBS_DIR}/glfw/include
        ${LIBS_DIR}/imgui
        ${LIBS_DIR}/imgui/backends
    )
    target_link_libraries(Editor PRIVATE Engine imgui glfw Vulkan::Vulkan)
endif()

# =============================================================================
# Game (Plugin DLL) - Game.dll - Client only, stub for hot-reload
# =============================================================================
if(VULKAN_FOUND)
    add_library(Game SHARED ${SRC_DIR}/ecs/GamePlugin.cpp)
    target_include_directories(Game PRIVATE ${INCLUDE_DIR} ${LIBS_DIR}/flecs/include)
    target_link_libraries(Game PRIVATE Engine flecs_static)
    set_target_properties(Game PROPERTIES PREFIX "" OUTPUT_NAME "Game")
endif()

# =============================================================================
# DedicatedServer (Executable) - Headless only
# =============================================================================
if(ENGINE_HEADLESS)
    add_executable(DedicatedServer ${SRC_DIR}/core/ServerMain.cpp)
    target_include_directories(DedicatedServer PRIVATE ${INCLUDE_DIR})
    target_link_libraries(DedicatedServer PRIVATE Engine)
    # Ensure Engine is built first and linker finds Engine.lib
    add_dependencies(DedicatedServer Engine)
    set_target_properties(DedicatedServer PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release
    )
endif()

# =============================================================================
# Shader Compilation
# =============================================================================
if(VULKAN_FOUND)
    set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
    file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})
    if(NOT Vulkan_GLSLC_EXECUTABLE)
        find_program(Vulkan_GLSLC_EXECUTABLE NAMES glslc glslc.exe HINTS "${Vulkan_INCLUDE_DIR}/../bin" "${Vulkan_INCLUDE_DIR}/../Bin")
    endif()
    file(GLOB GLSL_SHADERS "${SHADERS_DIR}/*.vert" "${SHADERS_DIR}/*.frag" "${SHADERS_DIR}/*.comp")
    foreach(SHADER ${GLSL_SHADERS})
        get_filename_component(SHADER_NAME ${SHADER} NAME_WE)
        get_filename_component(SHADER_EXT ${SHADER} EXT)
        if(SHADER_EXT STREQUAL ".vert")
            set(STAGE "vert")
        elseif(SHADER_EXT STREQUAL ".frag")
            set(STAGE "frag")
        elseif(SHADER_EXT STREQUAL ".comp")
            set(STAGE "comp")
        else()
            continue()
        endif()
        if(Vulkan_GLSLC_EXECUTABLE)
            add_custom_command(
                OUTPUT ${SHADER_OUTPUT_DIR}/${SHADER_NAME}_${STAGE}.spv
                COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${SHADER} -o ${SHADER_OUTPUT_DIR}/${SHADER_NAME}_${STAGE}.spv
                DEPENDS ${SHADER}
                COMMENT "Compiling ${SHADER_NAME}${SHADER_EXT} to SPIR-V"
            )
            list(APPEND SPIRV_SHADERS ${SHADER_OUTPUT_DIR}/${SHADER_NAME}_${STAGE}.spv)
        endif()
    endforeach()
    # Ray tracing miss shader (shaders/ray_miss.rmiss): glslc has no .rmiss stage;
    # glslangValidator needs ray-tracing-enabled build and --target-env=vulkan1.2+.
    # Source is built when RTXGI RT pipeline is wired; see RtxgiBridge.cpp comment.
    if(SPIRV_SHADERS)
        add_custom_target(CompileShaders ALL DEPENDS ${SPIRV_SHADERS})
    endif()
endif()

message(STATUS "ScorpionSatin: ENGINE_HEADLESS=${ENGINE_HEADLESS}, ENGINE_EDITOR=${ENGINE_EDITOR}, ENGINE_RTXGI=${ENGINE_RTXGI}")
