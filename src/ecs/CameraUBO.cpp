#include "Camera.h"

namespace Engine {

bool FillGlobalUBOFromWorld(flecs::world& world, GlobalUBO& ubo) {
    const ActiveCamera* active = world.try_get<ActiveCamera>();
    if (!active || !active->entity.is_alive())
        return false;

    const CameraData* cam_data = active->entity.try_get<CameraData>();
    const Transform* cam_transform = active->entity.try_get<Transform>();
    if (!cam_data || !cam_transform)
        return false;

    ubo.view = cam_data->view;
    ubo.proj = cam_data->projection;
    ubo.viewProj = cam_data->projection * cam_data->view;
    ubo.cameraPos = glm::vec4(cam_transform->position, 1.0f);
    return true;
}

} // namespace Engine
