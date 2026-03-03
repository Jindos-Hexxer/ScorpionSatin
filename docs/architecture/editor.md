# Editor (ImGui)

The editor is an ImGui-based development interface built directly into the engine. It provides a multi-window IDE-like experience for inspecting entities, tweaking values, and viewing the game world -- all without separate tooling.

## Design Principles

- The editor lives in the **Engine**, not the Game DLL. It is always available during development builds.
- The editor never touches game logic directly; it reads and writes Flecs components through the [ECS](ecs.md) world.
- In shipping builds, the editor can be compiled out entirely via a preprocessor define (`ENGINE_EDITOR=OFF`).

## ImGui Integration

### Docking

- Uses the ImGui **Docking** branch for a multi-window, tabbed layout.
- Default layout includes panels for Viewport, Entity Hierarchy, Inspector, Console, and Asset Browser.
- Layout is serializable to an `imgui.ini` file so it persists between sessions.

### Vulkan Backend

- ImGui is initialized with the Vulkan backend (`imgui_impl_vulkan`).
- Rendering happens in the [UI Pass](renderer.md) as the final render graph pass, drawn on top of the composited frame.
- ImGui uses its own descriptor pool and pipeline, separate from the game's bindless set.

## Panels

### Viewport

- The game world is rendered to an offscreen `VkImage` (the output of the Lighting/Composition pass).
- This image is displayed inside an `ImGui::Image()` widget, giving a resizable viewport.
- Mouse input inside the viewport is forwarded to the game camera and selection systems.
- Gizmos (translate, rotate, scale) are drawn over the viewport for selected entities.

### Entity Hierarchy

- Lists all entities from the `flecs::world` using `world.each()`.
- Displays parent-child relationships as a tree.
- Selecting an entity opens it in the Inspector panel.
- Supports search/filter by entity name or component type.

### Entity Inspector

- For the selected entity, iterates its components using Flecs reflection (`entity.each()`).
- Exposes component fields as ImGui widgets:
  - `Position` / `Rotation` / `Scale` -> float sliders.
  - `Health`, `Mana` -> progress bars and input fields.
  - `MeshRef`, `MaterialRef` -> dropdown selectors.
  - `CollisionRadius` -> float slider for entity collision sphere size.
  - `GridCell` -> read-only display of current Spatial Grid cell.
- Changes written through the inspector are applied directly to the Flecs component, taking effect next frame.

### Console

- Displays engine log output (info, warnings, errors).
- Supports command input for runtime debugging (e.g., spawning entities, toggling systems).

### Asset Browser

- Browses the `/assets` directory for models, textures, and shaders.
- Drag-and-drop from the asset browser onto the viewport or inspector to assign assets to entities.

<!-- TODO: Define the asset metadata format and import pipeline -->

## NavMesh Visualization

In editor mode, the [NavMesh](navmesh.md) can be overlaid on the viewport:
- Renders the navigation mesh as a wireframe on top of the world geometry.
- Highlights walkable vs non-walkable areas.
- Shows pathfinding debug lines when testing NPC routes.

## Editor-Only Systems

The editor registers its own Flecs systems that only run in editor builds:
- **Selection System** -- Tracks which entity is selected and draws highlight outlines.
- **Gizmo System** -- Reads input and modifies `Position`/`Rotation`/`Scale` for the selected entity.
- **Grid System** -- Renders a ground-plane grid in the viewport.

## Related Documents

- [Renderer](renderer.md) -- UI Pass where ImGui draws; viewport image comes from the Lighting pass.
- [ECS](ecs.md) -- All entity data read/written through Flecs components.
- [NavMesh](navmesh.md) -- NavMesh visualization and baking triggered from the editor.
- [Build System & Hot-Reload](build-and-hotreload.md) -- Editor acts as the host for Game DLL hot-reloading.
