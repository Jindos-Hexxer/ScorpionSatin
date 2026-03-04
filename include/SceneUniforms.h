#pragma once

#include <glm/glm.hpp>
#include <cstdint>

namespace Engine {

/** GPU layout for directional (sun) light in GlobalUBO. */
struct alignas(16) DirectionalLightGPU {
    glm::vec4 direction; // xyz = direction, w = intensity
    glm::vec4 color;     // xyz = color, w = cast_shadows (1 or 0)
};

/** GPU layout for skylight in GlobalUBO. */
struct alignas(16) SkyLightGPU {
    glm::vec4 tint{1.0f, 1.0f, 1.0f, 1.0f};
    int32_t cubemap_idx = -1;
    int32_t pad[3]{};
};

/** Max cascades for directional light shadow map. */
constexpr int kMaxShadowCascades = 4;

/** Cascade shadow map data in GlobalUBO. */
struct alignas(16) ShadowCascadeGPU {
    alignas(16) glm::mat4 lightViewProj{1.0f};
};

/** Scene uniforms for Vulkan UBO: view, projection, camera, sun, sky, CSM. */
struct GlobalUBO {
    alignas(16) glm::mat4 view{1.0f};
    alignas(16) glm::mat4 proj{1.0f};
    alignas(16) glm::mat4 viewProj{1.0f};
    alignas(16) glm::vec4 cameraPos{0.0f, 0.0f, 0.0f, 1.0f};
    DirectionalLightGPU sun{};
    SkyLightGPU sky{};
    // Cascaded shadow map: split depths (view-space Z, first 4 components used)
    alignas(16) glm::vec4 cascadeSplits{0.0f, 0.0f, 0.0f, 0.0f};
    int numCascades = 0;
    int shadowPad0 = 0, shadowPad1 = 0, shadowPad2 = 0;
    ShadowCascadeGPU shadowCascades[kMaxShadowCascades]{};
};

} // namespace Engine
