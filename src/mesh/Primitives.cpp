#include "MeshPrimitives.h"
#include <cmath>

namespace Engine {

namespace {

constexpr float kPi = 3.14159265358979323846f;

void setVertex(Vertex& v, float px, float py, float pz, float nx, float ny, float nz, float u, float uv,
    float tx = 0.0f, float ty = 0.0f, float tz = 0.0f) {
    v.position[0] = px;
    v.position[1] = py;
    v.position[2] = pz;
    v.normal[0] = nx;
    v.normal[1] = ny;
    v.normal[2] = nz;
    v.uv[0] = u;
    v.uv[1] = uv;
    v.tangent[0] = tx;
    v.tangent[1] = ty;
    v.tangent[2] = tz;
}

void addQuad(std::vector<Vertex>& verts, std::vector<uint32_t>& idx,
    float x0, float y0, float z0, float x1, float y1, float z1,
    float x2, float y2, float z2, float x3, float y3, float z3,
    float nx, float ny, float nz, float u0, float v0, float u1, float v1) {
    uint32_t base = static_cast<uint32_t>(verts.size());
    Vertex a{}, b{}, c{}, d{};
    setVertex(a, x0, y0, z0, nx, ny, nz, u0, v0);
    setVertex(b, x1, y1, z1, nx, ny, nz, u1, v0);
    setVertex(c, x2, y2, z2, nx, ny, nz, u1, v1);
    setVertex(d, x3, y3, z3, nx, ny, nz, u0, v1);
    verts.push_back(a);
    verts.push_back(b);
    verts.push_back(c);
    verts.push_back(d);
    idx.push_back(base);
    idx.push_back(base + 1);
    idx.push_back(base + 2);
    idx.push_back(base);
    idx.push_back(base + 2);
    idx.push_back(base + 3);
}

} // namespace

void ComputeTangents(PrimitiveMesh& m) {
    if (m.vertices.size() < 3 || m.indices.size() < 3) return;
    std::vector<float> accTx(m.vertices.size(), 0.0f);
    std::vector<float> accTy(m.vertices.size(), 0.0f);
    std::vector<float> accTz(m.vertices.size(), 0.0f);
    for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
        uint32_t i0 = m.indices[i];
        uint32_t i1 = m.indices[i + 1];
        uint32_t i2 = m.indices[i + 2];
        Vertex& v0 = m.vertices[i0];
        Vertex& v1 = m.vertices[i1];
        Vertex& v2 = m.vertices[i2];
        float e1x = v1.position[0] - v0.position[0], e1y = v1.position[1] - v0.position[1], e1z = v1.position[2] - v0.position[2];
        float e2x = v2.position[0] - v0.position[0], e2y = v2.position[1] - v0.position[1], e2z = v2.position[2] - v0.position[2];
        float duv1x = v1.uv[0] - v0.uv[0], duv1y = v1.uv[1] - v0.uv[1];
        float duv2x = v2.uv[0] - v0.uv[0], duv2y = v2.uv[1] - v0.uv[1];
        float det = duv1x * duv2y - duv2x * duv1y;
        float r = (std::abs(det) > 1e-8f) ? (1.0f / det) : 0.0f;
        float tx = (e1x * duv2y - e2x * duv1y) * r;
        float ty = (e1y * duv2y - e2y * duv1y) * r;
        float tz = (e1z * duv2y - e2z * duv1y) * r;
        accTx[i0] += tx; accTy[i0] += ty; accTz[i0] += tz;
        accTx[i1] += tx; accTy[i1] += ty; accTz[i1] += tz;
        accTx[i2] += tx; accTy[i2] += ty; accTz[i2] += tz;
    }
    for (size_t v = 0; v < m.vertices.size(); ++v) {
        float tx = accTx[v], ty = accTy[v], tz = accTz[v];
        float len = std::sqrt(tx * tx + ty * ty + tz * tz);
        if (len > 1e-6f) {
            tx /= len; ty /= len; tz /= len;
            float nx = m.vertices[v].normal[0], ny = m.vertices[v].normal[1], nz = m.vertices[v].normal[2];
            float d = tx * nx + ty * ny + tz * nz;
            tx -= d * nx; ty -= d * ny; tz -= d * nz;
            len = std::sqrt(tx * tx + ty * ty + tz * tz);
            if (len > 1e-6f) { tx /= len; ty /= len; tz /= len; }
        }
        m.vertices[v].tangent[0] = tx;
        m.vertices[v].tangent[1] = ty;
        m.vertices[v].tangent[2] = tz;
    }
}

PrimitiveMesh Box(float hx, float hy, float hz) {
    PrimitiveMesh m;
    float x0 = -hx, x1 = hx, y0 = -hy, y1 = hy, z0 = -hz, z1 = hz;
    // -X
    addQuad(m.vertices, m.indices, x0, y0, z0, x0, y1, z0, x0, y1, z1, x0, y0, z1, -1, 0, 0, 0, 0, 1, 1);
    // +X
    addQuad(m.vertices, m.indices, x1, y0, z1, x1, y1, z1, x1, y1, z0, x1, y0, z0, 1, 0, 0, 0, 0, 1, 1);
    // -Y
    addQuad(m.vertices, m.indices, x0, y0, z1, x1, y0, z1, x1, y0, z0, x0, y0, z0, 0, -1, 0, 0, 0, 1, 1);
    // +Y
    addQuad(m.vertices, m.indices, x0, y1, z0, x1, y1, z0, x1, y1, z1, x0, y1, z1, 0, 1, 0, 0, 0, 1, 1);
    // -Z
    addQuad(m.vertices, m.indices, x1, y0, z0, x0, y0, z0, x0, y1, z0, x1, y1, z0, 0, 0, -1, 0, 0, 1, 1);
    // +Z
    addQuad(m.vertices, m.indices, x0, y0, z1, x1, y0, z1, x1, y1, z1, x0, y1, z1, 0, 0, 1, 0, 0, 1, 1);
    ComputeTangents(m);
    return m;
}

PrimitiveMesh Plane(float width, float depth, int subdivW, int subdivD) {
    PrimitiveMesh m;
    float hw = width * 0.5f;
    float hd = depth * 0.5f;
    int segW = subdivW > 0 ? subdivW : 1;
    int segD = subdivD > 0 ? subdivD : 1;
    for (int iz = 0; iz <= segD; ++iz) {
        float tz = static_cast<float>(iz) / static_cast<float>(segD);
        float z = -hd + tz * depth;
        float vz = 1.0f - tz;
        for (int ix = 0; ix <= segW; ++ix) {
            float tx = static_cast<float>(ix) / static_cast<float>(segW);
            float x = -hw + tx * width;
            Vertex v{};
            setVertex(v, x, 0.0f, z, 0.0f, 1.0f, 0.0f, tx, vz);
            m.vertices.push_back(v);
        }
    }
    for (int iz = 0; iz < segD; ++iz) {
        for (int ix = 0; ix < segW; ++ix) {
            uint32_t a = static_cast<uint32_t>(iz * (segW + 1) + ix);
            uint32_t b = a + 1;
            uint32_t c = a + (segW + 1);
            uint32_t d = c + 1;
            m.indices.push_back(a);
            m.indices.push_back(c);
            m.indices.push_back(b);
            m.indices.push_back(b);
            m.indices.push_back(c);
            m.indices.push_back(d);
        }
    }
    ComputeTangents(m);
    return m;
}

PrimitiveMesh Sphere(float radius, int segmentsLongitude, int segmentsLatitude) {
    PrimitiveMesh m;
    int lon = segmentsLongitude >= 3 ? segmentsLongitude : 3;
    int lat = segmentsLatitude >= 2 ? segmentsLatitude : 2;
    for (int iy = 0; iy <= lat; ++iy) {
        float ty = static_cast<float>(iy) / static_cast<float>(lat);
        float phi = ty * kPi;
        float y = std::cos(phi);
        float r = std::sin(phi);
        float v = 1.0f - ty;
        for (int ix = 0; ix <= lon; ++ix) {
            float tx = static_cast<float>(ix) / static_cast<float>(lon);
            float theta = tx * 2.0f * kPi;
            float x = r * std::cos(theta);
            float z = r * std::sin(theta);
            float nx = x;
            float ny = y;
            float nz = z;
            float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (len > 1e-6f) {
                nx /= len;
                ny /= len;
                nz /= len;
            }
            Vertex vtx{};
            setVertex(vtx, radius * x, radius * y, radius * z, nx, ny, nz, 1.0f - tx, v);
            m.vertices.push_back(vtx);
        }
    }
    for (int iy = 0; iy < lat; ++iy) {
        for (int ix = 0; ix < lon; ++ix) {
            uint32_t a = static_cast<uint32_t>(iy * (lon + 1) + ix);
            uint32_t b = a + 1;
            uint32_t c = a + (lon + 1);
            uint32_t d = c + 1;
            m.indices.push_back(a);
            m.indices.push_back(c);
            m.indices.push_back(b);
            m.indices.push_back(b);
            m.indices.push_back(c);
            m.indices.push_back(d);
        }
    }
    ComputeTangents(m);
    return m;
}

// Shared tapered cylinder: topRadius 0 => cone (single apex, one cap), else cylinder (two caps).
static PrimitiveMesh cylinderOrCone(float bottomRadius, float topRadius, float bottomY, float topY, int radialSegments) {
    PrimitiveMesh m;
    int seg = radialSegments >= 3 ? radialSegments : 3;
    const bool isCone = topRadius <= 0.0f;
    uint32_t topCenter = 0;
    uint32_t botCenter = 1;
    if (isCone) {
        Vertex apex{}, botCenterV{};
        float height = topY - bottomY;
        float coneLen = std::sqrt(bottomRadius * bottomRadius + height * height);
        float nxScale = bottomRadius / coneLen;
        float nyScale = -height / coneLen;
        setVertex(apex, 0.0f, topY, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f);
        setVertex(botCenterV, 0.0f, bottomY, 0.0f, 0.0f, -1.0f, 0.0f, 0.5f, 0.5f);
        m.vertices.push_back(apex);
        m.vertices.push_back(botCenterV);
        for (int i = 0; i <= seg; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(seg);
            float a = t * 2.0f * kPi;
            float x = bottomRadius * std::cos(a);
            float z = bottomRadius * std::sin(a);
            float sideNx = nxScale * std::cos(a);
            float sideNz = nxScale * std::sin(a);
            Vertex vApex{}, vBot{}, vSide{};
            setVertex(vApex, 0.0f, topY, 0.0f, sideNx, nyScale, sideNz, t, 0.0f);
            setVertex(vBot, x, bottomY, z, 0.0f, -1.0f, 0.0f, 0.5f + 0.5f * std::cos(a), 0.5f + 0.5f * std::sin(a));
            setVertex(vSide, x, bottomY, z, sideNx, nyScale, sideNz, t, 1.0f);
            m.vertices.push_back(vApex);
            m.vertices.push_back(vBot);
            m.vertices.push_back(vSide);
        }
        for (int i = 0; i < seg; ++i) {
            int o = 2 + i * 3;
            m.indices.push_back(static_cast<uint32_t>(o));
            m.indices.push_back(static_cast<uint32_t>(o + 1));
            m.indices.push_back(static_cast<uint32_t>(o + 4));
            m.indices.push_back(botCenter);
            m.indices.push_back(static_cast<uint32_t>(o + 1));
            m.indices.push_back(static_cast<uint32_t>(o + 4));
        }
        ComputeTangents(m);
        return m;
    }
    Vertex vTop{}, vBot{};
    setVertex(vTop, 0.0f, topY, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f);
    setVertex(vBot, 0.0f, bottomY, 0.0f, 0.0f, -1.0f, 0.0f, 0.5f, 0.5f);
    m.vertices.push_back(vTop);
    m.vertices.push_back(vBot);
    float radius = bottomRadius; // cylinder uses same radius
    for (int i = 0; i <= seg; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(seg);
        float a = t * 2.0f * kPi;
        float x = radius * std::cos(a);
        float z = radius * std::sin(a);
        float nx = std::cos(a);
        float nz = std::sin(a);
        Vertex vt{}, vb{}, vs0{}, vs1{};
        setVertex(vt, x, topY, z, 0.0f, 1.0f, 0.0f, 0.5f + 0.5f * std::cos(a), 0.5f + 0.5f * std::sin(a));
        setVertex(vb, x, bottomY, z, 0.0f, -1.0f, 0.0f, 0.5f + 0.5f * std::cos(a), 0.5f + 0.5f * std::sin(a));
        setVertex(vs0, x, topY, z, nx, 0.0f, nz, t, 1.0f);
        setVertex(vs1, x, bottomY, z, nx, 0.0f, nz, t, 0.0f);
        m.vertices.push_back(vt);
        m.vertices.push_back(vb);
        m.vertices.push_back(vs0);
        m.vertices.push_back(vs1);
    }
    for (int i = 0; i < seg; ++i) {
        int o = 2 + i * 4;
        m.indices.push_back(topCenter);
        m.indices.push_back(static_cast<uint32_t>(o));
        m.indices.push_back(static_cast<uint32_t>(o + 4));
        m.indices.push_back(botCenter);
        m.indices.push_back(static_cast<uint32_t>(o + 5));
        m.indices.push_back(static_cast<uint32_t>(o + 1));
        uint32_t s0 = static_cast<uint32_t>(o + 2);
        uint32_t s1 = static_cast<uint32_t>(o + 3);
        uint32_t s2 = static_cast<uint32_t>(o + 6);
        uint32_t s3 = static_cast<uint32_t>(o + 7);
        m.indices.push_back(s0);
        m.indices.push_back(s1);
        m.indices.push_back(s3);
        m.indices.push_back(s0);
        m.indices.push_back(s3);
        m.indices.push_back(s2);
    }
    ComputeTangents(m);
    return m;
}

PrimitiveMesh Cylinder(float radius, float halfHeight, int radialSegments) {
    return cylinderOrCone(radius, radius, -halfHeight, halfHeight, radialSegments);
}

PrimitiveMesh Cone(float radius, float height, int radialSegments) {
    return cylinderOrCone(radius, 0.0f, 0.0f, height, radialSegments);
}

PrimitiveMesh Capsule(float radius, float halfCylinderHeight, int radialSegments, int heightSegments) {
    PrimitiveMesh m;
    int rseg = radialSegments >= 3 ? radialSegments : 3;
    int hseg = heightSegments >= 1 ? heightSegments : 1;
    float topY = halfCylinderHeight;
    float botY = -halfCylinderHeight;
    int latHalf = (rseg / 2) + 1;
    auto addHemisphere = [&](float centerY, float sign) {
        uint32_t base = static_cast<uint32_t>(m.vertices.size());
        for (int iy = 0; iy <= latHalf; ++iy) {
            float ty = static_cast<float>(iy) / static_cast<float>(latHalf);
            float phi = (1.0f - ty) * 0.5f * kPi;
            if (sign < 0.0f) phi = 0.5f * kPi + ty * 0.5f * kPi;
            float y = centerY + sign * radius * std::cos(phi);
            float r = radius * std::sin(phi);
            float ny = sign * std::cos(phi);
            float v = (sign > 0.0f) ? ty : (1.0f - ty);
            for (int ix = 0; ix <= rseg; ++ix) {
                float tx = static_cast<float>(ix) / static_cast<float>(rseg);
                float theta = tx * 2.0f * kPi;
                float x = r * std::cos(theta);
                float z = r * std::sin(theta);
                float nx = std::cos(theta) * std::sin(phi);
                float nz = std::sin(theta) * std::sin(phi);
                if (sign < 0.0f) { nx = -nx; nz = -nz; }
                Vertex vtx{};
                setVertex(vtx, x, y, z, nx, ny, nz, 1.0f - tx, v);
                m.vertices.push_back(vtx);
            }
        }
        for (int iy = 0; iy < latHalf; ++iy) {
            for (int ix = 0; ix < rseg; ++ix) {
                uint32_t a = base + static_cast<uint32_t>(iy * (rseg + 1) + ix);
                uint32_t b = a + 1;
                uint32_t c = a + (rseg + 1);
                uint32_t d = c + 1;
                m.indices.push_back(a);
                m.indices.push_back(c);
                m.indices.push_back(b);
                m.indices.push_back(b);
                m.indices.push_back(c);
                m.indices.push_back(d);
            }
        }
    };
    addHemisphere(topY, 1.0f);
    addHemisphere(botY, -1.0f);
    uint32_t cylBase = static_cast<uint32_t>(m.vertices.size());
    for (int iy = 0; iy <= hseg; ++iy) {
        float ty = static_cast<float>(iy) / static_cast<float>(hseg);
        float y = botY + ty * (2.0f * halfCylinderHeight);
        float v = 1.0f - ty;
        for (int ix = 0; ix <= rseg; ++ix) {
            float tx = static_cast<float>(ix) / static_cast<float>(rseg);
            float a = tx * 2.0f * kPi;
            float x = radius * std::cos(a);
            float z = radius * std::sin(a);
            float nx = std::cos(a);
            float nz = std::sin(a);
            Vertex vtx{};
            setVertex(vtx, x, y, z, nx, 0.0f, nz, 1.0f - tx, v);
            m.vertices.push_back(vtx);
        }
    }
    for (int iy = 0; iy < hseg; ++iy) {
        for (int ix = 0; ix < rseg; ++ix) {
            uint32_t a = cylBase + static_cast<uint32_t>(iy * (rseg + 1) + ix);
            uint32_t b = a + 1;
            uint32_t c = a + (rseg + 1);
            uint32_t d = c + 1;
            m.indices.push_back(a);
            m.indices.push_back(c);
            m.indices.push_back(b);
            m.indices.push_back(b);
            m.indices.push_back(c);
            m.indices.push_back(d);
        }
    }
    ComputeTangents(m);
    return m;
}

PrimitiveMesh Torus(float majorRadius, float minorRadius, int majorSegments, int minorSegments) {
    PrimitiveMesh m;
    int maj = majorSegments >= 3 ? majorSegments : 3;
    int min = minorSegments >= 3 ? minorSegments : 3;
    for (int i = 0; i <= maj; ++i) {
        float u = static_cast<float>(i) / static_cast<float>(maj);
        float theta = u * 2.0f * kPi;
        float cx = majorRadius * std::cos(theta);
        float cz = majorRadius * std::sin(theta);
        float tx = -std::sin(theta);
        float tz = std::cos(theta);
        for (int j = 0; j <= min; ++j) {
            float v = static_cast<float>(j) / static_cast<float>(min);
            float phi = v * 2.0f * kPi;
            float dx = std::cos(phi);
            float dy = std::sin(phi);
            float px = cx + minorRadius * dx * std::cos(theta);
            float py = minorRadius * dy;
            float pz = cz + minorRadius * dx * std::sin(theta);
            float nx = dx * std::cos(theta);
            float ny = dy;
            float nz = dx * std::sin(theta);
            Vertex vtx{};
            setVertex(vtx, px, py, pz, nx, ny, nz, 1.0f - u, 1.0f - v);
            m.vertices.push_back(vtx);
        }
    }
    for (int i = 0; i < maj; ++i) {
        for (int j = 0; j < min; ++j) {
            uint32_t a = static_cast<uint32_t>(i * (min + 1) + j);
            uint32_t b = a + 1;
            uint32_t c = a + (min + 1);
            uint32_t d = c + 1;
            m.indices.push_back(a);
            m.indices.push_back(c);
            m.indices.push_back(b);
            m.indices.push_back(b);
            m.indices.push_back(c);
            m.indices.push_back(d);
        }
    }
    ComputeTangents(m);
    return m;
}

const PrimitiveMesh& UnitBox() {
    static const PrimitiveMesh m = Box(0.5f, 0.5f, 0.5f);
    return m;
}

const PrimitiveMesh& UnitSphere() {
    static const PrimitiveMesh m = Sphere(1.0f, 32, 24);
    return m;
}

const PrimitiveMesh& UnitQuad() {
    static const PrimitiveMesh m = Plane(1.0f, 1.0f, 1, 1);
    return m;
}

} // namespace Engine
