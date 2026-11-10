// ============================================================================
// SINGULAR Ω — ERA real sparse workload: linear-blend skinning of a real rigged
// glTF asset (`DISC-000901`, product track). Rule `SG-ARCH-400`: <= 400 lines.
//
// LBS is linear in the skinning-matrix entries with constant coefficients:
//   p_v[r] = Σ_j w_vj Σ_c S_j[r][c] · ṽ_v[c],   ṽ = (x, y, z, 1)
// so it maps exactly onto ERA: inputs = 12 entries of each joint's 3x4 skinning
// matrix, outputs = 3 coordinates per vertex, arc weight = w_vj · ṽ_v[c].
// The (non-linear) joint hierarchy is evaluated on the host; a pose edit of one
// joint changes the matrices of its subtree, which becomes one batch of input ops.
// Geometry is uniformly rescaled so |coord| <= 4 (hard-tanh range is ±8); the scale
// is reported and errors are given in both normalized and asset units.
// ============================================================================
#pragma once
#include "cycraft/quantum_syntax/omega_era_audit_bench.hpp"
#include <functional>
#include <simdjson.h>

namespace cycraft::omega::quantum_syntax::era {

struct M4 { double m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}; };  // column-major (glTF)
inline M4 mul(const M4& a, const M4& b) {
    M4 r;
    for (int c = 0; c < 4; ++c) for (int rr = 0; rr < 4; ++rr) {
        double s = 0; for (int k = 0; k < 4; ++k) s += a.m[k * 4 + rr] * b.m[c * 4 + k];
        r.m[c * 4 + rr] = s;
    }
    return r;
}
inline M4 trs(const double t[3], const double q[4], const double s[3]) {
    const double x = q[0], y = q[1], z = q[2], w = q[3];
    M4 r;
    const double R[9] = {1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w),
                         2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w),
                         2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y)};  // columns
    for (int c = 0; c < 3; ++c) for (int rr = 0; rr < 3; ++rr) r.m[c * 4 + rr] = R[c * 3 + rr] * s[c];
    r.m[12] = t[0]; r.m[13] = t[1]; r.m[14] = t[2];
    return r;
}

struct Rig {
    bool ok = false; std::string path, error;
    std::vector<int> node_parent;            // per glTF node
    std::vector<M4> node_local;
    std::vector<int> joints;                 // skin joint -> node index
    std::vector<M4> inv_bind;
    std::vector<std::array<double, 3>> rest; // vertex rest positions (asset units)
    std::vector<std::array<int, 4>> vj;      // joint indices (into joints)
    std::vector<std::array<double, 4>> vw;   // weights
    double scale = 1.0;                      // normalized = asset * scale
};

class RigLoader {
public:
    static Rig load_glb(const std::string& path) {
        Rig rig; rig.path = path;
        std::ifstream f(path, std::ios::binary);
        if (!f) { rig.error = "cannot open"; return rig; }
        std::vector<char> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        auto u32 = [&](size_t off) { uint32_t v; std::memcpy(&v, data.data() + off, 4); return v; };
        if (data.size() < 20 || u32(0) != 0x46546C67u) { rig.error = "not a GLB"; return rig; }
        const uint32_t jlen = u32(12);
        if (u32(16) != 0x4E4F534Au || 20 + jlen + 8 > data.size()) { rig.error = "bad JSON chunk"; return rig; }
        const size_t bin_off = 20 + jlen + 8;
        const std::string json(data.data() + 20, jlen);
        simdjson::dom::parser parser; simdjson::dom::element doc;
        if (parser.parse(json).get(doc)) { rig.error = "JSON parse failed"; return rig; }
        auto num = [](simdjson::dom::element e) { double d = 0; int64_t i = 0; uint64_t u = 0;
            if (!e.get(d)) return d; if (!e.get(i)) return static_cast<double>(i); if (!e.get(u)) return static_cast<double>(u); return 0.0; };
        simdjson::dom::array nodes, skins, meshes, accessors, views;
        if (doc["nodes"].get(nodes) || doc["skins"].get(skins) || doc["meshes"].get(meshes) || doc["accessors"].get(accessors) || doc["bufferViews"].get(views)) {
            rig.error = "missing nodes/skins/meshes/accessors/bufferViews"; return rig;
        }
        const size_t nn = nodes.size();
        rig.node_parent.assign(nn, -1); rig.node_local.assign(nn, M4{});
        int skinned_node = -1;
        size_t ni = 0;
        for (auto n : nodes) {
            simdjson::dom::array ch, arr;
            if (!n["children"].get(ch)) for (auto c : ch) rig.node_parent[static_cast<size_t>(num(c))] = static_cast<int>(ni);
            if (!n["matrix"].get(arr)) { int k = 0; for (auto x : arr) rig.node_local[ni].m[k++] = num(x); }
            else {
                double t[3] = {0, 0, 0}, q[4] = {0, 0, 0, 1}, s[3] = {1, 1, 1};
                if (!n["translation"].get(arr)) { int k = 0; for (auto x : arr) t[k++] = num(x); }
                if (!n["rotation"].get(arr)) { int k = 0; for (auto x : arr) q[k++] = num(x); }
                if (!n["scale"].get(arr)) { int k = 0; for (auto x : arr) s[k++] = num(x); }
                rig.node_local[ni] = trs(t, q, s);
            }
            simdjson::dom::element tmp;
            if (!n["skin"].get(tmp) && !n["mesh"].get(tmp) && skinned_node < 0) skinned_node = static_cast<int>(ni);
            ++ni;
        }
        if (skinned_node < 0) { rig.error = "no node with skin+mesh"; return rig; }
        simdjson::dom::element sn = nodes.at(static_cast<size_t>(skinned_node)).value();
        const size_t skin_i = static_cast<size_t>(num(sn["skin"].value())), mesh_i = static_cast<size_t>(num(sn["mesh"].value()));
        simdjson::dom::element skin = skins.at(skin_i).value();
        simdjson::dom::array jarr;
        if (skin["joints"].get(jarr)) { rig.error = "skin without joints"; return rig; }
        for (auto j : jarr) rig.joints.push_back(static_cast<int>(num(j)));
        // accessor reader: returns component values as doubles, `comps` per element
        auto read = [&](size_t acc_i, int& comps, std::vector<double>& out) -> bool {
            simdjson::dom::element a = accessors.at(acc_i).value();
            std::string_view type; if (a["type"].get(type)) return false;
            comps = type == "SCALAR" ? 1 : type == "VEC3" ? 3 : type == "VEC4" ? 4 : type == "MAT4" ? 16 : 0;
            if (!comps) return false;
            const int ct = static_cast<int>(num(a["componentType"].value()));
            const size_t count = static_cast<size_t>(num(a["count"].value()));
            simdjson::dom::element bvidx; if (a["bufferView"].get(bvidx)) return false;
            simdjson::dom::element bv = views.at(static_cast<size_t>(num(bvidx))).value();
            size_t off = bin_off, stride = 0; simdjson::dom::element e;
            if (!bv["byteOffset"].get(e)) off += static_cast<size_t>(num(e));
            if (!a["byteOffset"].get(e)) off += static_cast<size_t>(num(e));
            const size_t csz = (ct == 5126 || ct == 5125) ? 4 : (ct == 5123 || ct == 5122) ? 2 : 1;
            if (!bv["byteStride"].get(e)) stride = static_cast<size_t>(num(e));
            if (!stride) stride = csz * static_cast<size_t>(comps);
            out.resize(count * static_cast<size_t>(comps));
            for (size_t i = 0; i < count; ++i)
                for (int c = 0; c < comps; ++c) {
                    const size_t p = off + i * stride + static_cast<size_t>(c) * csz;
                    if (p + csz > data.size()) return false;
                    double v = 0;
                    if (ct == 5126) { float x; std::memcpy(&x, data.data() + p, 4); v = x; }
                    else if (ct == 5123) { uint16_t x; std::memcpy(&x, data.data() + p, 2); v = x; }
                    else if (ct == 5121) { uint8_t x = static_cast<uint8_t>(data[p]); v = x; }
                    else if (ct == 5125) { uint32_t x; std::memcpy(&x, data.data() + p, 4); v = x; }
                    else return false;
                    out[i * static_cast<size_t>(comps) + static_cast<size_t>(c)] = v;
                }
            return true;
        };
        int comps = 0; std::vector<double> buf;
        simdjson::dom::element ib;
        rig.inv_bind.assign(rig.joints.size(), M4{});
        if (!skin["inverseBindMatrices"].get(ib)) {
            if (!read(static_cast<size_t>(num(ib)), comps, buf) || comps != 16) { rig.error = "inverseBindMatrices unreadable"; return rig; }
            for (size_t j = 0; j < rig.joints.size(); ++j) for (int k = 0; k < 16; ++k) rig.inv_bind[j].m[k] = buf[j * 16 + static_cast<size_t>(k)];
        }
        simdjson::dom::array prims;
        if (meshes.at(mesh_i)["primitives"].get(prims)) { rig.error = "mesh without primitives"; return rig; }
        simdjson::dom::element attrs = prims.at(0)["attributes"].value();
        simdjson::dom::element pa, ja, wa;
        if (attrs["POSITION"].get(pa) || attrs["JOINTS_0"].get(ja) || attrs["WEIGHTS_0"].get(wa)) { rig.error = "primitive lacks POSITION/JOINTS_0/WEIGHTS_0"; return rig; }
        std::vector<double> P, J, Wt;
        int cp = 0, cj = 0, cw = 0;
        if (!read(static_cast<size_t>(num(pa)), cp, P) || cp != 3 || !read(static_cast<size_t>(num(ja)), cj, J) || cj != 4 ||
            !read(static_cast<size_t>(num(wa)), cw, Wt) || cw != 4) { rig.error = "attribute read failed (normalized weights unsupported)"; return rig; }
        const size_t nv = P.size() / 3;
        double maxabs = 0;
        for (size_t v = 0; v < nv; ++v) {
            rig.rest.push_back({P[v * 3], P[v * 3 + 1], P[v * 3 + 2]});
            rig.vj.push_back({static_cast<int>(J[v * 4]), static_cast<int>(J[v * 4 + 1]), static_cast<int>(J[v * 4 + 2]), static_cast<int>(J[v * 4 + 3])});
            rig.vw.push_back({Wt[v * 4], Wt[v * 4 + 1], Wt[v * 4 + 2], Wt[v * 4 + 3]});
        }
        rig.ok = true;
        // Uniform scale so that rest-pose skinned positions satisfy |coord| <= 4.
        const auto S = skin_matrices(rig);
        for (size_t v = 0; v < nv; ++v) { const auto p = lbs(rig, S, v); for (double x : p) maxabs = std::max(maxabs, std::abs(x)); }
        rig.scale = maxabs > 0 ? 4.0 / maxabs : 1.0;
        return rig;
    }

    static std::vector<M4> world(const Rig& rig) {
        std::vector<M4> w(rig.node_local.size());
        std::vector<int> state(rig.node_local.size(), 0);
        std::function<const M4&(int)> get = [&](int i) -> const M4& {
            if (state[static_cast<size_t>(i)]) return w[static_cast<size_t>(i)];
            const int p = rig.node_parent[static_cast<size_t>(i)];
            w[static_cast<size_t>(i)] = p < 0 ? rig.node_local[static_cast<size_t>(i)] : mul(get(p), rig.node_local[static_cast<size_t>(i)]);
            state[static_cast<size_t>(i)] = 1;
            return w[static_cast<size_t>(i)];
        };
        for (size_t i = 0; i < w.size(); ++i) get(static_cast<int>(i));
        return w;
    }
    // Skinning matrices in ASSET units (scale applied later when quantizing).
    static std::vector<M4> skin_matrices(const Rig& rig) {
        const auto w = world(rig);
        std::vector<M4> S(rig.joints.size());
        for (size_t j = 0; j < S.size(); ++j) S[j] = mul(w[static_cast<size_t>(rig.joints[j])], rig.inv_bind[j]);
        return S;
    }
    static std::array<double, 3> lbs(const Rig& rig, const std::vector<M4>& S, size_t v) {
        std::array<double, 3> p{0, 0, 0};
        const double h[4] = {rig.rest[v][0], rig.rest[v][1], rig.rest[v][2], 1.0};
        for (int k = 0; k < 4; ++k) {
            const double wk = rig.vw[v][static_cast<size_t>(k)];
            if (wk == 0) continue;
            const M4& M = S[static_cast<size_t>(rig.vj[v][static_cast<size_t>(k)])];
            for (int r = 0; r < 3; ++r) { double s = 0; for (int c = 0; c < 4; ++c) s += M.m[c * 4 + r] * h[c]; p[static_cast<size_t>(r)] += wk * s; }
        }
        return p;
    }
};

} // namespace cycraft::omega::quantum_syntax::era
