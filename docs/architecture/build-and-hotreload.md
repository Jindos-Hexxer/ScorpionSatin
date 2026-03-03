# Build System & Hot-Reload

The engine uses CMake for cross-platform builds, producing shared libraries that enable a plugin architecture and hot-reloading during development.

## Build Targets

| Target | Output | Platform | Description |
| :--- | :--- | :--- | :--- |
| Engine (Client) | `Engine.dll` | Windows | Full engine with Vulkan, MsQuic, Recast/Detour, ImGui. |
| Engine (Server) | `Engine.so` | Linux | Headless engine: MsQuic, Recast/Detour, Spatial Grid. No graphics. |
| Game (Client) | `Game.dll` | Windows | Game logic plugin loaded by the Editor. |
| Game (Server) | `GameServer.so` | Linux | Game logic for the dedicated server. |
| Editor | `Editor.exe` | Windows | Host executable that creates the window and loads Engine + Game DLLs. |
| Dedicated Server | `DedicatedServer` | Linux | Host executable for headless server. |

## Project Structure (Two Repos)

```text
/Engine (Repo A)
├── include/                # Public headers for Game Projects
│   ├── EngineCore.h        # Entry point & App base class
│   └── Renderer.h          # Vulkan interfaces
├── src/
│   ├── Runtime/            # Vulkan (vk-bootstrap), MsQuic, Recast/Detour
│   ├── Editor/             # ImGui Viewport & Entity Inspector
│   └── DedicatedServer/    # Headless entry point (Linux)
├── thirdparty/             # Flecs, VMA, RTXGI SDK, Recast/Detour, MsQuic
└── CMakeLists.txt

/GameProject (Repo B)
├── src/
│   ├── Main.cpp            # Inherits from Engine::Application
│   └── Systems/            # Game-specific Flecs systems
├── assets/                 # Models, Textures, Shaders, NavMesh .bin
└── CMakeLists.txt          # find_package(MyEngine REQUIRED)
```

## CMake Configuration

### Client Build (Windows)

```cmake
cmake -DENGINE_HEADLESS=OFF -DENGINE_EDITOR=ON ..
```

Produces `Engine.dll`, `Editor.exe`, and the Game DLL.

### Server Build (Linux)

```cmake
cmake -DENGINE_HEADLESS=ON -DENGINE_EDITOR=OFF .. && make
```

Strips Vulkan, RTXGI, ImGui, and GLFW. Produces `Engine.so` and `DedicatedServer`. See [Server / Headless](server-headless.md).

### Key CMake Options

| Option | Default | Description |
| :--- | :--- | :--- |
| `ENGINE_HEADLESS` | `OFF` | Strip all graphics; build for server. |
| `ENGINE_EDITOR` | `ON` | Include ImGui editor panels. |
| `ENGINE_RTXGI` | `ON` | Enable RTXGI global illumination (requires RTX GPU). |

## Hot-Reloading (Development)

Hot-reloading allows the Game DLL to be rebuilt and reloaded without restarting the editor or losing game state.

### How It Works

1. **IGameInstance Interface:** The Game DLL exports a class implementing `IGameInstance`. The Editor calls methods on this interface (`Init`, `Shutdown`, `RegisterSystems`).
2. **State Lives in Engine:** The `flecs::world` is owned by the Engine DLL, not the Game DLL. Entity data (positions, health, physics bodies) survives a Game DLL reload.
3. **Temp-Copy Strategy:**
   - The Editor loads a **copy** of `Game.dll` (e.g., `Game_temp.dll`).
   - The IDE/compiler writes to the original `Game.dll`.
   - On detecting a change, the Editor:
     1. Calls `IGameInstance::Shutdown()` on the old DLL (unregisters game systems from Flecs).
     2. Unloads the old temp copy.
     3. Copies the new `Game.dll` to a fresh temp file.
     4. Loads the new temp copy.
     5. Calls `IGameInstance::Init()` (re-registers systems against the existing `flecs::world`).
4. **Result:** Game logic changes take effect immediately. Entity state is preserved.

### Limitations

- Adding/removing Flecs **components** (struct definitions) requires a full restart because the world's component metadata changes.
- Adding/removing Flecs **systems** works fine via hot-reload.
- NavMesh `.bin` file changes require a restart (the mesh is loaded once at startup).

<!-- TODO: Document file-watcher implementation for detecting DLL changes -->

## Shader Compilation

- GLSL shaders in `/shaders` are compiled to SPIR-V at build time using `glslangValidator` or `glslc`.
- CMake custom commands watch for `.vert`, `.frag`, `.comp` file changes and recompile.
- Compiled `.spv` files are output to the build directory and loaded at runtime.

<!-- TODO: Document runtime shader hot-reload for faster iteration -->

## Related Documents

- [Renderer](renderer.md) -- Shader pipeline and SPIR-V compilation.
- [ECS](ecs.md) -- `flecs::world` ownership enables state preservation during hot-reload.
- [Server / Headless](server-headless.md) -- CMake headless build configuration.
- [Editor](editor.md) -- Editor is the host executable that drives hot-reload.
