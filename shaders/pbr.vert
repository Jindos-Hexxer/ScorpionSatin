#version 460
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec2 outUV;
layout(location = 2) out mat3 outTBN;

layout(set = 0, binding = 0) uniform SceneUBO {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec4 cameraPos;
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 model;
    uint material_id; // padding in C++: 4 bytes at offset 64
} pc;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    outWorldPos = worldPos.xyz;
    outUV = inTexCoord;

    vec3 T = normalize((pc.model * vec4(inTangent, 0.0)).xyz);
    vec3 N = normalize((pc.model * vec4(inNormal, 0.0)).xyz);
    vec3 B = cross(N, T);
    outTBN = mat3(T, B, N);

    gl_Position = ubo.viewProj * worldPos;
}
