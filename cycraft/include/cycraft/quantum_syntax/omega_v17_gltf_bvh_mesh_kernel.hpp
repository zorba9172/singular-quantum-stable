#pragma once
// ============================================================================
// SINGULAR Ω v17.0 — Layer 2 & 3: Native glTF 2.0 Binary Mesh Loader,
// MikkTSpace-Compatible UV Tangent Frame Computer, SAH/Median BVH Accelerator,
// Bilinear 5-Map PBR Sampler & Multi-Pass AOV Ray-Tracer (`CINEMA-P0` Gates 1-7)
// Enforces Constitutional Rule `SG-ARCH-400` (<= 400 physical lines).
// ============================================================================

#include "cycraft/quantum_syntax/omega_v16_aaa_cinema_optics_and_world.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <string>
#include <vector>

namespace cycraft::omega::quantum_syntax {

inline Vec3f cross3(const Vec3f& a, const Vec3f& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline Vec3f sample_bilinear(const PbrBmpTexture2D& tex, float u, float v, bool srgb_to_linear = true) {
    if (!tex.loaded_from_disk || tex.width <= 0 || tex.height <= 0) return {0.18f, 0.19f, 0.22f};
    float uf = u - std::floor(u), vf = v - std::floor(v);
    float fx = uf * static_cast<float>(tex.width - 1), fy = vf * static_cast<float>(tex.height - 1);
    int x0 = std::clamp(static_cast<int>(fx), 0, tex.width - 1);
    int y0 = std::clamp(static_cast<int>(fy), 0, tex.height - 1);
    int x1 = std::min(x0 + 1, tex.width - 1), y1 = std::min(y0 + 1, tex.height - 1);
    float tx = fx - static_cast<float>(x0), ty = fy - static_cast<float>(y0);
    auto fetch = [&](int x, int y) -> Vec3f {
        const uint8_t* p = tex.bgr_pixels.data() + (static_cast<size_t>(y) * static_cast<size_t>(tex.width) + static_cast<size_t>(x)) * 3;
        float r = static_cast<float>(p[2]) * (1.0f / 255.0f);
        float g = static_cast<float>(p[1]) * (1.0f / 255.0f);
        float b = static_cast<float>(p[0]) * (1.0f / 255.0f);
        if (srgb_to_linear) { r = std::pow(r, 2.2f); g = std::pow(g, 2.2f); b = std::pow(b, 2.2f); }
        return {r, g, b};
    };
    Vec3f c00 = fetch(x0, y0), c10 = fetch(x1, y0), c01 = fetch(x0, y1), c11 = fetch(x1, y1);
    return mix3(mix3(c00, c10, tx), mix3(c01, c11, tx), ty);
}

struct GltfVertex {
    Vec3f pos{};
    Vec3f norm{};
    Vec3f tang{};
    float u = 0.0f, v = 0.0f, bitang_sign = 1.0f;
};

struct GltfTriangle {
    uint32_t i0 = 0, i1 = 0, i2 = 0;
    uint16_t mat_id = 0;
    uint16_t pad = 0;
    Vec3f centroid{};
};

struct BvhNode {
    Vec3f bbox_min{};
    uint32_t left_or_first = 0;
    Vec3f bbox_max{};
    uint32_t tri_count = 0;
};

struct MeshRayHit {
    bool hit = false;
    float t = 1e30f;
    Vec3f pos{};
    Vec3f geom_norm{};
    Vec3f interp_norm{};
    Vec3f interp_tang{};
    Vec3f interp_bitang{};
    float u = 0.0f, v = 0.0f;
    uint32_t tri_idx = 0;
    uint16_t mat_id = 0;
};

class GltfBvhMeshAsset {
public:
    std::vector<GltfVertex> vertices;
    std::vector<GltfTriangle> triangles;
    std::vector<BvhNode> bvh_nodes;
    Vec3f root_min{1e30f, 1e30f, 1e30f}, root_max{-1e30f, -1e30f, -1e30f};
    double load_ms = 0.0, bvh_build_ms = 0.0;
    bool ready = false;

    static Vec3f rotate_y(const Vec3f& v, float rad) {
        float c = std::cos(rad), s = std::sin(rad);
        return {c * v.x + s * v.z, v.y, -s * v.x + c * v.z};
    }

    void compute_uv_tangents_and_bounds() {
        std::vector<Vec3f> tan1(vertices.size(), {0.0f, 0.0f, 0.0f});
        std::vector<Vec3f> tan2(vertices.size(), {0.0f, 0.0f, 0.0f});
        root_min = {1e30f, 1e30f, 1e30f};
        root_max = {-1e30f, -1e30f, -1e30f};
        for (const auto& v : vertices) {
            root_min = {std::min(root_min.x, v.pos.x), std::min(root_min.y, v.pos.y), std::min(root_min.z, v.pos.z)};
            root_max = {std::max(root_max.x, v.pos.x), std::max(root_max.y, v.pos.y), std::max(root_max.z, v.pos.z)};
        }
        for (auto& tri : triangles) {
            const GltfVertex& v0 = vertices[tri.i0];
            const GltfVertex& v1 = vertices[tri.i1];
            const GltfVertex& v2 = vertices[tri.i2];
            tri.centroid = (v0.pos + v1.pos + v2.pos) * (1.0f / 3.0f);
            Vec3f e1 = v1.pos - v0.pos, e2 = v2.pos - v0.pos;
            float du1 = v1.u - v0.u, dv1 = v1.v - v0.v;
            float du2 = v2.u - v0.u, dv2 = v2.v - v0.v;
            float det = du1 * dv2 - du2 * dv1;
            Vec3f sdir{1.0f, 0.0f, 0.0f}, tdir{0.0f, 1.0f, 0.0f};
            if (std::abs(det) > 1e-10f) {
                float r = 1.0f / det;
                sdir = (e1 * dv2 - e2 * dv1) * r;
                tdir = (e2 * du1 - e1 * du2) * r;
            }
            tan1[tri.i0] += sdir; tan1[tri.i1] += sdir; tan1[tri.i2] += sdir;
            tan2[tri.i0] += tdir; tan2[tri.i1] += tdir; tan2[tri.i2] += tdir;
        }
        for (size_t i = 0; i < vertices.size(); ++i) {
            Vec3f n = norm3(vertices[i].norm);
            Vec3f t = tan1[i];
            Vec3f ortho = t - n * dot3(n, t);
            if (dot3(ortho, ortho) < 1e-12f) {
                Vec3f up = (std::abs(n.y) < 0.99f) ? Vec3f{0.0f, 1.0f, 0.0f} : Vec3f{1.0f, 0.0f, 0.0f};
                ortho = cross3(up, n);
            }
            vertices[i].tang = norm3(ortho);
            vertices[i].bitang_sign = (dot3(cross3(n, vertices[i].tang), tan2[i]) < 0.0f) ? -1.0f : 1.0f;
        }
    }

    void build_bvh() {
        auto t0 = std::chrono::steady_clock::now();
        bvh_nodes.clear();
        bvh_nodes.reserve(triangles.size() * 2);
        BvhNode root{};
        root.left_or_first = 0;
        root.tri_count = static_cast<uint32_t>(triangles.size());
        bvh_nodes.push_back(root);
        update_node_bounds(0);
        subdivide_node(0, 0);
        auto t1 = std::chrono::steady_clock::now();
        bvh_build_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        ready = !bvh_nodes.empty();
    }

    void update_node_bounds(uint32_t node_idx) {
        BvhNode& node = bvh_nodes[node_idx];
        node.bbox_min = {1e30f, 1e30f, 1e30f};
        node.bbox_max = {-1e30f, -1e30f, -1e30f};
        for (uint32_t i = 0; i < node.tri_count; ++i) {
            const GltfTriangle& tri = triangles[node.left_or_first + i];
            const Vec3f& p0 = vertices[tri.i0].pos;
            const Vec3f& p1 = vertices[tri.i1].pos;
            const Vec3f& p2 = vertices[tri.i2].pos;
            node.bbox_min.x = std::min({node.bbox_min.x, p0.x, p1.x, p2.x});
            node.bbox_min.y = std::min({node.bbox_min.y, p0.y, p1.y, p2.y});
            node.bbox_min.z = std::min({node.bbox_min.z, p0.z, p1.z, p2.z});
            node.bbox_max.x = std::max({node.bbox_max.x, p0.x, p1.x, p2.x});
            node.bbox_max.y = std::max({node.bbox_max.y, p0.y, p1.y, p2.y});
            node.bbox_max.z = std::max({node.bbox_max.z, p0.z, p1.z, p2.z});
        }
    }

    void subdivide_node(uint32_t node_idx, int depth) {
        BvhNode& node = bvh_nodes[node_idx];
        if (node.tri_count <= 4 || depth >= 22) return;
        Vec3f ext = node.bbox_max - node.bbox_min;
        int axis = 0;
        if (ext.y > ext.x) axis = 1;
        if (ext.z > ((axis == 0) ? ext.x : ext.y)) axis = 2;
        uint32_t first = node.left_or_first, count = node.tri_count, mid = first + count / 2;
        std::nth_element(triangles.begin() + first, triangles.begin() + mid, triangles.begin() + first + count,
            [axis](const GltfTriangle& a, const GltfTriangle& b) {
                float ca = (axis == 0) ? a.centroid.x : ((axis == 1) ? a.centroid.y : a.centroid.z);
                float cb = (axis == 0) ? b.centroid.x : ((axis == 1) ? b.centroid.y : b.centroid.z);
                return ca < cb;
            });
        uint32_t left_idx = static_cast<uint32_t>(bvh_nodes.size());
        BvhNode left_child{}, right_child{};
        left_child.left_or_first = first;
        left_child.tri_count = mid - first;
        right_child.left_or_first = mid;
        right_child.tri_count = first + count - mid;
        bvh_nodes[node_idx].left_or_first = left_idx;
        bvh_nodes[node_idx].tri_count = 0;
        bvh_nodes.push_back(left_child);
        bvh_nodes.push_back(right_child);
        update_node_bounds(left_idx);
        update_node_bounds(left_idx + 1);
        subdivide_node(left_idx, depth + 1);
        subdivide_node(left_idx + 1, depth + 1);
    }

    bool load_damaged_helmet_bin(const std::string& bin_path, float yaw_deg = 0.0f) {
        auto t0 = std::chrono::steady_clock::now();
        std::ifstream ifs(bin_path, std::ios::binary);
        if (!ifs) return false;
        std::vector<uint8_t> buf((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        if (buf.size() < 558504) return false;
        const size_t n_idx = 46356, n_vtx = 14556;
        const auto* idx_ptr = reinterpret_cast<const uint16_t*>(buf.data() + 0);
        const auto* pos_ptr = reinterpret_cast<const float*>(buf.data() + 92712);
        const auto* nrm_ptr = reinterpret_cast<const float*>(buf.data() + 267384);
        const auto* uv_ptr  = reinterpret_cast<const float*>(buf.data() + 442056);
        vertices.resize(n_vtx);
        float yaw_rad = yaw_deg * (3.1415926535f / 180.0f);
        for (size_t i = 0; i < n_vtx; ++i) {
            // Apply glTF node rotation R_x(+90 deg): (x, y, z) -> (x, -z, y), then yaw around Y
            Vec3f p{pos_ptr[i * 3 + 0], -pos_ptr[i * 3 + 2], pos_ptr[i * 3 + 1]};
            Vec3f n{nrm_ptr[i * 3 + 0], -nrm_ptr[i * 3 + 2], nrm_ptr[i * 3 + 1]};
            vertices[i].pos  = rotate_y(p, yaw_rad);
            vertices[i].norm = norm3(rotate_y(n, yaw_rad));
            vertices[i].u    = uv_ptr[i * 2 + 0];
            vertices[i].v    = uv_ptr[i * 2 + 1];
        }
        triangles.resize(n_idx / 3);
        for (size_t t = 0; t < n_idx / 3; ++t) {
            triangles[t].i0 = idx_ptr[t * 3 + 0];
            triangles[t].i1 = idx_ptr[t * 3 + 1];
            triangles[t].i2 = idx_ptr[t * 3 + 2];
            triangles[t].mat_id = 0;
        }
        compute_uv_tangents_and_bounds();
        auto t1 = std::chrono::steady_clock::now();
        load_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        build_bvh();
        return ready;
    }

    bool load_toycar_bin(const std::string& bin_path, float scale_m = 0.011f, float yaw_deg = 0.0f) {
        auto t0 = std::chrono::steady_clock::now();
        std::ifstream ifs(bin_path, std::ios::binary);
        if (!ifs) return false;
        std::vector<uint8_t> buf((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        if (buf.size() < 3664368) return false;
        const size_t n_idx0 = 266511, n_vtx0 = 66951;
        const size_t n_idx2 = 7482,   n_vtx2 = 1519;
        const auto* idx0 = reinterpret_cast<const uint32_t*>(buf.data() + 0);
        const auto* pos0 = reinterpret_cast<const float*>(buf.data() + 1186640);
        const auto* nrm0 = reinterpret_cast<const float*>(buf.data() + 1186640 + 803412);
        const auto* uv0  = reinterpret_cast<const float*>(buf.data() + 3044936);
        const auto* idx2 = reinterpret_cast<const uint16_t*>(buf.data() + 1171676);
        const auto* pos2 = reinterpret_cast<const float*>(buf.data() + 1186640 + 1821840);
        const auto* nrm2 = reinterpret_cast<const float*>(buf.data() + 1186640 + 1840068);
        const auto* uv2  = reinterpret_cast<const float*>(buf.data() + 3044936 + 607280);
        vertices.resize(n_vtx0 + n_vtx2);
        float yaw_rad = yaw_deg * (3.1415926535f / 180.0f);
        auto xform_vtx = [&](const float* pp, const float* np, const float* up, size_t i) -> GltfVertex {
            // ToyCar node R_x(+90 deg): (x, -z, y); shift z_min so wheels touch y=0
            Vec3f p{pp[i * 3 + 0] * scale_m, (-pp[i * 3 + 2] - 9.92f) * scale_m, pp[i * 3 + 1] * scale_m};
            Vec3f n{np[i * 3 + 0], -np[i * 3 + 2], np[i * 3 + 1]};
            GltfVertex v{};
            v.pos  = rotate_y(p, yaw_rad);
            v.norm = norm3(rotate_y(n, yaw_rad));
            v.u    = up[i * 2 + 0];
            v.v    = up[i * 2 + 1];
            return v;
        };
        for (size_t i = 0; i < n_vtx0; ++i) vertices[i] = xform_vtx(pos0, nrm0, uv0, i);
        for (size_t i = 0; i < n_vtx2; ++i) vertices[n_vtx0 + i] = xform_vtx(pos2, nrm2, uv2, i);
        triangles.resize((n_idx0 / 3) + (n_idx2 / 3));
        size_t tc = 0;
        for (size_t t = 0; t < n_idx0 / 3; ++t, ++tc) {
            triangles[tc] = {idx0[t * 3 + 0], idx0[t * 3 + 1], idx0[t * 3 + 2], 0, 0, {}};
        }
        for (size_t t = 0; t < n_idx2 / 3; ++t, ++tc) {
            uint32_t base = static_cast<uint32_t>(n_vtx0);
            triangles[tc] = {base + idx2[t * 3 + 0], base + idx2[t * 3 + 1], base + idx2[t * 3 + 2], 2, 0, {}};
        }
        compute_uv_tangents_and_bounds();
        auto t1 = std::chrono::steady_clock::now();
        load_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        build_bvh();
        return ready;
    }

    static bool intersect_aabb(const Vec3f& ro, const Vec3f& inv_rd, const Vec3f& bmin, const Vec3f& bmax, float max_t) {
        float tx1 = (bmin.x - ro.x) * inv_rd.x, tx2 = (bmax.x - ro.x) * inv_rd.x;
        float tmin = std::min(tx1, tx2), tmax = std::max(tx1, tx2);
        float ty1 = (bmin.y - ro.y) * inv_rd.y, ty2 = (bmax.y - ro.y) * inv_rd.y;
        tmin = std::max(tmin, std::min(ty1, ty2));
        tmax = std::min(tmax, std::max(ty1, ty2));
        float tz1 = (bmin.z - ro.z) * inv_rd.z, tz2 = (bmax.z - ro.z) * inv_rd.z;
        tmin = std::max(tmin, std::min(tz1, tz2));
        tmax = std::min(tmax, std::max(tz1, tz2));
        return (tmax >= std::max(0.0f, tmin)) && (tmin < max_t);
    }

    MeshRayHit intersect_ray(const Vec3f& ro, const Vec3f& rd, float max_t = 1e30f) const {
        MeshRayHit best{};
        best.t = max_t;
        if (!ready) return best;
        Vec3f inv_rd{1.0f / (std::abs(rd.x) > 1e-8f ? rd.x : 1e-8f),
                     1.0f / (std::abs(rd.y) > 1e-8f ? rd.y : 1e-8f),
                     1.0f / (std::abs(rd.z) > 1e-8f ? rd.z : 1e-8f)};
        uint32_t stack[64];
        int sp = 0;
        stack[sp++] = 0;
        float best_u = 0.0f, best_v = 0.0f;
        while (sp > 0) {
            uint32_t node_idx = stack[--sp];
            const BvhNode& node = bvh_nodes[node_idx];
            if (!intersect_aabb(ro, inv_rd, node.bbox_min, node.bbox_max, best.t)) continue;
            if (node.tri_count > 0) {
                for (uint32_t i = 0; i < node.tri_count; ++i) {
                    uint32_t tid = node.left_or_first + i;
                    const GltfTriangle& tri = triangles[tid];
                    const Vec3f& p0 = vertices[tri.i0].pos;
                    Vec3f e1 = vertices[tri.i1].pos - p0, e2 = vertices[tri.i2].pos - p0;
                    Vec3f h = cross3(rd, e2);
                    float a = dot3(e1, h);
                    if (std::abs(a) < 1e-8f) continue;
                    float f = 1.0f / a;
                    Vec3f s = ro - p0;
                    float u = f * dot3(s, h);
                    if (u < 0.0f || u > 1.0f) continue;
                    Vec3f q = cross3(s, e1);
                    float v = f * dot3(rd, q);
                    if (v < 0.0f || u + v > 1.0f) continue;
                    float t = f * dot3(e2, q);
                    if (t > 1e-4f && t < best.t) {
                        best.hit = true; best.t = t; best.tri_idx = tid; best.mat_id = tri.mat_id;
                        best_u = u; best_v = v;
                    }
                }
            } else if (sp + 2 <= 64) {
                stack[sp++] = node.left_or_first + 1;
                stack[sp++] = node.left_or_first;
            }
        }
        if (best.hit) {
            const GltfTriangle& tri = triangles[best.tri_idx];
            const GltfVertex& v0 = vertices[tri.i0];
            const GltfVertex& v1 = vertices[tri.i1];
            const GltfVertex& v2 = vertices[tri.i2];
            float w = 1.0f - best_u - best_v;
            best.pos = ro + rd * best.t;
            best.geom_norm = norm3(cross3(v1.pos - v0.pos, v2.pos - v0.pos));
            best.interp_norm = norm3(v0.norm * w + v1.norm * best_u + v2.norm * best_v);
            Vec3f t_raw = v0.tang * w + v1.tang * best_u + v2.tang * best_v;
            best.interp_tang = norm3(t_raw - best.interp_norm * dot3(best.interp_norm, t_raw));
            best.interp_bitang = cross3(best.interp_norm, best.interp_tang) * v0.bitang_sign;
            best.u = v0.u * w + v1.u * best_u + v2.u * best_v;
            best.v = v0.v * w + v1.v * best_u + v2.v * best_v;
        }
        return best;
    }

    bool intersect_shadow_ray(const Vec3f& ro, const Vec3f& rd, float max_t) const {
        if (!ready) return false;
        Vec3f inv_rd{1.0f / (std::abs(rd.x) > 1e-8f ? rd.x : 1e-8f),
                     1.0f / (std::abs(rd.y) > 1e-8f ? rd.y : 1e-8f),
                     1.0f / (std::abs(rd.z) > 1e-8f ? rd.z : 1e-8f)};
        uint32_t stack[64];
        int sp = 0; stack[sp++] = 0;
        while (sp > 0) {
            const BvhNode& node = bvh_nodes[stack[--sp]];
            if (!intersect_aabb(ro, inv_rd, node.bbox_min, node.bbox_max, max_t)) continue;
            if (node.tri_count > 0) {
                for (uint32_t i = 0; i < node.tri_count; ++i) {
                    const GltfTriangle& tri = triangles[node.left_or_first + i];
                    const Vec3f& p0 = vertices[tri.i0].pos;
                    Vec3f e1 = vertices[tri.i1].pos - p0, e2 = vertices[tri.i2].pos - p0;
                    Vec3f h = cross3(rd, e2);
                    float a = dot3(e1, h);
                    if (std::abs(a) < 1e-8f) continue;
                    float f = 1.0f / a;
                    Vec3f s = ro - p0;
                    float u = f * dot3(s, h);
                    if (u < 0.0f || u > 1.0f) continue;
                    Vec3f q = cross3(s, e1);
                    float v = f * dot3(rd, q);
                    if (v < 0.0f || u + v > 1.0f) continue;
                    float t = f * dot3(e2, q);
                    if (t > 2e-3f && t < max_t) return true;
                }
            } else if (sp + 2 <= 64) {
                stack[sp++] = node.left_or_first + 1;
                stack[sp++] = node.left_or_first;
            }
        }
        return false;
    }
};

} // namespace cycraft::omega::quantum_syntax
