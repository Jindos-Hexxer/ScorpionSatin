// RTXGI miss shader: when a probe ray misses geometry, return Skylight color.
// Bind same descriptor set as PBR (set 0: UBO binding 0, cubemaps binding 4).
// When RTXGI Vulkan backend is wired, register this shader in the RT pipeline.
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) rayPayloadInEXT vec3 hitValue;

layout(set = 0, binding = 0) uniform GlobalSceneUBO {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec4 cameraPos;
    vec4 sunDirection;
    vec4 sunColor;
    vec4 skyTint;
    int skyCubemapIdx;
    int skyPad0, skyPad1, skyPad2;
} global;

layout(set = 0, binding = 4) uniform samplerCube globalCubemaps[];

void main() {
    vec3 rayDir = gl_WorldRayDirectionEXT;

    if (global.skyCubemapIdx >= 0) {
        vec3 skyColor = texture(globalCubemaps[nonuniformEXT(global.skyCubemapIdx)], rayDir).rgb;
        skyColor *= global.skyTint.xyz * global.skyTint.w;
        hitValue = skyColor;
    } else {
        hitValue = mix(vec3(0.5, 0.7, 1.0), vec3(0.1, 0.2, 0.4), max(rayDir.y, 0.0));
    }
}
