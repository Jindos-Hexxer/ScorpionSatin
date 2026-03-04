// Mesh and material loading from glTF 2.0 (cgltf)

#include "MeshLoader.h"
#include "MeshPrimitives.h"
#include "PBRMaterial.h"
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
#include <cstring>
#include <string>
#include <vector>

namespace Engine {

namespace {

void copyVec3(float* dst, const cgltf_float* src) {
    dst[0] = static_cast<float>(src[0]);
    dst[1] = static_cast<float>(src[1]);
    dst[2] = static_cast<float>(src[2]);
}

void copyVec2(float* dst, const cgltf_float* src) {
    dst[0] = static_cast<float>(src[0]);
    dst[1] = static_cast<float>(src[1]);
}

void fillPBRMaterial(PBRMaterialData& out, const cgltf_material* mat,
    std::vector<std::string>& imagePaths, std::vector<const cgltf_image*>& imageOrder) {
    if (!mat) return;
    const cgltf_pbr_metallic_roughness* pbr = &mat->pbr_metallic_roughness;
    out.base_color_factor.r = pbr->base_color_factor[0];
    out.base_color_factor.g = pbr->base_color_factor[1];
    out.base_color_factor.b = pbr->base_color_factor[2];
    out.base_color_factor.a = pbr->base_color_factor[3];
    out.metallic_factor = static_cast<float>(pbr->metallic_factor);
    out.roughness_factor = static_cast<float>(pbr->roughness_factor);
    if (pbr->base_color_texture.texture && pbr->base_color_texture.texture->image) {
        const cgltf_image* img = pbr->base_color_texture.texture->image;
        uint32_t idx = UINT32_MAX;
        for (size_t i = 0; i < imageOrder.size(); ++i)
            if (imageOrder[i] == img) { idx = static_cast<uint32_t>(i); break; }
        if (idx == UINT32_MAX) {
            idx = static_cast<uint32_t>(imageOrder.size());
            imageOrder.push_back(img);
            imagePaths.push_back(img->uri ? img->uri : "");
        }
        out.base_color_tex_idx = static_cast<int32_t>(idx);
    }
    if (mat->normal_texture.texture && mat->normal_texture.texture->image) {
        const cgltf_image* img = mat->normal_texture.texture->image;
        uint32_t idx = UINT32_MAX;
        for (size_t i = 0; i < imageOrder.size(); ++i)
            if (imageOrder[i] == img) { idx = static_cast<uint32_t>(i); break; }
        if (idx == UINT32_MAX) {
            idx = static_cast<uint32_t>(imageOrder.size());
            imageOrder.push_back(img);
            imagePaths.push_back(img->uri ? img->uri : "");
        }
        out.normal_tex_idx = static_cast<int32_t>(idx);
    }
    if (pbr->metallic_roughness_texture.texture && pbr->metallic_roughness_texture.texture->image) {
        const cgltf_image* img = pbr->metallic_roughness_texture.texture->image;
        uint32_t idx = UINT32_MAX;
        for (size_t i = 0; i < imageOrder.size(); ++i)
            if (imageOrder[i] == img) { idx = static_cast<uint32_t>(i); break; }
        if (idx == UINT32_MAX) {
            idx = static_cast<uint32_t>(imageOrder.size());
            imageOrder.push_back(img);
            imagePaths.push_back(img->uri ? img->uri : "");
        }
        out.metallic_roughness_tex_idx = static_cast<int32_t>(idx);
    }
    if (mat->emissive_texture.texture && mat->emissive_texture.texture->image) {
        const cgltf_image* img = mat->emissive_texture.texture->image;
        uint32_t idx = UINT32_MAX;
        for (size_t i = 0; i < imageOrder.size(); ++i)
            if (imageOrder[i] == img) { idx = static_cast<uint32_t>(i); break; }
        if (idx == UINT32_MAX) {
            idx = static_cast<uint32_t>(imageOrder.size());
            imageOrder.push_back(img);
            imagePaths.push_back(img->uri ? img->uri : "");
        }
        out.emissive_tex_idx = static_cast<int32_t>(idx);
    }
    if (mat->occlusion_texture.texture && mat->occlusion_texture.texture->image) {
        const cgltf_image* img = mat->occlusion_texture.texture->image;
        uint32_t idx = UINT32_MAX;
        for (size_t i = 0; i < imageOrder.size(); ++i)
            if (imageOrder[i] == img) { idx = static_cast<uint32_t>(i); break; }
        if (idx == UINT32_MAX) {
            idx = static_cast<uint32_t>(imageOrder.size());
            imageOrder.push_back(img);
            imagePaths.push_back(img->uri ? img->uri : "");
        }
        out.occlusion_tex_idx = static_cast<int32_t>(idx);
    }
    out.emissive_factor.r = mat->emissive_factor[0];
    out.emissive_factor.g = mat->emissive_factor[1];
    out.emissive_factor.b = mat->emissive_factor[2];
    out.emissive_factor.a = 1.0f;
    out.alpha_cutoff = static_cast<float>(mat->alpha_cutoff);
    if (mat->alpha_mode == cgltf_alpha_mode_mask) { /* already have alpha_cutoff */ }
    if (mat->double_sided) out.flags |= PBR_FLAG_DOUBLE_SIDED;
    if (mat->alpha_mode == cgltf_alpha_mode_blend) out.flags |= PBR_FLAG_ALPHA_BLEND;
}

bool loadPrimitive(PrimitiveMesh& out, const cgltf_primitive* prim) {
    if (!prim || prim->type != cgltf_primitive_type_triangles) return false;
    float* positions = nullptr;
    float* normals = nullptr;
    float* uvs = nullptr;
    size_t vertexCount = 0;
    const cgltf_accessor* indexAccessor = prim->indices;

    for (size_t i = 0; i < prim->attributes_count; ++i) {
        const cgltf_attribute& attr = prim->attributes[i];
        if (attr.type == cgltf_attribute_type_position && attr.data->type == cgltf_type_vec3) {
            vertexCount = attr.data->count;
            positions = (float*)malloc(vertexCount * 3 * sizeof(float));
            cgltf_accessor_unpack_floats(attr.data, positions, vertexCount * 3);
        } else if (attr.type == cgltf_attribute_type_normal && attr.data->type == cgltf_type_vec3) {
            normals = (float*)malloc(attr.data->count * 3 * sizeof(float));
            cgltf_accessor_unpack_floats(attr.data, normals, attr.data->count * 3);
        } else if (attr.type == cgltf_attribute_type_texcoord && attr.index == 0 && attr.data->type == cgltf_type_vec2) {
            uvs = (float*)malloc(attr.data->count * 2 * sizeof(float));
            cgltf_accessor_unpack_floats(attr.data, uvs, attr.data->count * 2);
        }
    }
    if (!positions || vertexCount == 0) {
        free(positions);
        free(normals);
        free(uvs);
        return false;
    }
    if (!normals) {
        normals = (float*)calloc(vertexCount * 3, sizeof(float));
        for (size_t v = 0; v < vertexCount; ++v) normals[v * 3 + 1] = 1.0f;
    }
    if (!uvs) {
        uvs = (float*)calloc(vertexCount * 2, sizeof(float));
    }

    std::vector<uint32_t> indices;
    if (indexAccessor) {
        indices.resize(indexAccessor->count);
        cgltf_accessor_unpack_indices(indexAccessor, indices.data(), sizeof(uint32_t), indexAccessor->count);
    } else {
        for (size_t i = 0; i < vertexCount; ++i) indices.push_back(static_cast<uint32_t>(i));
    }

    out.vertices.resize(vertexCount);
    for (size_t v = 0; v < vertexCount; ++v) {
        copyVec3(out.vertices[v].position, positions + v * 3);
        copyVec3(out.vertices[v].normal, normals + v * 3);
        copyVec2(out.vertices[v].uv, uvs + v * 2);
        out.vertices[v].tangent[0] = 1.0f;
        out.vertices[v].tangent[1] = 0.0f;
        out.vertices[v].tangent[2] = 0.0f;
    }
    out.indices = std::move(indices);
    ComputeTangents(out);

    free(positions);
    free(normals);
    free(uvs);
    return true;
}

} // namespace

bool LoadGltf(const char* path, GltfLoadResult& out) {
    if (!path || path[0] == '\0') return false;
    cgltf_options options = {};
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&options, path, &data) != cgltf_result_success)
        return false;
    if (cgltf_load_buffers(&options, data, path) != cgltf_result_success) {
        cgltf_free(data);
        return false;
    }

    std::vector<const cgltf_image*> imageOrder;
    for (size_t mi = 0; mi < data->materials_count; ++mi) {
        PBRMaterialData mat = {};
        fillPBRMaterial(mat, &data->materials[mi], out.imagePaths, imageOrder);
        out.materials.push_back(mat);
    }
    if (out.materials.empty())
        out.materials.push_back(PBRMaterialData{});

    for (size_t ni = 0; ni < data->nodes_count; ++ni) {
        const cgltf_node* node = &data->nodes[ni];
        if (!node->mesh) continue;
        const cgltf_mesh* mesh = node->mesh;
        for (size_t pi = 0; pi < mesh->primitives_count; ++pi) {
            PrimitiveMesh prim;
            if (!loadPrimitive(prim, &mesh->primitives[pi])) continue;
            out.meshes.push_back(std::move(prim));
            uint32_t matIndex = 0;
            if (mesh->primitives[pi].material && data->materials_count > 0) {
                const cgltf_material* m = mesh->primitives[pi].material;
                for (size_t k = 0; k < data->materials_count; ++k)
                    if (&data->materials[k] == m) { matIndex = static_cast<uint32_t>(k); break; }
            }
            if (matIndex >= static_cast<uint32_t>(out.materials.size())) matIndex = 0;
            out.meshMaterialIndices.push_back(matIndex);
        }
    }

    cgltf_free(data);
    return !out.meshes.empty();
}

bool LoadMeshesFromGltf(const char* path, std::vector<PrimitiveMesh>& outMeshes) {
    GltfLoadResult result;
    if (!LoadGltf(path, result)) return false;
    for (PrimitiveMesh& m : result.meshes)
        outMeshes.push_back(std::move(m));
    return true;
}

} // namespace Engine
