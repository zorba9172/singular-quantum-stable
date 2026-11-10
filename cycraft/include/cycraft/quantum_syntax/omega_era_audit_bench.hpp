// ============================================================================
// SINGULAR Ω — ERA audit: robust statistics, clean interleaved benchmark and a
// real-weights workload (`DISC-000901`). Rule `SG-ARCH-400`: <= 400 lines.
//
// Statistics: per-op latencies are pooled per variant (median, p95); per-trial
// mean cost gives one A/B ratio per trial against ERA; we report the median ratio
// and a percentile-bootstrap 95% CI over trials. Variant order rotates each trial
// so slow host drift hits every variant equally. Host load (getloadavg) is recorded.
// ============================================================================
#pragma once
#include "cycraft/quantum_syntax/omega_era_kernel.hpp"
#include "cycraft/quantum_syntax/omega_exact_reversible_arc_benchmark.hpp"
#include <array>
#include <cstdlib>
#include <cstring>
#include <sys/resource.h>

namespace cycraft::omega::quantum_syntax::era {

struct Dist { double median = 0, p95 = 0; };
struct Ratio { double median = 0, ci_lo = 0, ci_hi = 0; };

inline double quantile(std::vector<double> v, double q) {
    if (v.empty()) return 0;
    const size_t k = static_cast<size_t>(q * static_cast<double>(v.size() - 1) + 0.5);
    std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(k), v.end());
    return v[k];
}
inline Ratio bootstrap_median(const std::vector<double>& x, uint64_t seed) {
    Ratio r; r.median = quantile(x, 0.5);
    if (x.size() < 2) { r.ci_lo = r.ci_hi = r.median; return r; }
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<size_t> pick(0, x.size() - 1);
    std::vector<double> meds(2000), s(x.size());
    for (auto& m : meds) { for (auto& e : s) e = x[pick(rng)]; m = quantile(s, 0.5); }
    r.ci_lo = quantile(meds, 0.025); r.ci_hi = quantile(meds, 0.975);
    return r;
}
inline double host_load1() { double l[3] = {0, 0, 0}; return getloadavg(l, 3) > 0 ? l[0] : -1.0; }
inline double cpu_seconds() {  // user + system CPU time of this process
    rusage u{}; getrusage(RUSAGE_SELF, &u);
    return static_cast<double>(u.ru_utime.tv_sec + u.ru_stime.tv_sec) + 1e-6 * static_cast<double>(u.ru_utime.tv_usec + u.ru_stime.tv_usec);
}
inline size_t peak_rss_bytes() { rusage u{}; getrusage(RUSAGE_SELF, &u); return static_cast<size_t>(u.ru_maxrss); }  // macOS: bytes

enum VariantId { kFp64Full = 0, kIntFull, kFp64Incr, kIntStaticCone, kEra, kIntFullParallel, kNumVariants };
inline const char* variant_name(int v) {
    static const char* n[] = {"fp64 full recompute / op", "integer full recompute / op (same semantics)",
                              "fp64 incremental (same code)", "integer incremental, static cone (same semantics)",
                              "ERA kernel (int64→i128, value cut-off, adaptive sweep)",
                              "integer full recompute, level-parallel 4 threads (same semantics)"};
    return n[v];
}

struct CleanBenchResult {
    std::string name;
    uint32_t nodes = 0; size_t arcs = 0, ops = 0; int trials = 0;
    double load_before = 0, load_after = 0;
    std::array<Dist, kNumVariants> per_op{};
    std::array<bool, kNumVariants> enabled{};
    std::array<double, kNumVariants> cpu_us_per_op{};  // user+sys CPU time (energy proxy; no privileged power counters)
    std::array<std::vector<float>, kNumVariants> samples;  // raw per-op latencies (µs), capped at 20k per variant
    std::array<Ratio, kNumVariants> ratio_vs_era{};  // >1 means ERA is faster
    size_t era_oracle_mismatch = 0;
    bool era_used_int64 = false;
    size_t era_state_bytes = 0, fp64_state_bytes = 0;  // engine memory incl. sketch, journal (cap 1)
};

class EraCleanBench {
public:
    using Clock = std::chrono::steady_clock;
    // An "op" is a batch of Op (size 1 for single edits, e.g. 256 for a token swap).
    using Batch = std::vector<Op>;

    template <class E> static void apply_batch(E& e, const Batch& b) {
        for (const Op& op : b) {
            if (op.kind == 0) e.set_input(op.id, op.q, false); else e.set_weight(op.id, static_cast<int32_t>(op.q), false);
        }
    }
    static double now_us(Clock::time_point t0) { return std::chrono::duration<double, std::micro>(Clock::now() - t0).count(); }

    static std::vector<double> run_variant(int v, const ArcGraph& g, const std::vector<int64_t>& in_q,
                                           const std::vector<int32_t>& w_q, const std::vector<Batch>& ops,
                                           CleanBenchResult& res) {
        std::vector<double> lat; lat.reserve(ops.size());
        auto timed = [&](auto&& body) { for (const auto& b : ops) { auto t0 = Clock::now(); body(b); lat.push_back(now_us(t0)); } };
        if (v == kFp64Full || v == kFp64Incr) {
            ArcEngine<DoubleTraits> e(g, 1); e.load(in_q, w_q);
            res.fp64_state_bytes = e.state_bytes();
            if (v == kFp64Full) timed([&](const Batch& b) { apply_batch(e, b); e.full_recompute(); });
            else timed([&](const Batch& b) { apply_batch(e, b); e.propagate(true); });
        } else if (v == kIntFull) {
            EraKernel e(g, 1); e.load(in_q, w_q);
            timed([&](const Batch& b) { apply_batch(e, b); e.full_recompute(); });
        } else if (v == kIntFullParallel) {
            if (exact64_admissible(g, in_q, w_q)) { ParallelFull<Exact64Traits> e(g, 4); e.load(in_q, w_q); timed([&](const Batch& b) { apply_batch(e, b); e.full_recompute(); }); }
            else { ParallelFull<ExactTraits> e(g, 4); e.load(in_q, w_q); timed([&](const Batch& b) { apply_batch(e, b); e.full_recompute(); }); }
        } else if (v == kIntStaticCone) {
            ArcEngine<Exact64Traits> e64(g, 1); e64.load(in_q, w_q);
            if (e64.admitted()) timed([&](const Batch& b) { apply_batch(e64, b); e64.propagate(false); });
            else { ArcEngine<ExactTraits> e(g, 1); e.load(in_q, w_q); timed([&](const Batch& b) { apply_batch(e, b); e.propagate(false); }); }
        } else {
            EraKernel e(g, 1); e.load(in_q, w_q);
            timed([&](const Batch& b) { apply_batch(e, b); e.propagate(); });
            res.era_used_int64 = !e.wide(); res.era_state_bytes = e.state_bytes();
            const auto ref = EraOracle::evaluate(g, e.inputs(), e.weights());
            res.era_oracle_mismatch += ref.empty() ? g.n : count_mismatch(e.values(), ref);
        }
        return lat;
    }

    static CleanBenchResult run(const std::string& name, const ArcGraph& g, const std::vector<int64_t>& in_q,
                                const std::vector<int32_t>& w_q, const std::vector<Batch>& ops, int trials,
                                const std::array<bool, kNumVariants>& enabled) {
        CleanBenchResult r;
        r.name = name; r.nodes = g.n; r.arcs = g.arcs(); r.ops = ops.size(); r.trials = trials;
        r.load_before = host_load1();
        r.enabled = enabled;
        std::array<std::vector<double>, kNumVariants> pooled, trial_mean;
        for (int t = 0; t < trials; ++t)
            for (int k = 0; k < kNumVariants; ++k) {
                const int v = (k + t) % kNumVariants;  // rotate order
                if (!enabled[v]) continue;
                const double cpu0 = cpu_seconds();
                auto lat = run_variant(v, g, in_q, w_q, ops, r);
                r.cpu_us_per_op[v] += (cpu_seconds() - cpu0) * 1e6 / static_cast<double>(ops.size() * static_cast<size_t>(trials));
                double s = 0; for (double x : lat) s += x;
                trial_mean[v].push_back(s / static_cast<double>(lat.size()));
                pooled[v].insert(pooled[v].end(), lat.begin(), lat.end());
            }
        r.load_after = host_load1();
        for (int v = 0; v < kNumVariants; ++v) {
            if (!enabled[v]) continue;
            r.per_op[v] = {quantile(pooled[v], 0.5), quantile(pooled[v], 0.95)};
            r.samples[v].assign(pooled[v].begin(), pooled[v].begin() + static_cast<std::ptrdiff_t>(std::min<size_t>(pooled[v].size(), 20000)));
            std::vector<double> ratios;
            for (size_t t = 0; t < trial_mean[v].size() && t < trial_mean[kEra].size(); ++t)
                ratios.push_back(trial_mean[v][t] / trial_mean[kEra][t]);
            r.ratio_vs_era[v] = bootstrap_median(ratios, 0xB007 + static_cast<uint64_t>(v));
        }
        return r;
    }
};

// ---------------------------------------------------------------------------
// Real-weights workload: dense projection matrices read from a safetensors file
// (F32). The network is a SIMPLIFICATION of the source model: the up/down MLP
// projections of the first L layers chained with a hard-tanh activation; no
// gating, norms, attention or residuals. Quality is measured against fp64 on the
// same simplified network with the ORIGINAL float weights.
// ---------------------------------------------------------------------------
struct RealModel {
    bool ok = false;
    std::string path, error;
    ArcGraph g;
    std::vector<int32_t> w_q;
    std::vector<float> w_f;              // original weights, same arc order
    std::vector<std::vector<float>> emb; // token embedding rows (subset)
    uint32_t in_dim = 0, out_base = 0, out_dim = 0;
    double max_abs_weight = 0, max_abs_embedding = 0;
};

class RealModelLoader {
public:
    struct Tensor { size_t begin = 0, end = 0; std::vector<size_t> shape; bool f32 = false; };
    static bool find_tensor(const std::string& hdr, const std::string& name, Tensor& t) {
        const size_t k = hdr.find("\"" + name + "\"");
        if (k == std::string::npos) return false;
        const size_t blk_end = hdr.find('}', k);
        const std::string blk = hdr.substr(k, blk_end - k);
        t.f32 = blk.find("\"F32\"") != std::string::npos;
        auto nums = [&](const char* key) {
            std::vector<size_t> out;
            size_t p = blk.find(key);
            if (p == std::string::npos) return out;
            p = blk.find('[', p);
            const size_t q = blk.find(']', p);
            std::string s = blk.substr(p + 1, q - p - 1);
            for (char& c : s) if (c == ',') c = ' ';
            std::istringstream is(s); size_t x; while (is >> x) out.push_back(x);
            return out;
        };
        t.shape = nums("\"shape\"");
        const auto off = nums("\"data_offsets\"");
        if (off.size() != 2) return false;
        t.begin = off[0]; t.end = off[1];
        return true;
    }

    static RealModel load(const std::string& path, int layers, uint8_t frac_bits, size_t tokens) {
        RealModel m; m.path = path;
        std::ifstream f(path, std::ios::binary);
        if (!f) { m.error = "cannot open " + path; return m; }
        uint64_t hlen = 0; f.read(reinterpret_cast<char*>(&hlen), 8);
        if (!f || hlen == 0 || hlen > (64u << 20)) { m.error = "bad safetensors header"; return m; }
        std::string hdr(hlen, '\0'); f.read(hdr.data(), static_cast<std::streamsize>(hlen));
        const size_t base = 8 + hlen;
        auto read_f32 = [&](const Tensor& t, std::vector<float>& out) {
            out.resize((t.end - t.begin) / 4);
            f.seekg(static_cast<std::streamoff>(base + t.begin));
            f.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(t.end - t.begin));
            return static_cast<bool>(f);
        };
        Tensor te;
        if (!find_tensor(hdr, "embed_tokens.weight", te) || !te.f32 || te.shape.size() != 2) { m.error = "embed_tokens.weight missing/not F32"; return m; }
        std::vector<float> emb_all;
        if (!read_f32(te, emb_all)) { m.error = "embedding read failed"; return m; }
        m.in_dim = static_cast<uint32_t>(te.shape[1]);
        for (size_t t = 0; t < tokens && t < te.shape[0]; ++t) {
            const size_t row = (t * 7919u + 13u) % te.shape[0];  // fixed, spread token ids
            m.emb.emplace_back(emb_all.begin() + static_cast<std::ptrdiff_t>(row * m.in_dim),
                               emb_all.begin() + static_cast<std::ptrdiff_t>((row + 1) * m.in_dim));
            for (float x : m.emb.back()) m.max_abs_embedding = std::max(m.max_abs_embedding, std::abs(static_cast<double>(x)));
        }
        uint32_t cur_base = 0, cur_dim = m.in_dim, next_base = m.in_dim;
        for (int l = 0; l < layers; ++l)
            for (const char* proj : {"up_proj", "down_proj"}) {
                Tensor tw;
                const std::string name = "layers." + std::to_string(l) + ".mlp." + proj + ".weight";
                if (!find_tensor(hdr, name, tw) || !tw.f32 || tw.shape.size() != 2 || tw.shape[1] != cur_dim) { m.error = name + " missing/mismatch"; return m; }
                std::vector<float> W;
                if (!read_f32(tw, W)) { m.error = name + " read failed"; return m; }
                const uint32_t out = static_cast<uint32_t>(tw.shape[0]);
                for (uint32_t o = 0; o < out; ++o)
                    for (uint32_t i = 0; i < cur_dim; ++i) {
                        const float x = W[static_cast<size_t>(o) * cur_dim + i];
                        m.g.e_src.push_back(cur_base + i); m.g.e_dst.push_back(next_base + o);
                        m.w_f.push_back(x);
                        m.w_q.push_back(static_cast<int32_t>(std::lround(static_cast<double>(x) * 0x1p16)));
                        m.max_abs_weight = std::max(m.max_abs_weight, std::abs(static_cast<double>(x)));
                    }
                cur_base = next_base; cur_dim = out; next_base += out;
            }
        m.g.n = next_base; m.g.act = Act::HardTanh; m.g.frac_bits = frac_bits;
        m.g.finalize();
        m.out_base = cur_base; m.out_dim = cur_dim;
        m.ok = true;
        return m;
    }

    static int64_t q32(float x) { return std::llround(std::clamp(static_cast<double>(x), -8.0, 8.0) * 0x1p32); }

    // fp64 reference of the same simplified network with the original float weights
    // and no output grid (node ids are topological: src < dst).
    static std::vector<double> reference(const RealModel& m, const std::vector<float>& x) {
        std::vector<double> val(m.g.n, 0.0);
        for (uint32_t v = 0; v < m.g.n; ++v) {
            if (v < m.in_dim) { val[v] = std::clamp(static_cast<double>(x[v]), -8.0, 8.0); continue; }
            double acc = 0.0;
            for (uint32_t k = m.g.in_off[v]; k < m.g.in_off[v + 1]; ++k) {
                const uint32_t e = m.g.in_eid[k];
                acc += static_cast<double>(m.w_f[e]) * val[m.g.e_src[e]];
            }
            val[v] = std::clamp(acc, -8.0, 8.0);
        }
        return val;
    }
};

} // namespace cycraft::omega::quantum_syntax::era
