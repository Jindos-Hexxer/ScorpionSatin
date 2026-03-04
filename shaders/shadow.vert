#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shader_viewport_layer_array : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;

layout(set = 0, binding = 0) uniform SceneUBO {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec4 cameraPos;
    vec4 sunDirection;
    vec4 sunColor;
    vec4 skyTint;
    int skyCubemapIdx;
    int skyPad0, skyPad1, skyPad2;
    vec4 cascadeSplits;
    int numCascades;
    int shadowPad0, shadowPad1, shadowPad2;
    mat4 lightViewProj[4];
} ubo;

layout(push_constant) uniform PushConstants {
    uint cascade_index;
    mat4 model;
} pc;

void main() {
    uint ci = min(pc.cascade_index, 3u);
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    gl_Position = ubo.lightViewProj[ci] * worldPos;
    gl_Layer = int(ci);
}
