#pragma once

#include <cstdint>

/**
 * Vulkan interfaces for the render pipeline.
 * RenderDevice, render graph passes, bindless descriptors.
 */

namespace Engine {

struct RendererConfig {
    uint32_t width = 1920;
    uint32_t height = 1080;
    bool enableRTXGI = true;
};

} // namespace Engine
