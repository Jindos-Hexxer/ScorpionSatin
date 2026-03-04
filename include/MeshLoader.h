#pragma once

#include "MeshPrimitives.h"
#include "PBRMaterial.h"
#include <string>
#include <vector>

namespace Engine {

/**
 * Result of loading a glTF file: meshes, PBR materials, and image paths for bindless texture upload.
 * For each mesh, meshMaterialIndices[i] is the material index (into materials) for meshes[i].
 * Material texture indices (base_color_tex_idx etc.) are glTF texture indices (0-based); remap to
 * bindless indices after loading images and calling RenderDevice::RegisterBindlessTexture.
 */
struct GltfLoadResult {
    std::vector<PrimitiveMesh> meshes;
    std::vector<PBRMaterialData> materials;
    std::vector<uint32_t> meshMaterialIndices;  // one per mesh
    std::vector<std::string> imagePaths;        // unique image paths for texture upload
};

/**
 * Load full glTF 2.0 file: meshes (with tangents), materials, and image paths.
 * Returns true if at least one mesh was loaded. Materials use glTF texture indices;
 * host should load images, register bindless textures, then remap material.*_tex_idx.
 */
bool LoadGltf(const char* path, GltfLoadResult& out);
inline bool LoadGltf(const std::string& path, GltfLoadResult& out) {
    return LoadGltf(path.c_str(), out);
}

/**
 * Load mesh(es) only from a GLTF 2.0 file (legacy). Each primitive becomes one PrimitiveMesh.
 * Returns true if at least one mesh was loaded; meshes are appended to outMeshes.
 */
bool LoadMeshesFromGltf(const char* path, std::vector<PrimitiveMesh>& outMeshes);
inline bool LoadMeshesFromGltf(const std::string& path, std::vector<PrimitiveMesh>& outMeshes) {
    return LoadMeshesFromGltf(path.c_str(), outMeshes);
}

} // namespace Engine
