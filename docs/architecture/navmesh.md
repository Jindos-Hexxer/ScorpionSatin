# NavMesh (Recast/Detour)

Recast handles NavMesh generation (baking) from 3D geometry. Detour handles runtime pathfinding and queries against the baked mesh. Together they provide navigation for NPCs, movement validation for players, and serve as the engine's **environmental collision system** -- the NavMesh is the physical "floor and walls" of the world. See [Collision & Spatial Logic](physics.md) for how this fits into the overall collision strategy.

## Two-Phase Workflow

### Phase 1: Baking (Recast) -- Editor / Offline

NavMesh generation runs in the [Editor](editor.md) or as an offline build step:

1. **Input:** World geometry (static meshes, terrain) is fed to Recast as triangle soup.
2. **Voxelization:** Recast voxelizes the geometry into a heightfield.
3. **Region Building:** Walkable regions are identified based on agent parameters:
   - `agentHeight` -- Minimum clearance.
   - `agentRadius` -- Minimum width.
   - `agentMaxClimb` -- Maximum step height.
   - `agentMaxSlope` -- Maximum walkable slope angle.
4. **Mesh Generation:** Polygonal NavMesh is generated from walkable regions.
5. **Output:** Serialized to a `.bin` file in the assets directory.

The editor provides visualization of the baked NavMesh overlaid on the 3D viewport (wireframe, walkable area highlighting).

### Phase 2: Runtime Queries (Detour) -- Client & Server

At startup, the engine loads the `.bin` file into a `dtNavMesh` object. Runtime queries go through `dtNavMeshQuery`:

- **Pathfinding:** `findPath()` returns a corridor of polygons from start to goal.
- **String Pulling:** `findStraightPath()` converts the polygon corridor into a series of waypoints.
- **Point Queries:** `findNearestPoly()` snaps a world position to the nearest point on the NavMesh.
- **Movement:** `moveAlongSurface()` slides a position along the NavMesh, stopping at edges (walls) and following slopes. This is the primary movement function for all entities.
- **Height Query:** `getPolyHeight()` projects an entity's Y position onto the NavMesh surface, preventing floating or falling through the world.
- **Raycast:** `raycast()` tests line-of-sight on the NavMesh surface (2D polygon raycast, much cheaper than 3D mesh raycasting). Used for spell targeting, aggro checks, and pathing validation.

## Server-Side Usage

On the [headless server](server-headless.md), the NavMesh is the primary tool for:

### NPC Pathfinding

- AI systems query `dtNavMeshQuery::findPath()` to navigate NPCs between points.
- Paths are followed using `moveAlongSurface()` which handles wall sliding and edge clamping natively -- no physics character controller needed.
- Crowd simulation (Detour Crowd) can manage groups of NPCs to avoid local collisions.

### Player Movement Validation

Every movement packet received from a client is validated:
1. Check that the player's claimed position is on or near a valid NavMesh polygon (`findNearestPoly`).
2. Check that the path from previous position to new position doesn't cross non-walkable areas (`raycast`).
3. If validation fails, the server rejects the movement and sends a correction.

This prevents:
- Walking through walls.
- Flying or teleporting (positions not on NavMesh).
- Speed hacking (distance between ticks exceeds max velocity).

## Tiled NavMesh

For large MMO worlds, the NavMesh is split into tiles:
- Each tile covers a fixed-size area of the world.
- Tiles can be loaded/unloaded independently for streaming.
- Adjacent tiles are stitched together by Detour for seamless cross-tile pathfinding.
- Tiles align with the [zone boundaries](server-headless.md) used for server scaling.

<!-- TODO: Define tile size and coordinate system conventions -->

## Dynamic Obstacles

For runtime changes to navigation (e.g., a bridge being destroyed):
- Detour supports temporary obstacles via `dtTileCache`.
- Obstacles mark regions as non-walkable without a full rebake.
- Full rebake can be triggered from the editor for permanent geometry changes.

<!-- TODO: Document the dtTileCache integration and obstacle API -->

## Off-Mesh Connections

For navigation across gaps the mesh can't represent (ladders, jump pads, teleporters):
- Off-mesh connections are placed manually in the editor.
- Detour includes them in pathfinding results.
- The AI system recognizes off-mesh connections and triggers the appropriate animation/movement.

## Related Documents

- [Server / Headless](server-headless.md) -- NavMesh loaded at server startup for authoritative validation.
- [Collision & Spatial Logic](physics.md) -- NavMesh provides environmental collision; Spatial Grid handles entity-vs-entity.
- [Editor](editor.md) -- NavMesh baking and visualization in the editor viewport.
- [ECS](ecs.md) -- AI systems that consume pathfinding results are Flecs systems.
