#pragma once

#include <glm/glm.hpp>

namespace Engine {

/** Scene uniforms for Vulkan UBO: view, projection, viewProj, camera position. */
struct GlobalUBO {
    alignas(16) glm::mat4 view{1.0f};
    alignas(16) glm::mat4 proj{1.0f};
    alignas(16) glm::mat4 viewProj{1.0f};
    alignas(16) glm::vec4 cameraPos{0.0f, 0.0f, 0.0f, 1.0f};
};

} // namespace Engine
