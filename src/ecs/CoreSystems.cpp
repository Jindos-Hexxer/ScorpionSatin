// Minimal Flecs ECS core systems and camera module
#include "CoreSystems.h"
#include "Camera.h"
#include "Transform.h"
#include <flecs.h>

namespace {

void RegisterCameraModule(flecs::world& ecs) {
    ecs.component<Engine::Transform>();
    ecs.component<Engine::CameraData>();
    ecs.component<Engine::ActiveCamera>();
    ecs.component<Engine::InputState>();
    ecs.component<Engine::ViewportSize>();
    ecs.component<Engine::EditorCamera>();
    ecs.component<Engine::ThirdPersonCamera>();

    ecs.ensure<Engine::ViewportSize>();
    ecs.ensure<Engine::ActiveCamera>();

    Engine::RegisterEditorCameraSystem(ecs);
    Engine::RegisterThirdPersonCameraSystem(ecs);
    Engine::RegisterCameraMatrixSystem(ecs);
}

} // namespace

void InitializeCoreSystems(flecs::world& ecs) {
    RegisterCameraModule(ecs);
}
