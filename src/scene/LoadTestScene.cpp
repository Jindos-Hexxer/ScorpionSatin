#include "LoadTestScene.h"
#include "Camera.h"
#include "Lights.h"
#include "PBRMaterial.h"
#include "Transform.h"
#include <flecs.h>
#include <glm/gtc/quaternion.hpp>
#include <string>

namespace Engine {

void LoadTestScene(
    flecs::world& world,
    std::function<uint32_t(const PBRMaterialData&)> registerMaterial,
    std::function<uint32_t(BuiltinMeshes)> getMeshId)
{
    // Remove default Sun/Sky so only test scene lights have ActiveSun/ActiveSky
    if (flecs::entity sun = world.lookup("Sun"); sun.id() != 0)
        sun.destruct();
    if (flecs::entity sky = world.lookup("Sky"); sky.id() != 0)
        sky.destruct();

    // ---------------------------------------------------------
    // 1. SETUP EDITOR CAMERA
    // ---------------------------------------------------------
    auto editor_cam = world.entity("EditorCamera")
        .set<Transform>({
            .position = glm::vec3(0.0f, 5.0f, 15.0f),
            .rotation = glm::quat(glm::vec3(glm::radians(-15.0f), 0.0f, 0.0f)),
            .scale = glm::vec3(1.0f),
        })
        .set<CameraData>({
            .fov = glm::radians(75.0f),
            .near_plane = 0.1f,
            .far_plane = 10000.0f,
        })
        .set<EditorCamera>({
            .move_speed = 20.0f,
            .look_sensitivity = 0.1f,
            .pitch = glm::radians(-15.0f),
            .yaw = 0.0f,
        });

    world.set<ActiveCamera>({.entity = editor_cam});

    // ---------------------------------------------------------
    // 2. SETUP LIGHTING (Sun & Sky)
    // ---------------------------------------------------------
    world.entity("DirectionalLight_Sun")
        .add<ActiveSun>()
        .set<Transform>({
            .position = glm::vec3(0.0f),
            .rotation = glm::quat(glm::vec3(glm::radians(-45.0f), glm::radians(45.0f), 0.0f)),
            .scale = glm::vec3(1.0f),
        })
        .set<DirectionalLight>({
            .color = glm::vec3(1.0f, 0.98f, 0.95f),
            .intensity = 100000.0f,
            .cast_shadows = true,
        });

    world.entity("SkyLight_Environment")
        .add<ActiveSky>()
        .set<SkyLight>({
            .cubemap_tex_idx = -1,
            .tint = glm::vec3(1.0f, 1.0f, 1.0f),
            .intensity = 1.0f,
        });

    // ---------------------------------------------------------
    // 3. SETUP GROUND PLANE
    // ---------------------------------------------------------
    uint32_t floor_mat_id = registerMaterial(PBRMaterialData{
        .base_color_factor = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f),
        .metallic_factor = 0.0f,
        .roughness_factor = 0.9f,
    });

    world.entity("GroundPlane")
        .set<Transform>({
            .position = glm::vec3(0.0f, 0.0f, 0.0f),
            .rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
            .scale = glm::vec3(100.0f, 1.0f, 100.0f),
        })
        .set<MeshRenderer>({
            .mesh_id = getMeshId(BuiltinMeshes::Plane),
            .material_id = floor_mat_id,
        });

    // ---------------------------------------------------------
    // 4. SETUP PBR TEST GRID (Spheres)
    // ---------------------------------------------------------
    const int grid_size = 5;
    const float spacing = 2.5f;
    const glm::vec3 start_pos(
        -(grid_size * spacing) / 2.0f,
        1.0f,
        -(grid_size * spacing) / 2.0f);

    for (int metallic_idx = 0; metallic_idx <= grid_size; ++metallic_idx) {
        for (int roughness_idx = 0; roughness_idx <= grid_size; ++roughness_idx) {
            float metallic = static_cast<float>(metallic_idx) / static_cast<float>(grid_size);
            float roughness = glm::clamp(static_cast<float>(roughness_idx) / static_cast<float>(grid_size), 0.05f, 1.0f);

            uint32_t mat_id = registerMaterial(PBRMaterialData{
                .base_color_factor = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
                .metallic_factor = metallic,
                .roughness_factor = roughness,
            });

            glm::vec3 pos = start_pos + glm::vec3(
                static_cast<float>(metallic_idx) * spacing,
                0.0f,
                static_cast<float>(roughness_idx) * spacing);

            std::string name = "Sphere_M" + std::to_string(static_cast<int>(metallic * 100))
                + "_R" + std::to_string(static_cast<int>(roughness * 100));

            world.entity(name.c_str())
                .set<Transform>({
                    .position = pos,
                    .rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                    .scale = glm::vec3(1.0f),
                })
                .set<MeshRenderer>({
                    .mesh_id = getMeshId(BuiltinMeshes::Sphere),
                    .material_id = mat_id,
                });
        }
    }

    // ---------------------------------------------------------
    // 5. HERO OBJECT (Player Placeholder)
    // ---------------------------------------------------------
    uint32_t gold_mat_id = registerMaterial(PBRMaterialData{
        .base_color_factor = glm::vec4(1.0f, 0.84f, 0.0f, 1.0f),
        .metallic_factor = 1.0f,
        .roughness_factor = 0.15f,
    });

    world.entity("Player_Placeholder")
        .set<Transform>({
            .position = glm::vec3(0.0f, 1.0f, 0.0f),
            .rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
            .scale = glm::vec3(1.0f, 2.0f, 1.0f),
        })
        .set<MeshRenderer>({
            .mesh_id = getMeshId(BuiltinMeshes::Cube),
            .material_id = gold_mat_id,
        });
}

} // namespace Engine
