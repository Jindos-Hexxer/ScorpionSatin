#include "Camera.h"

namespace Engine {

void UpdateInputState(flecs::world& world, const RawInput& raw) {
    InputState& state = world.ensure<InputState>();
    state.mouse_delta.x = raw.mouse_delta_x;
    state.mouse_delta.y = raw.mouse_delta_y;
    state.scroll_delta = raw.scroll_delta;
    state.right_mouse_down = raw.right_mouse_down;
    state.left_mouse_down = raw.left_mouse_down;
    state.key_W = raw.key_W;
    state.key_A = raw.key_A;
    state.key_S = raw.key_S;
    state.key_D = raw.key_D;
    state.key_Q = raw.key_Q;
    state.key_E = raw.key_E;
}

} // namespace Engine
