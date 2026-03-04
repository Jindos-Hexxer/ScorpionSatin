#pragma once

#ifdef ENGINE_RTXGI
#include "SceneUniforms.h"
#endif

namespace Engine {

/** Initialize RTXGI (probe grid, etc.). No-op if ENGINE_RTXGI is off. */
void InitializeRtxgi();

#ifdef ENGINE_RTXGI
/** Feed active camera data to RTXGI so lighting uses the same view. Call after UpdateSceneUBO each frame. */
void RtxgiSetCameraFromUBO(const GlobalUBO& ubo);
#endif

/** Per-frame RTXGI step. Call after the G-Buffer pass each frame as part of the default render path. Stub for now; will take command buffer and G-Buffer views when SDK is integrated. */
void RtxgiStepFrame();

} // namespace Engine
