# Renderer

Vulkan 1.4 rendering backend with deferred shading, RTXGI global illumination, and a bindless resource model.

## Vulkan Abstraction ("Render Device")

Instead of raw Vulkan calls throughout the codebase, all GPU interaction goes through a `RenderDevice` wrapper.

### Device Context

- Use `vk-bootstrap` for Instance, Physical Device, Logical Device, and Swapchain creation.
- Automatically selects a GPU with Vulkan 1.4 support and required extensions (ray tracing, dynamic rendering).
- Swapchain recreation handled internally on window resize.

### Memory Management

- All buffer and image allocations go through VMA (`VulkanMemoryAllocator`).
- Staging buffers for CPU-to-GPU uploads are pooled per frame to avoid allocation churn.
- GPU-only buffers (vertex, index, SSBO) are allocated with `VMA_MEMORY_USAGE_GPU_ONLY`.

### Bindless Descriptor Set

- Maintain one global descriptor set that holds all textures and storage buffers.
- Textures are registered at load time and referenced by index in shaders (`texture_id` in the material).
- Avoids per-draw descriptor binding; the entire scene draws with a single `vkCmdBindDescriptorSets` call.
- Storage buffers (instance transforms, material data) are also bound once globally.

## Render Graph

The frame is organized into sequential passes. Each pass declares its attachments and dependencies so barriers are inserted automatically.

### Pass Order

1. **Shadow Pass** -- Render depth-only from each light's perspective into a shadow atlas or cascaded shadow maps.
2. **G-Buffer Pass** -- Deferred geometry pass. Outputs:
   - Albedo (RGB)
   - World-space Normals
   - Depth
   - Material properties (Roughness, Metallic)
3. **RTXGI Pass** -- Update RTXGI probes using ray tracing. Reads G-Buffer depth/normals. Produces an irradiance volume for indirect lighting. Uses the NVIDIA RTXGI SDK Vulkan backend.
4. **Lighting / Composition Pass** -- Full-screen pass that combines:
   - Direct lighting (shadow-mapped point/spot/directional lights)
   - Indirect lighting (RTXGI irradiance)
   - Outputs the final HDR color buffer.
5. **UI Pass** -- Renders the ImGui editor/HUD on top of the final image. See [Editor](editor.md) for details.

### GPU Data Upload

Every frame, the [ECS Sync System](ecs.md) writes entity `Transform` data into a Global Instance Buffer (Vulkan SSBO). The vertex shader indexes into this buffer using `gl_InstanceIndex` to fetch the model matrix.

<!-- TODO: Define the exact SSBO layout struct (mat4 model, material_id, etc.) -->

## RTXGI Integration

- RTXGI is the default GI path. After `UpdateSceneUBO`, call `Engine::RtxgiSetCameraFromUBO(ubo)`. After the G-Buffer pass, call `Engine::RtxgiStepFrame()` so the SDK runs its update using camera and G-Buffer (albedo, normal, roughness, depth). See [Camera integration](../camera-integration.md) for the full render flow.
- Probes are placed in a 3D grid covering the playable area.
- Each frame, a subset of probes is updated via ray tracing (staggered updates to spread the cost).
- The irradiance and distance data are stored in texture atlases that the Lighting pass samples.
- Probe placement and density are configurable per-zone in the editor.

<!-- TODO: Document probe update scheduling and budget (rays per frame) -->

## Shader Pipeline

- All shaders are written in GLSL and compiled to SPIR-V at build time.
- Shader source lives in `/shaders`.
- Push constants are used for per-draw data (object index, pass-specific parameters).
- Specialization constants select shader variants (e.g., with/without RTXGI).

## Related Documents

- [ECS](ecs.md) -- Sync System that feeds transform data to the GPU.
- [Editor](editor.md) -- UI Pass rendering and viewport integration.
- [Build System & Hot-Reload](build-and-hotreload.md) -- Shader compilation in the build pipeline.
