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
    vec4 sunDirection; // xyz = direction, w = intensity
    vec4 sunColor;     // xyz = color, w = cast_shadows
    vec4 skyTint;      // xyz = tint, w = intensity
    int skyCubemapIdx;
    int skyPad0, skyPad1, skyPad2;
    vec4 cascadeSplits;
    int numCascades;
    int shadowPad0, shadowPad1, shadowPad2;
    mat4 lightViewProj[4];
} ubo;

layout(set = 0, binding = 2) uniform sampler2DArrayShadow shadowMap;

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
layout(set = 0, binding = 4) uniform samplerCube globalCubemaps[];

layout(push_constant) uniform PushConstants {
    mat4 model;
    uint material_id;
} pc;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;
const float MAX_REFLECTION_LOD = 8.0;
const float SHADOW_MAP_SIZE = 2048.0;

float sampleCascadeShadow(int cascade, vec4 lightSpacePos) {
    vec3 ndc = lightSpacePos.xyz / lightSpacePos.w;
    if (ndc.z < 0.0 || ndc.z > 1.0 || ndc.x < 0.0 || ndc.x > 1.0 || ndc.y < 0.0 || ndc.y > 1.0)
        return 1.0;
    vec2 uv = ndc.xy;
    float refZ = ndc.z;
    float texelSize = 1.0 / SHADOW_MAP_SIZE;
    float sum = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            sum += texture(shadowMap, vec4(uv + vec2(x, y) * texelSize, float(cascade), refZ));
        }
    }
    return sum / 9.0;
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    denom = PI * denom * denom;
    if (denom <= 0.0) return 0.0;
    return a2 / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    PBRMaterialData mat = materialData.materials[pc.material_id];

    vec4 albedo = mat.base_color_factor;
    if (mat.base_color_tex_idx >= 0) {
        albedo *= texture(globalTextures[nonuniformEXT(mat.base_color_tex_idx)], inUV);
    }
    if (albedo.a < mat.alpha_cutoff) discard;

    float metallic = mat.metallic_factor;
    float roughness = mat.roughness_factor;
    if (mat.metallic_roughness_tex_idx >= 0) {
        vec4 mrSample = texture(globalTextures[nonuniformEXT(mat.metallic_roughness_tex_idx)], inUV);
        roughness *= mrSample.g;
        metallic *= mrSample.b;
    }
    roughness = max(roughness, 0.04);

    vec3 N = normalize(inTBN[2]);
    if (mat.normal_tex_idx >= 0) {
        vec3 normalMap = texture(globalTextures[nonuniformEXT(mat.normal_tex_idx)], inUV).rgb;
        normalMap = normalMap * 2.0 - 1.0;
        N = normalize(inTBN * normalMap);
    }

    vec3 V = normalize(ubo.cameraPos.xyz - inWorldPos);
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo.rgb, metallic);

    vec3 Lo = vec3(0.0);

    // Directional sun light (direction to light = -sunDirection.xyz)
    vec3 L = -normalize(ubo.sunDirection.xyz);
    float sunIntensity = ubo.sunDirection.w;
    float shadowAttenuation = 1.0;
    if (ubo.sunColor.w > 0.5 && ubo.numCascades > 0) {
        float viewDist = length(ubo.cameraPos.xyz - inWorldPos);
        int cascade = ubo.numCascades - 1;
        for (int i = 0; i < ubo.numCascades; ++i) {
            if (viewDist <= ubo.cascadeSplits[i]) {
                cascade = i;
                break;
            }
        }
        vec4 lightSpacePos = ubo.lightViewProj[cascade] * vec4(inWorldPos, 1.0);
        shadowAttenuation = sampleCascadeShadow(cascade, lightSpacePos);
    }
    vec3 H = normalize(V + L);
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;
    float NdotL = max(dot(N, L), 0.0);
    Lo += (kD * albedo.rgb / PI + specular) * NdotL * ubo.sunColor.xyz * sunIntensity * shadowAttenuation;

    // Skylight: ambient fallback when no cubemap, or specular IBL when cubemap is set
    if (ubo.skyCubemapIdx < 0) {
        float up = max(N.y, 0.0);
        vec3 skyAmbient = mix(vec3(0.1, 0.2, 0.4), vec3(0.5, 0.7, 1.0), up);
        skyAmbient *= ubo.skyTint.xyz * ubo.skyTint.w;
        Lo += skyAmbient * (1.0 - metallic) * albedo.rgb / PI;
    } else {
        vec3 R = reflect(-V, N);
        float lod = roughness * MAX_REFLECTION_LOD;
        vec3 specularAmbient = textureLod(globalCubemaps[nonuniformEXT(ubo.skyCubemapIdx)], R, lod).rgb;
        specularAmbient *= ubo.skyTint.xyz * ubo.skyTint.w;
        Lo += specularAmbient * F;
        float up = max(N.y, 0.0);
        vec3 skyAmbient = mix(vec3(0.1, 0.2, 0.4), vec3(0.5, 0.7, 1.0), up);
        skyAmbient *= ubo.skyTint.xyz * ubo.skyTint.w;
        Lo += skyAmbient * (1.0 - metallic) * albedo.rgb / PI;
    }

    vec3 emissive = mat.emissive_factor.rgb;
    if (mat.emissive_tex_idx >= 0) {
        emissive *= texture(globalTextures[nonuniformEXT(mat.emissive_tex_idx)], inUV).rgb;
    }
    vec3 finalColor = Lo + emissive;

    finalColor = finalColor / (finalColor + vec3(1.0));
    finalColor = pow(finalColor, vec3(1.0 / 2.2));

    outColor = vec4(finalColor, albedo.a);
}
