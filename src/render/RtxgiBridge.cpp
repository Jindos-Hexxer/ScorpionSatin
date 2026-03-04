#include "RtxgiBridge.h"

namespace Engine {

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
