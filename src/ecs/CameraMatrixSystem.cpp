#include "Camera.h"
#include "Transform.h"
#include <flecs.h>
#include <glm/gtc/matrix_transform.hpp>

namespace Engine {

namespace {

glm::mat4 buildProjection(float fovY, float aspect, float nearZ, float farZ) {
    glm::mat4 p = glm::perspective(fovY, aspect, nearZ, farZ);
#ifdef ENGINE_REVERSE_Z
    p[2][2] = -p[2][2];
    p[3][2] = -p[3][2];
#endif
    return p;
}

} // namespace

static void CameraMatrixSystemImpl(flecs::iter& it) {
    auto transforms = it.field<Transform>(0);
    auto cameras = it.field<CameraData>(1);

    float aspect_ratio = 1920.0f / 1080.0f;
    if (const ViewportSize* vp = it.world().try_get<ViewportSize>(); vp && vp->height > 0.0f)
        aspect_ratio = vp->width / vp->height;

    for (size_t i = 0; i < it.count(); ++i) {
        const Transform& t = transforms[i];
        CameraData& cam = cameras[i];

        glm::vec3 forward = t.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 up = t.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
        cam.view = glm::lookAt(t.position, t.position + forward, up);

        cam.projection = buildProjection(cam.fov, aspect_ratio, cam.near_plane, cam.far_plane);
        cam.projection[1][1] *= -1.0f;
    }
}

void RegisterCameraMatrixSystem(flecs::world& world) {
    world.system<Transform, CameraData>("CameraMatrixSystem")
        .run(CameraMatrixSystemImpl);
}

} // namespace Engine
