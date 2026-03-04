#pragma once

#include "MeshPrimitives.h"
#include <string>
#include <vector>

namespace Engine {

/**
 * Load mesh(es) from a GLTF 2.0 file into the engine Vertex format (position, normal, uv, tangent).
 * Each mesh primitive in the file becomes one PrimitiveMesh. Use UploadMesh() to get a MeshHandle.
 * Returns true if at least one mesh was loaded; meshes are appended to outMeshes.
 */
bool LoadMeshesFromGltf(const char* path, std::vector<PrimitiveMesh>& outMeshes);
inline bool LoadMeshesFromGltf(const std::string& path, std::vector<PrimitiveMesh>& outMeshes) {
    return LoadMeshesFromGltf(path.c_str(), outMeshes);
}

} // namespace Engine
