// ============================================================================
// SINGULAR Ω — ERA skinning experiment (`DISC-000901`, product track).
// Maps a real rigged glTF asset onto ERA and measures, on real pose edits:
// speed vs 4 alternatives (median/p95/bootstrap CI), quality vs fp64 LBS, undo-all
// bit-identity and agreement with the independent oracle.
// Rule `SG-ARCH-400`: <= 400 physical lines.
// ============================================================================
#pragma once
#include "cycraft/quantum_syntax/omega_era_skinning.hpp"

namespace cycraft::omega::quantum_syntax::era {

struct SkinReport {
    bool ok = false; std::string path, error;
    size_t joints = 0, vertices = 0, pose_edits = 0; uint32_t nodes = 0; size_t arcs = 0;
    double scale = 1, mean_changed_inputs = 0, mean_touched_fraction = 0;
    double max_err_asset = 0, max_err_norm = 0, asset_extent = 0;
    size_t undo_mismatch = 0, oracle_mismatch = 0, out_of_range_inputs = 0;
    CleanBenchResult bench;
};

class EraSkinning {
public:
    static int64_t q32(double x) { return std::llround(x * 0x1p32); }

    static ArcGraph build(const Rig& rig, std::vector<int32_t>& w_q) {
        ArcGraph g;
        const uint32_t J = static_cast<uint32_t>(rig.joints.size()), V = static_cast<uint32_t>(rig.rest.size());
        g.n = 12 * J + 3 * V; g.act = Act::HardTanh; g.frac_bits = 32;
        for (uint32_t v = 0; v < V; ++v) {
            const double h[4] = {rig.rest[v][0] * rig.scale, rig.rest[v][1] * rig.scale, rig.rest[v][2] * rig.scale, 1.0};
            for (int k = 0; k < 4; ++k) {
                const double wk = rig.vw[v][static_cast<size_t>(k)];
                if (wk == 0) continue;
                const uint32_t j = static_cast<uint32_t>(rig.vj[v][static_cast<size_t>(k)]);
                for (uint32_t r = 0; r < 3; ++r)
                    for (uint32_t c = 0; c < 4; ++c) {
                        g.e_src.push_back(j * 12 + r * 4 + c); g.e_dst.push_back(12 * J + 3 * v + r);
                        w_q.push_back(static_cast<int32_t>(std::lround(wk * h[c] * 0x1p16)));
                    }
            }
        }
        g.finalize();
        return g;
    }
    // Normalized skinning-matrix entries: rotation/scale part unchanged, translation × scale.
    static std::vector<int64_t> inputs(const Rig& rig, const std::vector<M4>& S, uint32_t n, size_t& out_of_range) {
        std::vector<int64_t> in_q(n, 0);
        for (size_t j = 0; j < S.size(); ++j)
            for (int r = 0; r < 3; ++r)
                for (int c = 0; c < 4; ++c) {
                    const double x = (c < 3) ? S[j].m[c * 4 + r] : S[j].m[12 + r] * rig.scale;
                    if (std::abs(x) > 8.0) ++out_of_range;
                    in_q[j * 12 + static_cast<size_t>(r * 4 + c)] = q32(std::clamp(x, -8.0, 8.0));
                }
        return in_q;
    }
    static M4 axis_angle(double ax, double ay, double az, double ang) {
        const double n = std::sqrt(ax * ax + ay * ay + az * az), s = std::sin(ang / 2) / n;
        const double t[3] = {0, 0, 0}, q[4] = {ax * s, ay * s, az * s, std::cos(ang / 2)}, sc[3] = {1, 1, 1};
        return trs(t, q, sc);
    }

    // Pose-edit stream: each edit rotates one joint's local transform; the
    // matrices of its subtree change and become one batch of input ops.
    // `posed` ends in the final pose; skin[e] holds the fp64 skinning matrices
    // after edit e (asset units) for the fp64 reference pipeline.
    struct PoseStream { std::vector<EraCleanBench::Batch> batches; std::vector<std::vector<M4>> skin; double mean_changed_inputs = 0; };
    static PoseStream pose_stream(const Rig& rig, uint32_t n, const std::vector<int64_t>& in0, size_t edits, uint64_t seed,
                                  Rig& posed, size_t& out_of_range) {
        PoseStream ps;
        std::mt19937_64 rng(seed);
        std::uniform_real_distribution<double> u(-1.0, 1.0);
        std::vector<int64_t> cur = in0;
        double changed = 0;
        for (size_t i = 0; i < edits; ++i) {
            const size_t node = static_cast<size_t>(rig.joints[rng() % rig.joints.size()]);
            posed.node_local[node] = mul(posed.node_local[node], axis_angle(u(rng), u(rng), u(rng) + 1e-3, 0.25 * u(rng)));
            ps.skin.push_back(RigLoader::skin_matrices(posed));
            const auto next = inputs(posed, ps.skin.back(), n, out_of_range);
            EraCleanBench::Batch b;
            for (uint32_t k = 0; k < 12 * rig.joints.size(); ++k) if (next[k] != cur[k]) b.push_back(Op{0, k, next[k]});
            changed += static_cast<double>(b.size());
            cur = next;
            ps.batches.push_back(std::move(b));
        }
        ps.mean_changed_inputs = edits ? changed / static_cast<double>(edits) : 0;
        return ps;
    }

    static SkinReport run(const std::string& path, size_t edits, int trials, uint64_t seed) {
        SkinReport rep; rep.path = path;
        Rig rig = RigLoader::load_glb(path);
        if (!rig.ok) { rep.error = rig.error; return rep; }
        rep.ok = true; rep.joints = rig.joints.size(); rep.vertices = rig.rest.size(); rep.scale = rig.scale;
        rep.asset_extent = 4.0 / rig.scale;
        std::vector<int32_t> w_q;
        const ArcGraph g = build(rig, w_q);
        rep.nodes = g.n; rep.arcs = g.arcs();
        const std::vector<int64_t> in0 = inputs(rig, RigLoader::skin_matrices(rig), g.n, rep.out_of_range_inputs);
        Rig posed = rig;
        const PoseStream ps = pose_stream(rig, g.n, in0, edits, seed, posed, rep.out_of_range_inputs);
        const std::vector<EraCleanBench::Batch>& ops = ps.batches;
        rep.pose_edits = ops.size();
        rep.mean_changed_inputs = ps.mean_changed_inputs;
        // Single journaled ERA run: touched fraction, quality, oracle, undo-all.
        {
            EraKernel k(g, ops.size() * 12 * rig.joints.size() + 8);
            k.load(in0, w_q);
            const auto initial = k.values();
            double touched = 0;
            for (const auto& b : ops) { for (const Op& op : b) k.set_input(op.id, op.q); touched += static_cast<double>(k.propagate()); }
            rep.mean_touched_fraction = touched / static_cast<double>(ops.size()) / static_cast<double>(g.n);
            const auto ref = EraOracle::evaluate(g, k.inputs(), k.weights());
            rep.oracle_mismatch = ref.empty() ? g.n : count_mismatch(k.values(), ref);
            const auto S = RigLoader::skin_matrices(posed);
            const uint32_t base = 12 * static_cast<uint32_t>(rig.joints.size());
            for (size_t v = 0; v < rig.rest.size(); ++v) {
                const auto p = RigLoader::lbs(rig, S, v);  // fp64, original float data, asset units
                for (int r = 0; r < 3; ++r) {
                    const double era_norm = static_cast<double>(k.values()[base + 3 * v + static_cast<size_t>(r)]) * 0x1p-32;
                    rep.max_err_norm = std::max(rep.max_err_norm, std::abs(era_norm - p[static_cast<size_t>(r)] * rig.scale));
                }
            }
            rep.max_err_asset = rep.max_err_norm / rig.scale;
            while (k.undo_one()) k.propagate();
            rep.undo_mismatch = count_mismatch(k.values(), initial) + (k.inputs() != in0);
        }
        const std::array<bool, kNumVariants> all{true, true, true, true, true};
        rep.bench = EraCleanBench::run("D-skinning-" + std::filesystem::path(path).stem().string(), g, in0, w_q, ops, trials, all);
        rep.bench.era_oracle_mismatch += rep.oracle_mismatch;
        return rep;
    }
};

} // namespace cycraft::omega::quantum_syntax::era
