// Mesh loading from GLTF 2.0 into Engine::PrimitiveMesh (same pipeline as primitives + UploadMesh)

#include "MeshLoader.h"
#include "MeshPrimitives.h"
#include <cstdio>
#include <cstring>

namespace Engine {

namespace {

// Minimal GLTF 2.0 parsing for a single mesh with POSITION, NORMAL, TEXCOORD_0.
// No external dependency: read file and parse JSON/binary by hand for the minimal subset.
// Returns true if one or more primitives were loaded into outMeshes.

bool parseGltfMinimal(const char* path, std::vector<PrimitiveMesh>& outMeshes) {
    (void)path;
    (void)outMeshes;
    // Stub: real implementation would use cgltf or similar to parse GLTF, then for each
    // mesh primitive: read accessors for POSITION, NORMAL, TEXCOORD_0, INDEX, fill Vertex
    // (position, normal, uv, tangent via computeTangents), and append to outMeshes.
    return false;
}

} // namespace

bool LoadMeshesFromGltf(const char* path, std::vector<PrimitiveMesh>& outMeshes) {
    if (!path || path[0] == '\0') {
        return false;
    }
    (void)outMeshes;
    FILE* f = std::fopen(path, "rb");
    if (!f) {
        return false;
    }
    std::fclose(f);
    // Full implementation: parse GLTF (e.g. via cgltf), fill Vertex arrays, call computeTangents, append to outMeshes.
    return parseGltfMinimal(path, outMeshes);
}

} // namespace Engine
