# ECS (Flecs)

Flecs is the central orchestration layer of the engine. All game state lives in a `flecs::world`, and all logic is expressed as Flecs systems. This makes Flecs the "glue" that bridges collision, rendering, networking, and gameplay.

## Role in the Architecture

- The `flecs::world` instance is owned by the **Engine** (not the Game DLL), which enables [hot-reloading](build-and-hotreload.md) without losing state.
- Game DLLs register their systems and component types against this world at load time.
- The server runs the same `flecs::world` in [headless mode](server-headless.md) -- identical logic, no rendering systems.

## Core Components

Components are plain data structs registered with Flecs:

| Component | Fields | Purpose |
| :--- | :--- | :--- |
| `Position` | `float x, y, z` | World-space position. Updated by [NavMesh movement](navmesh.md). |
| `Rotation` | `float qx, qy, qz, qw` | Quaternion rotation. |
| `Transform` | `mat4 model` | Computed model matrix for rendering. |
| `Velocity` | `float vx, vy, vz` | Linear velocity. |
| `CollisionRadius` | `float value` | Sphere radius for [entity collision checks](physics.md). |
| `GridCell` | `int32_t cx, cz` | Current Spatial Grid cell. Updated by Grid Update System. |
| `Visible` | (tag) | Tag component. Present = entity is rendered. |
| `Hidden` | (tag) | Tag component. Present = entity is culled. |
| `MeshRef` | `uint32_t mesh_id` | Index into the global mesh registry. |
| `MaterialRef` | `uint32_t material_id` | Index into the bindless material table. |
| `NetworkSync` | `uint64_t net_id` | Networked entity identifier for [MsQuic sync](networking.md). |
| `VisibilityHierarchy` | `uint64_t immediate_mask, view_mask, horizon_mask` | Tiered bitmask for [interest management](scaling.md). |

<!-- TODO: Define component structs in code and keep this table in sync -->

## Core Systems

Systems run each frame during `flecs::world::progress()`. Execution order is controlled by Flecs phases.

### Spatial Grid Update System (Phase: `OnUpdate`)

Rebuilds the [Spatial Grid](physics.md) each tick. Iterates all entities with `Position` and assigns them to grid cells. This is O(n) and enables O(1) neighbor queries for entity collision.

### NavMesh Movement System (Phase: `OnUpdate`)

Applies movement input to entities using `dtNavMeshQuery::moveAlongSurface`. The NavMesh acts as the physical floor/walls -- movement automatically slides along edges and stops at boundaries. Updates `Position` directly. See [NavMesh](navmesh.md).

### Collision Check System (Phase: `OnUpdate`)

Queries the Spatial Grid for neighboring entities and performs sphere/AABB overlap checks. Fires collision events (e.g., `OnHit`, `OnEnterTrigger`) that game logic systems can observe. See [Collision & Spatial Logic](physics.md).

### Transform Build System (Phase: `PostUpdate`)

Combines `Position` + `Rotation` + `Scale` into the `Transform.model` matrix for each entity.

### Culling System (Phase: `PostUpdate`)

Performs frustum culling against the camera. Adds the `Visible` tag to entities inside the frustum, removes it (or adds `Hidden`) for entities outside. Only `Visible` entities are written to the GPU instance buffer.

### GPU Upload System (Phase: `PreStore`)

Iterates all entities with `Transform` + `Visible`. Writes their `Transform.model` matrix (and `material_id`) into the Global Instance Buffer (Vulkan SSBO). The [Renderer](renderer.md) consumes this buffer during the G-Buffer and Shadow passes.

### Visibility Update System (Phase: `OnUpdate`)

Updates the `VisibilityHierarchy` bitmasks using dirty-flag throttling. The immediate mask updates on every cell crossing; view and horizon masks only update when the player moves significant distance. See [Scaling & Interest Management](scaling.md).

### Network Receive System (Phase: `PreUpdate`)

Processes incoming [MsQuic](networking.md) messages and applies remote state updates to entities with `NetworkSync`.

### Network Send System (Phase: `PostUpdate`)

Serializes dirty `Position`/`Rotation` for entities with `NetworkSync` and enqueues them for transmission.

## Execution Order

```text
PreUpdate    --> Network Receive (apply remote state)
OnUpdate     --> Spatial Grid Update (assign entities to cells)
OnUpdate     --> NavMesh Movement (moveAlongSurface -> Position)
OnUpdate     --> Collision Checks (sphere/AABB vs neighboring cells)
OnUpdate     --> Visibility Update (dirty-flag bitmask recalc)
OnUpdate     --> Game Logic Systems (from Game DLL)
PostUpdate   --> Transform Build (Position+Rotation -> mat4)
PostUpdate   --> Culling (frustum test, tag Visible/Hidden)
PostUpdate   --> Network Send (serialize dirty state)
PreStore     --> GPU Upload (Visible transforms -> SSBO)
```

## Game DLL Integration

The Game DLL registers its own systems (AI, abilities, quest logic, etc.) during initialization. These systems run in the `OnUpdate` phase after the engine's collision systems, once network receive has applied the latest server state.

```cpp
// In Game DLL init
void RegisterSystems(flecs::world& world) {
    world.system<Position, Health>("DamageOverTime")
        .kind(flecs::OnUpdate)
        .each([](Position& pos, Health& hp) {
            // game-specific logic
        });
}
```

## Related Documents

- [Renderer](renderer.md) -- GPU Upload System feeds the instance buffer consumed by render passes.
- [Collision & Spatial Logic](physics.md) -- Spatial Grid, NavMesh movement, and collision check systems.
- [Networking](networking.md) -- Network systems serialize/deserialize entity state.
- [Server / Headless](server-headless.md) -- Same world, no GPU Upload or Culling systems.
- [Scaling & Interest Management](scaling.md) -- VisibilityHierarchy component and visibility update system.
- [Build System & Hot-Reload](build-and-hotreload.md) -- World ownership enables DLL swapping.
