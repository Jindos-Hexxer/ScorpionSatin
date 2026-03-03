# Engine Architecture: Modular Vulkan Runtime (Shared Library)

This game engine is specifically intended for MMO games.

## Core Philosophy

- **Engine as a Shared Library:** Compiled to `Engine.dll` (Windows) / `Engine.so` (Linux). Contains all heavy backends (rendering, collision, networking).
- **Editor as the Host:** The `.exe` that creates the OS window, initializes Vulkan, and loads the Game DLL.
- **Game as a Plugin:** Compiled to `Game.dll`. Contains only logic (Flecs systems) and data definitions.
- **Data-Oriented:** State lives in the Engine's `flecs::world`; logic lives in the Game DLL.

## Platform Targets

- **Client (Windows):** Vulkan + RTXGI + ImGui Editor + NavMesh Visualization.
- **Server (Linux):** Headless SO + MsQuic + Recast/Detour Navigation.

## Technology Stack


| Layer          | Library                                                                  | Purpose                                                                                  |
| -------------- | ------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------- |
| **Graphics**   | [Vulkan 1.4](https://vulkan.org)                                         | Modern, low-level rendering.                                                             |
| **GPU Memory** | [VMA](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) | Vulkan memory management.                                                                |
| **GI**         | [NVIDIA RTXGI](https://github.com/NVIDIAGameWorks/RTXGI)                 | Dynamic Global Illumination (Ray Traced).                                                |
| **ECS**        | [Flecs](https://github.com/SanderMertens/flecs)                          | Logic, Entity Management, Querying.                                                      |
| **Collision**  | NavMesh + Spatial Grid                                                   | Environmental collision (NavMesh), entity collision (sphere/AABB), LOS (Detour raycast). |
| **Networking** | [MsQuic](https://github.com/microsoft/msquic)                            | High-speed UDP transport (QUIC).                                                         |
| **UI/Editor**  | [ImGui](https://github.com/ocornut/imgui)                                | Viewport, Gizmos, Entity Inspector.                                                      |
| **Windowing**  | [GLFW](https://www.glfw.org) or SDL3                                     | OS window creation and input.                                                            |
| **Navigation** | [Recast/Detour](https://github.com/recastnavigation/recastnavigation)    | NavMesh generation, runtime pathfinding, and environmental collision.                    |


## Directory Structure

```text
/src/core       Engine entry point and windowing.
/src/render     Vulkan wrappers, RTXGI integration, Shaders (GLSL).
/src/ecs        Flecs components and system definitions.
/src/collision  Spatial Grid, sphere/AABB checks, collision queries.
/src/editor     ImGui panels and debug tools.
/src/network    MsQuic client/server transport.
/src/navmesh    Recast/Detour integration.
/shaders        SPIR-V source files.
```

## Execution Flow (Game Loop)

1. **Poll Events:** GLFW input handling.
2. **ECS Progress:** `flecs::world::progress()`.
  - Spatial Grid update (entity cell assignment).
  - NavMesh movement (`moveAlongSurface` for position updates).
  - Collision checks (sphere/AABB queries against neighboring cells).
  - Game Logic Systems run.
  - MsQuic synchronization.
3. **Frame Rendering:**
  - Upload Flecs `Transform` data to GPU (SSBO).
  - Execute Vulkan Command Buffers (Render Graph).
  - `ImGui::Render()` (Editor UI).
4. **Present:** Submit to Swapchain.

## Subsystem Documentation

Each subsystem is documented in its own file for focused context:


| Document                                                         | Description                                                                                     |
| ---------------------------------------------------------------- | ----------------------------------------------------------------------------------------------- |
| [Renderer](architecture/renderer.md)                             | Vulkan abstraction layer, render graph passes, RTXGI global illumination, bindless descriptors. |
| [ECS](architecture/ecs.md)                                       | Flecs integration, component definitions, system pipeline, execution order.                     |
| [Collision & Spatial Logic](architecture/physics.md)             | NavMesh environmental collision, Spatial Grid entity collision, Detour LOS raycasting.          |
| [Editor](architecture/editor.md)                                 | ImGui-based editor, docking, viewport rendering, entity inspector.                              |
| [Networking](architecture/networking.md)                         | MsQuic transport layer, reliable streams, unreliable datagrams, concurrency.                    |
| [Server / Headless](architecture/server-headless.md)             | Headless mode for Linux servers, deployment workflow, zone-based scaling.                       |
| [NavMesh](architecture/navmesh.md)                               | Recast/Detour NavMesh baking, runtime pathfinding, authoritative movement validation.           |
| [Build System & Hot-Reload](architecture/build-and-hotreload.md) | CMake configuration, DLL hot-reloading, project structure, conditional builds.                  |
| [Scaling & Interest Management](architecture/scaling.md)         | 3-circle visibility, bitmask culling, packet batching, interest lists for 5,000+ players.       |


