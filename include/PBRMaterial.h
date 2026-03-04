#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace Engine {

/** Material flags for PBRMaterialData.flags (bitmask). */
enum PBRMaterialFlags : uint32_t {
    PBR_FLAG_NONE = 0,
    PBR_FLAG_DOUBLE_SIDED = 1u << 0,
    PBR_FLAG_ALPHA_BLEND = 1u << 1,
};

/**
 * PBR metallic-roughness material data. Align to 16 bytes for Vulkan SSBO/GLSL.
 * Layout must match shader struct (see shaders/pbr.frag or pbr_common.glsl).
 */
struct alignas(16) PBRMaterialData {
    glm::vec4 base_color_factor{1.0f};  // Base color + Alpha
    glm::vec4 emissive_factor{0.0f};    // RGB = Emissive, A = unused/intensity

    float metallic_factor = 1.0f;
    float roughness_factor = 1.0f;
    float alpha_cutoff = 0.5f;  // For masked materials (leaves, grass)
    uint32_t flags = 0;         // PBRMaterialFlags bitmask

    // Bindless texture indices (-1 means no texture)
    int32_t base_color_tex_idx = -1;
    int32_t normal_tex_idx = -1;
    int32_t metallic_roughness_tex_idx = -1;
    int32_t occlusion_tex_idx = -1;
    int32_t emissive_tex_idx = -1;

    // Padding to maintain 16-byte alignment
    int32_t pad0 = 0, pad1 = 0, pad2 = 0;
};

/** ECS component: index into global mesh and material buffers for rendering. */
struct MeshRenderer {
    uint32_t mesh_id = 0;     // Engine::MeshHandle (index into global vertex/index buffers)
    uint32_t material_id = 0; // Index into the global PBRMaterialData SSBO
};

} // namespace Engine
