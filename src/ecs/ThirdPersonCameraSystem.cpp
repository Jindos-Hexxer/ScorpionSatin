#include "Camera.h"
#include "Transform.h"
#include <algorithm>
#include <flecs.h>
#include <glm/gtc/quaternion.hpp>

namespace Engine {

static void ThirdPersonCameraSystemImpl(flecs::iter& it) {
    auto transforms = it.field<Transform>(0);
    auto tpCams = it.field<ThirdPersonCamera>(1);

    const InputState* input = it.world().try_get<InputState>();
    const float dt = static_cast<float>(it.delta_time());

    for (size_t i = 0; i < it.count(); ++i) {
        Transform& t = transforms[i];
        ThirdPersonCamera& tp = tpCams[i];

        if (!tp.target.is_alive())
            continue;

        const Transform* target_transform = tp.target.try_get<Transform>();
        if (!target_transform)
            continue;

        if (input) {
            if (input->scroll_delta != 0.0f) {
                tp.target_distance -= input->scroll_delta;
                tp.target_distance = std::clamp(tp.target_distance, tp.min_distance, tp.max_distance);
            }
        }

        tp.current_distance = std::lerp(tp.current_distance, tp.target_distance, 10.0f * dt);

        if (input && (input->left_mouse_down || input->right_mouse_down)) {
            tp.yaw -= input->mouse_delta.x * 0.005f;
            tp.pitch -= input->mouse_delta.y * 0.005f;
            tp.pitch = std::clamp(tp.pitch, glm::radians(-80.0f), glm::radians(80.0f));
        }

        if (input && input->right_mouse_down) {
            Transform* target_mut = tp.target.try_get_mut<Transform>();
            if (target_mut)
                target_mut->rotation = glm::quat(glm::vec3(0.0f, tp.yaw, 0.0f));
        }

        glm::vec3 focus_point = target_transform->position + tp.target_offset;
        glm::quat cam_rot = glm::quat(glm::vec3(tp.pitch, tp.yaw, 0.0f));
        glm::vec3 backward = cam_rot * glm::vec3(0.0f, 0.0f, 1.0f);
        glm::vec3 ideal_position = focus_point + (backward * tp.current_distance);

        t.position = ideal_position;
        t.rotation = cam_rot;
    }
}

void RegisterThirdPersonCameraSystem(flecs::world& world) {
    world.system<Transform, ThirdPersonCamera>("ThirdPersonCameraSystem")
        .run(ThirdPersonCameraSystemImpl);
}

} // namespace Engine
