#include "Camera.h"
#include "Lights.h"
#include "SceneUniforms.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace Engine {

namespace {

/** Compute cascade split distances (linear, from camera). */
void ComputeCascadeSplits(float nearPlane, float shadowDistance, int numCascades, glm::vec4& outSplits) {
    outSplits = glm::vec4(0.0f);
    if (numCascades <= 0 || shadowDistance <= nearPlane)
        return;
    const int n = std::min(numCascades, kMaxShadowCascades);
    for (int i = 0; i < n; ++i) {
        float t = (i + 1) / static_cast<float>(n);
        float logFar = nearPlane * std::pow(shadowDistance / nearPlane, t);
        float linFar = nearPlane + (shadowDistance - nearPlane) * t;
        outSplits[i] = 0.25f * logFar + 0.75f * linFar; // blend log and linear
    }
}

/** Unproject 8 NDC frustum corners (z=0 and z=1) to world space. */
void GetFrustumCornersWorld(const glm::mat4& invViewProj, glm::vec3* outCorners) {
    const float zSlices[2] = {0.0f, 1.0f};
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                glm::vec4 ndc(2.0f * x - 1.0f, 2.0f * y - 1.0f, zSlices[z], 1.0f);
                glm::vec4 w = invViewProj * ndc;
                outCorners[z * 4 + y * 2 + x] = glm::vec3(w) / w.w;
            }
        }
    }
}

/** Build orthographic light view-projection for a set of world-space frustum corners. */
glm::mat4 BuildLightViewProj(const glm::vec3& lightDir, const glm::vec3& cameraPos,
    const glm::vec3* corners, int numCorners) {
    glm::vec3 lightForward = glm::normalize(lightDir);
    glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    if (std::abs(glm::dot(lightForward, worldUp)) > 0.99f)
        worldUp = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 lightRight = glm::normalize(glm::cross(worldUp, lightForward));
    glm::vec3 lightUp = glm::cross(lightForward, lightRight);

    glm::mat4 lightView = glm::lookAt(cameraPos, cameraPos + lightForward, lightUp);

    float minX = 1e30f, maxX = -1e30f;
    float minY = 1e30f, maxY = -1e30f;
    float minZ = 1e30f, maxZ = -1e30f;
    for (int i = 0; i < numCorners; ++i) {
        glm::vec4 p = lightView * glm::vec4(corners[i], 1.0f);
        minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
        minZ = std::min(minZ, p.z); maxZ = std::max(maxZ, p.z);
    }
    float pad = 10.0f;
    minX -= pad; maxX += pad;
    minY -= pad; maxY += pad;
    minZ -= pad; maxZ += pad;
    glm::mat4 lightProj = glm::ortho(minX, maxX, minY, maxY, -maxZ, -minZ);
    return lightProj * lightView;
}

} // namespace

bool FillGlobalUBOFromWorld(flecs::world& world, GlobalUBO& ubo) {
    const ActiveCamera* active = world.try_get<ActiveCamera>();
    if (!active || !active->entity.is_alive())
        return false;

    const CameraData* cam_data = active->entity.try_get<CameraData>();
    const Transform* cam_transform = active->entity.try_get<Transform>();
    if (!cam_data || !cam_transform)
        return false;

    ubo.view = cam_data->view;
    ubo.proj = cam_data->projection;
    ubo.viewProj = cam_data->projection * cam_data->view;
    ubo.cameraPos = glm::vec4(cam_transform->position, 1.0f);

    // Sun: direction from Transform rotation (forward = -Z), intensity and color from DirectionalLight
    bool has_sun = false;
    world.query<Transform, DirectionalLight, ActiveSun>().each([&](const Transform& t, const DirectionalLight& l, const ActiveSun&) {
        glm::vec3 forward = t.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        ubo.sun.direction = glm::vec4(glm::normalize(forward), l.intensity);
        ubo.sun.color = glm::vec4(l.color, l.cast_shadows ? 1.0f : 0.0f);
        has_sun = true;
    });
    if (!has_sun) {
        ubo.sun.direction = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
        ubo.sun.color = glm::vec4(1.0f, 1.0f, 0.95f, 0.0f);
    }

    // Sky: tint, intensity, cubemap index
    bool has_sky = false;
    world.query<SkyLight, ActiveSky>().each([&](const SkyLight& s, const ActiveSky&) {
        ubo.sky.tint = glm::vec4(s.tint, s.intensity);
        ubo.sky.cubemap_idx = s.cubemap_tex_idx;
        has_sky = true;
    });
    if (!has_sky) {
        ubo.sky.tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        ubo.sky.cubemap_idx = -1;
    }

    // Cascaded shadow maps: only when we have a sun that casts shadows
    glm::vec3 sunDir(0.0f, 1.0f, 0.0f);
    float shadowDistance = 200.0f;
    int numCascades = 4;
    world.query<Transform, DirectionalLight, ActiveSun>().each([&](const Transform& t, const DirectionalLight& l, const ActiveSun&) {
        sunDir = glm::normalize(t.rotation * glm::vec3(0.0f, 0.0f, -1.0f));
        shadowDistance = l.shadow_distance;
        numCascades = std::min(std::max(1, l.shadow_cascades), kMaxShadowCascades);
    });

    if (ubo.sun.color.w > 0.5f && numCascades > 0 && shadowDistance > cam_data->near_plane) {
        ComputeCascadeSplits(cam_data->near_plane, shadowDistance, numCascades, ubo.cascadeSplits);
        ubo.numCascades = numCascades;

        float aspect = 16.0f / 9.0f;
        if (const ViewportSize* vp = world.try_get<ViewportSize>(); vp && vp->height > 0.0f)
            aspect = vp->width / vp->height;

        const glm::mat4& view = cam_data->view;
        const glm::vec3 camPos = cam_transform->position;

        float cascadeNear = cam_data->near_plane;
        for (int i = 0; i < numCascades; ++i) {
            float cascadeFar = ubo.cascadeSplits[i];
            glm::mat4 projCascade = glm::perspective(cam_data->fov, aspect, cascadeNear, cascadeFar);
            glm::mat4 invViewProj = glm::inverse(projCascade * view);
            glm::vec3 corners[8];
            GetFrustumCornersWorld(invViewProj, corners);
            ubo.shadowCascades[i].lightViewProj = BuildLightViewProj(-sunDir, camPos, corners, 8);
            cascadeNear = cascadeFar;
        }
    } else {
        ubo.numCascades = 0;
    }

    return true;
}

} // namespace Engine
