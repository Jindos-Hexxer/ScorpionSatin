# Collision & Spatial Logic

The engine does not use a traditional physics engine (no rigid body solver, no GJK/EPA). Instead, collision is split into three lightweight strategies optimized for MMO server scalability:

- **Environmental Collision** -- NavMesh polygon containment and surface movement.
- **Entity Collision** -- Spatial Grid with sphere/cylinder overlap checks.
- **Line-of-Sight (LOS)** -- Detour NavMesh raycasting.

This approach uses orders of magnitude less CPU than a full physics simulation, enabling 2,000+ entities per core on a standard Linux VPS.

## Collision Strategy Overview

| Type | Method | Cost | Used For |
| :--- | :--- | :--- | :--- |
| Static (walls, floors) | `dtNavMeshQuery::moveAlongSurface` | Near-zero | Player/NPC movement against terrain and structures. |
| Dynamic (entity vs entity) | Spatial Grid + sphere checks | O(n) grid update, O(1) per query | Melee range, AoE hits, proximity triggers. |
| Line-of-Sight | `dtNavMeshQuery::raycast` | 2D polygon raycast | Spell targeting, aggro checks, pathing validation. |

## Environmental Collision (NavMesh)

The [NavMesh](navmesh.md) acts as the physical "floor and walls" of the world. No separate collision meshes are needed.

- **Movement:** `dtNavMeshQuery::moveAlongSurface` slides a point along the NavMesh, automatically stopping at edges (walls) and following slopes (floors). This replaces character controllers entirely.
- **Grounding:** An entity's Y position is always projected onto the NavMesh surface via `dtNavMeshQuery::getPolyHeight`, preventing floating or falling through the world.
- **Validation:** If a requested position is not on a valid NavMesh polygon (`findNearestPoly` returns a large distance), the movement is rejected.

This is nearly free in CPU terms -- it's polygon containment math, not mesh-vs-mesh intersection.

## Entity Collision (Spatial Grid)

For entity-vs-entity interactions (hitting a mob, AoE damage, proximity triggers), the engine uses a **Spatial Grid** integrated into [Flecs](ecs.md).

### Grid Structure

The world is divided into fixed-size cells (e.g., 10x10 units). Each cell tracks which entities occupy it. The grid is stored as a singleton Flecs component. This same grid doubles as the **cell-occupancy interest list** for [Scaling & Interest Management](scaling.md) -- no additional data structure needed.

```cpp
struct SpatialGrid {
    static constexpr int CELL_SIZE = 10;
    std::unordered_map<uint64_t, std::vector<flecs::entity>> cells;
};

// Cell key from world position
uint64_t CellKey(float x, float z) {
    int cx = (int)x / SpatialGrid::CELL_SIZE;
    int cz = (int)z / SpatialGrid::CELL_SIZE;
    return ((uint64_t)cx << 32) | (uint32_t)cz;
}
```

### Grid Update System (O(n))

Runs once per tick. Iterates all entities with `Position` and rebuilds the grid:

```cpp
world.system<Position>("UpdateSpatialGrid")
    .each([](flecs::entity e, Position& p) {
        int cx = (int)p.x / SpatialGrid::CELL_SIZE;
        int cz = (int)p.z / SpatialGrid::CELL_SIZE;
        // Insert entity into the cell; remove from old cell if changed.
    });
```

### Collision Query (O(1) per lookup)

To check for hits, query only the same cell and its 8 neighbors (9 cells total). This avoids O(n^2) all-pairs checks:

```cpp
world.system<Position, AttackRange>("CheckHits")
    .each([](flecs::entity e, Position& p, AttackRange& r) {
        auto targets = GetEntitiesInNeighboringCells(p);
        for (auto target : targets) {
            float dist = Distance(p, target.get<Position>());
            float combined = r.value + target.get<CollisionRadius>().value;
            if (dist < combined) {
                // HIT -- enqueue damage event
            }
        }
    });
```

### Collision Shapes

Entities use simple bounding volumes -- no mesh colliders:

| Shape | Use Case |
| :--- | :--- |
| **Sphere** | Default for mobs, players, projectiles. Cheapest distance check. |
| **Cylinder** | Tall entities (trees, pillars) where height matters. |
| **AABB** | Rectangular triggers (quest areas, zone boundaries). |

## Line-of-Sight (Detour Raycast)

LOS checks use `dtNavMeshQuery::raycast`, which tests a ray against the 2D NavMesh polygon edges. This is much cheaper than a 3D physics raycast through triangle meshes.

Used for:
- **Spell targeting:** Can the caster see the target? If the raycast hits a NavMesh edge before reaching the target, LOS is blocked.
- **Aggro checks:** NPCs only aggro players they can "see" (ray doesn't hit a wall polygon).
- **Pathing validation:** Quick check whether a straight-line path is clear.

<!-- TODO: Document height-based LOS checks for multi-floor environments (bridges, caves) -->

## Why Not a Physics Engine?

For a WoW-style MMO server:

| Concern | Physics Engine (Jolt, Bullet) | NavMesh + Spatial Grid |
| :--- | :--- | :--- |
| **CPU cost** | GJK/EPA solver per pair, broadphase + narrowphase | Simple distance math, polygon containment |
| **Memory** | High-poly collision meshes, body state | Compact NavMesh + flat grid |
| **Scalability** | ~200-500 entities/core | ~2,000+ entities/core |
| **Complexity** | Contact manifolds, sleep states, constraints | One grid, one NavMesh query object |
| **Environmental collision** | Mesh colliders for terrain | NavMesh (already needed for pathfinding) |

The NavMesh is already loaded for NPC pathfinding, so environmental collision comes essentially for free. The Spatial Grid adds minimal overhead.

## Client-Side Visual Physics

On the **client only**, lightweight visual effects (ragdolls, debris, particles hitting surfaces) can use a minimal physics library or custom impulse math. These are cosmetic and non-authoritative -- the server never processes them.

<!-- TODO: Decide on client-side visual physics approach (simple impulse system vs lightweight library) -->

## Related Documents

- [NavMesh](navmesh.md) -- Environmental collision source; `moveAlongSurface`, `raycast`, and `getPolyHeight`.
- [ECS](ecs.md) -- Spatial Grid systems and collision components live in Flecs.
- [Server / Headless](server-headless.md) -- Server-side authoritative collision.
- [Networking](networking.md) -- Movement validation results sent to clients.
- [Scaling & Interest Management](scaling.md) -- Spatial Grid doubles as the cell-occupancy interest list.
