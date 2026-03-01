<div align="center">
  <img src="./img/logo.png" alt="ScorpionSatin Logo" width="100">

# ScorpionSatin Engine

A high-performance game engine built with modern C++23, featuring physics simulation, ECS architecture, real-time graphics rendering, and networked multiplayer support.
</div>

## Features

- **Physics Engine**: Jolt Physics for realistic collision and dynamics
- **ECS Framework**: Flecs for flexible entity-component-system architecture
- **Graphics Rendering**: bgfx for cross-platform graphics abstraction
- **Networking**: MsQuic for low-latency multiplayer communication
- **Ray Tracing**: RTXGI for real-time global illumination
- **Cross-Platform**: Windows (client), Linux (server)

## Prerequisites

### Windows (Build Host)
- **Visual Studio 2022** or later with C++ build tools
- **CMake 3.25+** (add to PATH or install from [cmake.org](https://cmake.org))
- **Git** with submodule support

### Linux (Build Host)
- **GCC 11+** or **Clang 14+**
- **CMake 3.25+**
- **Git** with submodule support

## Initial Setup

### 1. Clone the Repository
```bash
git clone <your-repo-url> ScorpionSatin
cd ScorpionSatin
```

### 2. Initialize Git Submodules
```bash
git submodule update --init --recursive
```

This fetches all external dependencies:
- `libs/jolt-physics` - Physics simulation
- `libs/flecs` - ECS framework
- `libs/msquic` - Network communication
- `libs/bgfx-cmake` - Graphics rendering
- `libs/rtxgi` - Ray tracing global illumination

## Building on Windows

### Quick Build
Simply run the build script in PowerShell:
```powershell
.\build.bat
```

This will:
1. Create a `build/` directory
2. Configure CMake with Visual Studio 17 2022 (64-bit)
3. Compile all dependencies and the engine in Release mode
4. Output: `build/Release/ScorpionSatin.dll`

### Manual Build
```powershell
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
cd ..
```

### Debug Build
```powershell
mkdir build_debug
cd build_debug
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Debug
cd ..
```

## Building on Linux

### Setup
```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
cd ..
```

**Output:** `build/libScorpionSatin.so`

## Project Structure

```
ScorpionSatin/
├── /libs                    # External dependencies (Git submodules)
│   ├── jolt-physics/        # Physics engine
│   ├── flecs/               # ECS framework
│   ├── msquic/              # Network library
│   ├── bgfx-cmake/          # Graphics rendering
│   └── rtxgi/               # Ray tracing
├── /modules                 # Engine modules
│   ├── net/                 # Networking (MsQuic wrapper)
│   ├── rhi/                 # Rendering (RTXGI integration)
│   └── core/                # Core ECS systems
├── CMakeLists.txt           # Main build configuration
├── build.bat                # Windows build script
└── build/                   # Build output directory
```

## Using ScorpionSatin in Your Game

Create a game repository with this structure:

```
Project_YourGame/
├── /engine                  # Git submodule → ScorpionSatin
├── /src
│   ├── main.cpp             # Entry point
│   ├── /combat              # Game-specific ECS systems
│   └── /ui                  # Game UI
├── /assets                  # Game assets
├── /data                    # Exported game data
└── CMakeLists.txt           # Links to ScorpionSatin engine
```

### Game CMakeLists.txt Example
```cmake
cmake_minimum_required(VERSION 3.25)
project(YourGame LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)

# Engine submodule
add_subdirectory(engine)

# Windows Client
if(WIN32)
    add_executable(GameClient src/main.cpp)
    target_link_libraries(GameClient PRIVATE ScorpionSatin)
    target_compile_definitions(GameClient PRIVATE GAME_CLIENT ENABLE_RENDERING ENABLE_PHYSICS)
endif()

# Linux Server
if(UNIX AND NOT APPLE)
    add_executable(GameServer src/main.cpp)
    target_link_libraries(GameServer PRIVATE ScorpionSatin)
    target_compile_definitions(GameServer PRIVATE GAME_SERVER)
endif()
```

### Game Code Example
```cpp
#include <flecs.h>

int main() {
    flecs::world ecs;

#ifdef ENABLE_RENDERING
    // Client-only initialization
    InitializeGraphics();
#endif

#ifdef ENABLE_PHYSICS
    // Client-only physics
    InitializePhysics();
#endif

#ifdef GAME_SERVER
    // Server-only initialization
    StartServer();
#else
    // Client-only initialization
    StartClient();
#endif

    // Main loop
    while (Running()) {
        ecs.progress();
    }

    return 0;
}
```

## Build Troubleshooting

### CMake Configuration Fails
**Error**: `CMake Error at CMakeLists.txt:25`

**Solution**: Clear the cache and rebuild:
```powershell
cd build
rm CMakeCache.txt -Force
rm CMakeFiles -Recurse -Force
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### Missing Dependencies
**Error**: `Cannot find msquic`, `flecs`, etc.

**Solution**: Ensure submodules are initialized:
```bash
git submodule update --init --recursive
```

### Build Takes Too Long
The first build compiles all dependencies. Subsequent builds are much faster. You can parallelize:
```powershell
cmake --build . --config Release -- /m:4
```

## API Reference

### Core Systems
- **CoreSystems.h** - Initialize ECS with core game systems
- **MsQuicWrapper.h** - Network communication utilities
- **RtxgiBridge.h** - Ray tracing integration

### Usage
```cpp
#include "core/CoreSystems.h"
#include "net/MsQuicWrapper.h"

int main() {
    flecs::world ecs;
    InitializeCoreSystems(ecs);
    InitializeMsQuic();
    
    // Your game logic here
    
    return 0;
}
```

## Supported Platforms

| Platform | Architecture | Client | Server |
|----------|--------------|--------|--------|
| Windows  | x64          | ✅     | ✅     |
| Linux    | x64          | ❌     | ✅     |
| macOS    | x64/Arm64    | 🔄     | 🔄     |

✅ = Fully supported | ❌ = Not supported | 🔄 = In progress

## Performance Optimization

### Release Build
Always build in Release mode for optimal performance:
```powershell
cmake --build . --config Release
```

### Interprocedural Optimization (LTO)
Enabled by default. For faster compilation, disable in CMakeLists.txt:
```cmake
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "")
```

## Contributing

1. Create a feature branch: `git checkout -b feature/your-feature`
2. Make your changes and commit: `git commit -am 'Add feature'`
3. Push to the branch: `git push origin feature/your-feature`
4. Submit a pull request

## License

See `LICENSE` file for details.

## Support

For issues, bugs, or questions:
1. Check existing issues on GitHub
2. Provide a minimal reproduction case
3. Include platform, compiler version, and build logs

## Credits

- **Jolt Physics** - https://github.com/jrouwe/JoltPhysics
- **Flecs** - https://github.com/SanderMertens/flecs
- **bgfx** - https://github.com/bkaradzic/bgfx
- **MsQuic** - https://github.com/microsoft/msquic
- **RTXGI** - https://github.com/NVIDIA-RTX/RTXGI
