#pragma once

#include "PBRMaterial.h"
#include <cstdint>
#include <functional>
#include <flecs.h>

namespace Engine {

/** Builtin mesh IDs. Host must upload UnitBox(), UnitSphere(), UnitQuad() in this order. */
enum class BuiltinMeshes : uint32_t {
    Cube = 0,
    Sphere = 1,
    Plane = 2,
};

/**
 * Populates the Flecs world with the test scene: editor camera, sun, skylight,
 * ground plane, 6x6 PBR sphere grid, and a gold hero cube.
 * Destroys default "Sun" and "Sky" entities if present so only test scene lights are active.
 *
 * @param world Flecs world (InitializeCoreSystems must have been called).
 * @param registerMaterial Callback that appends PBR material to SSBO and returns its index.
 * @param getMeshId Callback that returns MeshHandle for the given builtin mesh.
 */
void LoadTestScene(
    flecs::world& world,
    std::function<uint32_t(const PBRMaterialData&)> registerMaterial,
    std::function<uint32_t(BuiltinMeshes)> getMeshId);

} // namespace Engine
