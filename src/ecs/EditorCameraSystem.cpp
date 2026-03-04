#include "Camera.h"
#include "Transform.h"
#include <algorithm>
#include <flecs.h>
#include <glm/gtc/quaternion.hpp>

namespace Engine {

static void EditorCameraSystemImpl(flecs::iter& it) {
    auto transforms = it.field<Transform>(0);
    auto editors = it.field<EditorCamera>(1);

    const InputState* input = it.world().try_get<InputState>();
    if (!input || !input->right_mouse_down)
        return;

    const float dt = static_cast<float>(it.delta_time());

    for (size_t i = 0; i < it.count(); ++i) {
        Transform& t = transforms[i];
        EditorCamera& ed = editors[i];

        ed.yaw -= input->mouse_delta.x * ed.look_sensitivity * dt;
        ed.pitch -= input->mouse_delta.y * ed.look_sensitivity * dt;
        ed.pitch = std::clamp(ed.pitch, glm::radians(-89.0f), glm::radians(89.0f));

        t.rotation = glm::quat(glm::vec3(ed.pitch, ed.yaw, 0.0f));

        glm::vec3 forward = t.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 right = t.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
        const glm::vec3 up(0.0f, 1.0f, 0.0f);

        glm::vec3 move_dir(0.0f);
        if (input->key_W) move_dir += forward;
        if (input->key_S) move_dir -= forward;
        if (input->key_D) move_dir += right;
        if (input->key_A) move_dir -= right;
        if (input->key_E) move_dir += up;
        if (input->key_Q) move_dir -= up;

        if (glm::length(move_dir) > 0.0f)
            t.position += glm::normalize(move_dir) * ed.move_speed * dt;
    }
}

void RegisterEditorCameraSystem(flecs::world& world) {
    world.system<Transform, EditorCamera>("EditorCameraSystem")
        .run(EditorCameraSystemImpl);
}

} // namespace Engine
