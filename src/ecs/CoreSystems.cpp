// Minimal Flecs ECS core systems and camera module
#include "CoreSystems.h"
#include "Camera.h"
#include "Lights.h"
#include "PBRMaterial.h"
#include "Transform.h"
#include <flecs.h>
#include <glm/gtc/quaternion.hpp>

namespace {

void RegisterCameraModule(flecs::world& ecs) {
    ecs.component<Engine::Transform>();
    ecs.component<Engine::CameraData>();
    ecs.component<Engine::ActiveCamera>();
    ecs.component<Engine::InputState>();
    ecs.component<Engine::ViewportSize>();
    ecs.component<Engine::EditorCamera>();
    ecs.component<Engine::ThirdPersonCamera>();
    ecs.component<Engine::MeshRenderer>();

    ecs.ensure<Engine::ViewportSize>();
    ecs.ensure<Engine::ActiveCamera>();

    Engine::RegisterEditorCameraSystem(ecs);
    Engine::RegisterThirdPersonCameraSystem(ecs);
    Engine::RegisterCameraMatrixSystem(ecs);
}

void RegisterLightingModule(flecs::world& ecs) {
    ecs.component<Engine::DirectionalLight>();
    ecs.component<Engine::SkyLight>();
    ecs.component<Engine::ActiveSun>();
    ecs.component<Engine::ActiveSky>();

    ecs.entity("Sun")
        .add<Engine::ActiveSun>()
        .set<Engine::Transform>({
            .position = glm::vec3(0.0f),
            .rotation = glm::quat(glm::vec3(glm::radians(-45.0f), glm::radians(30.0f), 0.0f))
        })
        .set<Engine::DirectionalLight>({.intensity = 120000.0f});

    ecs.entity("Sky")
        .add<Engine::ActiveSky>()
        .set<Engine::SkyLight>({});
}

} // namespace

void InitializeCoreSystems(flecs::world& ecs) {
    RegisterCameraModule(ecs);
    RegisterLightingModule(ecs);
}
