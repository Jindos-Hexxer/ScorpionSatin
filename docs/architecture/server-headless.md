# Server / Headless Mode

The engine supports a headless mode for running authoritative game servers on Linux. In this mode, all rendering (Vulkan, RTXGI, ImGui) is stripped out, while Flecs, MsQuic, Recast/Detour, and the Spatial Grid remain fully active.

## Activation

The Engine DLL/SO detects headless mode via:
- **Launch flag:** `--headless` command-line argument.
- **Build flag:** `cmake -DENGINE_HEADLESS=ON` compiles out all graphics code at build time, producing a smaller binary.

When headless:
- **Flecs:** Runs exactly the same as the client. Same world, same systems (minus rendering systems).
- **NavMesh + Spatial Grid:** Full [collision and movement logic](physics.md). NavMesh handles environmental collision; Spatial Grid handles entity-vs-entity checks.
- **MsQuic:** Acts as the server listener, accepting client connections.
- **Vulkan / RTXGI / ImGui / GLFW:** Completely disabled / null-initialized. Not linked.

## Server Architecture

```text
┌──────────────────────────────────────────────────┐
│              GameServer.so                        │
│  (Game logic: AI, quests, abilities, etc.)        │
├──────────────────────────────────────────────────┤
│              Engine.so (Headless)                  │
│  ┌──────────┐ ┌──────────────┐ ┌──────────────┐  │
│  │  Flecs   │ │ Recast/Detour│ │   MsQuic     │  │
│  │  (ECS)   │ │  (NavMesh +  │ │  (Listener)  │  │
│  │          │ │  Collision)  │ │              │  │
│  └──────────┘ └──────────────┘ └──────────────┘  │
│  ┌──────────────────┐                             │
│  │  Spatial Grid    │                             │
│  │  (Entity Checks) │                             │
│  └──────────────────┘                             │
└──────────────────────────────────────────────────┘
```

## Server Entry Point

The server has its own entry point (no GLFW window, no ImGui):

```text
/Engine_Repo/
├── src/Runtime/
│   ├── DedicatedServer/   # Entry point for Linux (no GLFW/ImGui)
│   └── Network/           # MsQuic Server implementation
/GameA_Repo/
├── src/                   # Shared Game Logic (Systems/Components)
└── CMakeLists.txt         # Conditional build: Game.dll vs GameServer.so
```

The server main loop is simpler than the client:
1. `MsQuic::Poll()` -- Process incoming connections and messages.
2. `flecs::world::progress(dt)` -- Run all ECS systems (Spatial Grid update, NavMesh movement, collision checks, game logic, AI, network send).
3. Sleep until next tick.

No frame rendering, no swapchain present.

## The MMO Server Stack

| Layer | Library | Server Role |
| :--- | :--- | :--- |
| **Navigation** | Recast/Detour | NPC pathfinding, movement validation. |
| **State** | Flecs | NPC AI states, player coordinates, spells, buffs. |
| **Networking** | MsQuic | Handles thousands of encrypted QUIC streams. |
| **Collision** | NavMesh + Spatial Grid | Environmental collision (walls/floors) and entity-vs-entity checks. |

## Authoritative Simulation

The server is the authority on all game state:
- **Movement:** Player movement requests are validated against [NavMesh](navmesh.md) (`moveAlongSurface`, `findNearestPoly`) before being applied. See [Collision & Spatial Logic](physics.md).
- **Combat:** Damage calculations, cooldowns, and ability effects run server-side.
- **Spawning:** NPC spawn timers and loot tables are server-authoritative.
- Clients receive state corrections via [MsQuic unreliable datagrams](networking.md).

## Zone-Based Scaling

For large MMO worlds, the server is split into zones:
- Each zone runs as a separate Linux process with its own `flecs::world`.
- Zones communicate via MsQuic for cross-zone events (e.g., player transfers, global chat).
- A zone can be moved to a different machine for load balancing.
- Players crossing a zone boundary trigger a handoff sequence.
- Within each zone, the [Scaling & Interest Management](scaling.md) system handles 5,000+ players via tiered visibility bitmasks and packet batching.

<!-- TODO: Document the zone handoff protocol and state transfer format -->

## Linux Deployment Workflow

1. **Build:** `cmake -DENGINE_HEADLESS=ON .. && make`
2. **Artifacts:** `Engine.so` + `GameServer.so` + NavMesh `.bin` files.
3. **Run:** `./DedicatedServer --headless --zone=ElwynnForest --port=4433`
4. **Scale:** Deploy multiple instances for different zones behind a lobby/login server.

<!-- TODO: Document containerization strategy (Docker) and orchestration -->

## Related Documents

- [Networking](networking.md) -- MsQuic server listener and client connection handling.
- [NavMesh](navmesh.md) -- Loaded at startup for pathfinding and movement validation.
- [Collision & Spatial Logic](physics.md) -- NavMesh environmental collision and Spatial Grid entity checks on the server.
- [ECS](ecs.md) -- Same Flecs world as the client, minus rendering systems.
- [Build System & Hot-Reload](build-and-hotreload.md) -- CMake conditional builds for headless.
- [Scaling & Interest Management](scaling.md) -- 3-circle visibility, bitmask culling, and packet batching per zone.
