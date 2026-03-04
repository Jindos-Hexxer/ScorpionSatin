#pragma once

#include <cstdint>
#include <vector>

namespace Engine {

struct Vertex {
    float position[3];
    float normal[3];
    float uv[2];
    float tangent[3];  // For normal mapping; bitangent = cross(normal, tangent) * sign in shader
};

struct PrimitiveMesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

PrimitiveMesh Box(float hx, float hy, float hz);
PrimitiveMesh Plane(float width, float depth, int subdivW = 1, int subdivD = 1);
PrimitiveMesh Sphere(float radius, int segmentsLongitude, int segmentsLatitude);
PrimitiveMesh Cylinder(float radius, float halfHeight, int radialSegments);
PrimitiveMesh Cone(float radius, float height, int radialSegments);
PrimitiveMesh Capsule(float radius, float halfCylinderHeight, int radialSegments, int heightSegments);
PrimitiveMesh Torus(float majorRadius, float minorRadius, int majorSegments, int minorSegments);

/** Cached primitives (lazy-initialized, shared). Use for common shapes to avoid repeated generation. */
const PrimitiveMesh& UnitBox();
const PrimitiveMesh& UnitSphere();
const PrimitiveMesh& UnitQuad();

/** Compute vertex tangents from positions, normals, and UVs (e.g. after loading from file). */
void ComputeTangents(PrimitiveMesh& mesh);

} // namespace Engine
