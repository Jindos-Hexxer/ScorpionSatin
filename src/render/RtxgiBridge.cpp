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

} // namespace Engine
