#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in mat3 inTBN;

layout(set = 0, binding = 0) uniform SceneUBO {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec4 cameraPos;
} ubo;

struct PBRMaterialData {
    vec4 base_color_factor;
    vec4 emissive_factor;
    float metallic_factor;
    float roughness_factor;
    float alpha_cutoff;
    uint flags;
    int base_color_tex_idx;
    int normal_tex_idx;
    int metallic_roughness_tex_idx;
    int occlusion_tex_idx;
    int emissive_tex_idx;
    int pad0, pad1, pad2;
};

layout(set = 0, binding = 1) readonly buffer MaterialBuffer {
    PBRMaterialData materials[];
} materialData;

layout(set = 0, binding = 3) uniform sampler2D globalTextures[];

layout(push_constant) uniform PushConstants {
    mat4 model;
    uint material_id;
} pc;

layout(location = 0) out vec4 outAlbedo;
layout(location = 1) out vec4 outNormalRoughness;

void main() {
    PBRMaterialData mat = materialData.materials[pc.material_id];

    vec4 albedo = mat.base_color_factor;
    if (mat.base_color_tex_idx >= 0) {
        albedo *= texture(globalTextures[nonuniformEXT(mat.base_color_tex_idx)], inUV);
    }
    if (albedo.a < mat.alpha_cutoff) discard;

    float roughness = mat.roughness_factor;
    if (mat.metallic_roughness_tex_idx >= 0) {
        vec4 mrSample = texture(globalTextures[nonuniformEXT(mat.metallic_roughness_tex_idx)], inUV);
        roughness *= mrSample.g;
    }
    roughness = max(roughness, 0.04);

    vec3 N = normalize(inTBN[2]);
    if (mat.normal_tex_idx >= 0) {
        vec3 normalMap = texture(globalTextures[nonuniformEXT(mat.normal_tex_idx)], inUV).rgb;
        normalMap = normalMap * 2.0 - 1.0;
        N = normalize(inTBN * normalMap);
    }

    outAlbedo = vec4(albedo.rgb, 1.0);
    outNormalRoughness = vec4(N * 0.5 + 0.5, roughness);
}
