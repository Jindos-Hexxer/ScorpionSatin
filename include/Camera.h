#pragma once

#include "Transform.h"
#include "SceneUniforms.h"
#include <flecs.h>
#include <glm/glm.hpp>

namespace Engine {

/** Camera projection and computed view/projection matrices. */
struct CameraData {
    float fov = glm::radians(75.0f);
    float near_plane = 0.1f;
    float far_plane = 10000.0f;
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
};

/** Singleton: entity used for rendering. */
struct ActiveCamera {
    flecs::entity entity;
};

/** Singleton: populated by host from OS input each frame. */
struct InputState {
    glm::vec2 mouse_delta{0.0f};
    float scroll_delta = 0.0f;
    bool right_mouse_down = false;
    bool left_mouse_down = false;
    bool key_W = false;
    bool key_A = false;
    bool key_S = false;
    bool key_D = false;
    bool key_Q = false;
    bool key_E = false;
};

/** UE5-style editor free-fly controller. Requires Transform + CameraData on same entity. */
struct EditorCamera {
    float move_speed = 10.0f;
    float look_sensitivity = 0.1f;
    float pitch = 0.0f;
    float yaw = 0.0f;
};

/** WoW-style third-person orbit controller. Requires Transform + CameraData; target must have Transform. */
struct ThirdPersonCamera {
    flecs::entity target;
    float target_distance = 10.0f;
    float current_distance = 10.0f;
    float min_distance = 1.0f;
    float max_distance = 30.0f;
    float pitch = glm::radians(20.0f);
    float yaw = 0.0f;
    glm::vec3 target_offset{0.0f, 1.5f, 0.0f};
};

/** Singleton: viewport size for aspect ratio. Set by host from swapchain or config. */
struct ViewportSize {
    float width = 1920.0f;
    float height = 1080.0f;
};

/** Raw input from host (GLFW/etc.) for UpdateInputState. */
struct RawInput {
    float mouse_delta_x = 0.0f;
    float mouse_delta_y = 0.0f;
    float scroll_delta = 0.0f;
    bool right_mouse_down = false;
    bool left_mouse_down = false;
    bool key_W = false;
    bool key_A = false;
    bool key_S = false;
    bool key_D = false;
    bool key_Q = false;
    bool key_E = false;
};

/** Copy RawInput into the world's InputState singleton. Call at frame start from main loop. */
void UpdateInputState(flecs::world& world, const RawInput& raw);

/** Register EditorCameraSystem on the world. Call from InitializeCoreSystems. */
void RegisterEditorCameraSystem(flecs::world& world);
/** Register ThirdPersonCameraSystem on the world. Call from InitializeCoreSystems. */
void RegisterThirdPersonCameraSystem(flecs::world& world);
/** Register CameraMatrixSystem on the world. Call from InitializeCoreSystems. */
void RegisterCameraMatrixSystem(flecs::world& world);

/** Fill GlobalUBO from the active camera. Returns true if active camera was valid and ubo was written. */
bool FillGlobalUBOFromWorld(flecs::world& world, GlobalUBO& ubo);

} // namespace Engine
