#include "RtxgiBridge.h"

namespace Engine {

// When the RTXGI Vulkan backend is integrated: register the ray miss shader so probe rays
// that miss geometry sample the Skylight. Miss shader path: shaders/ray_miss.rmiss (built to
// build/shaders/ray_miss_rmiss.spv). It expects descriptor set 0 (binding 0 = GlobalUBO,
// binding 4 = globalCubemaps). Bind the same descriptor set when tracing so the miss shader
// returns sky color from the active Skylight (cubemap or fallback gradient).

void InitializeRtxgi() {
}

#ifdef ENGINE_RTXGI
void RtxgiSetCameraFromUBO(const GlobalUBO& ubo) {
    (void)ubo;
    // TODO: Push ubo.cameraPos, ubo.view (or viewProj) to RTXGI SDK (e.g. probe grid / lighting view).
    // RTXGI sample uses m_view.GetViewOrigin() and view matrices; set equivalent state here.
}
#endif

void RtxgiStepFrame() {
    // TODO: Run RTXGI SDK update/resolve using camera and G-Buffer (albedo, normal, roughness, depth).
    // Call after G-Buffer pass; will take VkCommandBuffer and G-Buffer texture views when SDK is integrated.
}

} // namespace Engine
