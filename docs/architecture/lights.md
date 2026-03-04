# Lighting

Directional (sun) and skylight with cascaded shadow maps and optional RT probe sky fill.

## ECS Components

Lights are entities with a light component and, for the sun, a `Transform` whose rotation sets direction.

- **DirectionalLight** (`include/Lights.h`): `color`, `intensity` (Lux), `cast_shadows`, `shadow_distance`, `shadow_cascades`.
- **SkyLight**: `cubemap_tex_idx` (bindless cubemap index, or -1 for fallback), `tint`, `intensity`.
- **ActiveSun** / **ActiveSky**: tag components. Exactly one entity with `ActiveSun` and one with `ActiveSky` are used each frame; their data is written into the scene UBO.

The Sun entity is created in core systems with a `Transform` (rotation sets light direction, e.g. forward = -Z) and `DirectionalLight`; an optional Sky entity has `SkyLight` and `ActiveSky`.

## GPU Data (GlobalUBO)

Scene uniforms (`include/SceneUniforms.h`) are in a single UBO at descriptor set 0, binding 0:

- **DirectionalLightGPU**: `direction` (xyz + intensity in w), `color` (xyz + cast_shadows in w).
- **SkyLightGPU**: `tint` (xyz + intensity in w), `cubemap_idx`.
- **Cascaded shadows**: `cascadeSplits` (vec4 of far distances per cascade), `numCascades`, `shadowCascades[4]` (each a `lightViewProj` matrix).

`FillGlobalUBOFromWorld` in `CameraUBO.cpp` fills this from the active camera and the entities with `ActiveSun` / `ActiveSky`. Cascade splits and per-cascade light view-projection matrices are computed from the camera frustum and sun direction.

## Shadow Pass (CSM)

Before the G-Buffer pass, the host can run a depth-only shadow pass for the sun:

- **CreateShadowResources(size)**: Allocates a 2D array depth image (one layer per cascade), render pass, framebuffer, depth-compare sampler, and pipeline layout. Updates the PBR descriptor set so binding 2 is the shadow map.
- **CreateShadowPipeline(vertModule, fragModule)**: Builds the shadow pipeline (same vertex layout as PBR, push constant: cascade index + model matrix). Use the compiled `shadow.vert` and `shadow.frag`.
- Each frame: begin the shadow render pass, bind shadow pipeline and the same PBR descriptor set (UBO already has cascade data), set viewport to the shadow size. For each cascade index and each mesh, call **CmdDrawMeshShadow(cmd, meshId, modelMatrix, cascadeIndex)**. End the pass; the shadow image is left in `SHADER_READ_ONLY_OPTIMAL` for the lighting pass.

The PBR fragment shader (binding 2 = `sampler2DArrayShadow`) selects a cascade from view distance vs. `cascadeSplits`, transforms the fragment to light space with `lightViewProj[cascade]`, and applies a small PCF kernel. Direct sunlight is multiplied by this shadow attenuation when `cast_shadows` is set and `numCascades > 0`.

## Skylight (Ambient and IBL)

- **No cubemap** (`cubemap_idx < 0`): The PBR shader uses a gradient ambient term (tint × intensity) based on the normal’s up component.
- **With cubemap**: Binding 4 holds the bindless cubemap array. The PBR shader samples the cubemap for specular IBL (reflection vector, roughness-based LOD) and can add diffuse ambient from the same or a separate term.

Cubemaps are registered via `RenderDevice::RegisterBindlessCubemap`. The Sky entity’s `cubemap_tex_idx` is written into the UBO.

## RTXGI Miss Shader

When RTXGI probe rays miss geometry, the ray-tracing miss shader should return the skylight color so indirect lighting sees the sky. The source shader `shaders/ray_miss.rmiss` uses descriptor set 0 (binding 0 = GlobalUBO, binding 4 = cubemaps). If `skyCubemapIdx >= 0`, it samples the cubemap by ray direction and applies tint × intensity; otherwise it returns a gradient. When the RTXGI Vulkan backend is wired, this miss shader is registered in the RT pipeline and the same descriptor set is bound so probe rays that miss hit this shader. See the comment in `RtxgiBridge.cpp` for integration details.

## Pass Order Summary

1. Shadow pass (optional): depth-only, sun cascades → binding 2.
2. G-Buffer: geometry, no lighting.
3. RTXGI: probe update (miss shader = sky when ray misses).
4. Lighting: PBR pass uses UBO (sun, sky, cascades), shadow map (binding 2), and bindless textures/cubemaps (bindings 3, 4).
