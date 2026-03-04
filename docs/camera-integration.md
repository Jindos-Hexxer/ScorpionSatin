# Camera module – main loop integration

The camera module (ECS components + systems + GlobalUBO) is implemented and registered in `InitializeCoreSystems`. To use it from the Editor or any host:

## 1. Create world and run pipeline

- Create a `flecs::world`, call `InitializeCoreSystems(ecs)` (and optionally `Game_RegisterSystems(&ecs)`).
- Each frame, pass delta time into the pipeline, e.g. `ecs.progress(dt)` (or run the built-in pipeline with your `dt`).

## 2. Input

- At the **start** of each frame, gather raw input (mouse delta, scroll, buttons, W/A/S/D/Q/E) from GLFW (or your input layer).
- Fill `Engine::RawInput` and call `Engine::UpdateInputState(ecs, raw)` so the Editor and ThirdPerson camera systems read up-to-date input.

## 3. Viewport

- If you have a swapchain or window size, set the viewport singleton so the camera aspect ratio is correct:
  - `ecs.get_mut<Engine::ViewportSize>()->width = extent.width;`
  - `ecs.get_mut<Engine::ViewportSize>()->height = extent.height;`

## 4. Spawn cameras (once)

- **Editor camera**: create an entity with `Transform`, `CameraData`, and `EditorCamera`. Optionally set initial `Transform::position` and `EditorCamera::pitch`/`yaw`.
- **Third-person camera**: create a “player” entity with `Transform`. Create another entity with `Transform`, `CameraData`, and `ThirdPersonCamera`, and set `ThirdPersonCamera::target` to the player entity.
- Set the active camera: `ecs.get_mut<Engine::ActiveCamera>()->entity = editor_camera_entity;` (or the third-person camera entity).

## 5. Render

- After `ecs.progress(dt)` (so camera systems have run):
  - `Engine::GlobalUBO ubo{};`
  - If `Engine::FillGlobalUBOFromWorld(ecs, ubo)` returns true, call `renderDevice.UpdateSceneUBO(ubo)` (and, when `ENGINE_RTXGI` is on, `Engine::RtxgiSetCameraFromUBO(ubo)`).
- Ensure `RenderDevice::CreateSceneUBO()` was called once after `RenderDevice::Init()`.
- Use `RenderDevice::GetSceneUBOBuffer()` and `GetSceneUBOSize()` when building descriptor sets for the scene UBO.

## 6. Reverse-Z

- The camera projection is built for Reverse-Z by default (far = 0, near = 1). Use a depth format and compare op suitable for reverse-Z (e.g. `VK_COMPARE_OP_GREATER_OR_EQUAL`).
