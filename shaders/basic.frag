#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(vec3(0.5, 1.0, 0.3));
    float diff = max(dot(N, L), 0.0);
    outColor = vec4(vec3(0.2 + 0.8 * diff), 1.0);
}
