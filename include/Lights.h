#pragma once

#include <glm/glm.hpp>
#include <cstdint>

namespace Engine {

/** Directional Light: Simulates the Sun / Moon. */
struct DirectionalLight {
    glm::vec3 color{1.0f, 1.0f, 0.95f};
    float intensity = 100000.0f;
    bool cast_shadows = true;

    float shadow_distance = 200.0f;
    int shadow_cascades = 4;
};

/** Skylight: HDRI / Cubemap environment fill. */
struct SkyLight {
    int32_t cubemap_tex_idx = -1;
    glm::vec3 tint{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
};

/** Tag: entity is the active directional sun light. */
struct ActiveSun {};

/** Tag: entity is the active skylight. */
struct ActiveSky {};

} // namespace Engine
