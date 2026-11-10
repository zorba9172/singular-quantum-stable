// ============================================================================
// SINGULAR Ω — ERA benchmark & evidence emitter (`DISC-000901`)
// Same graph, same op stream, same propagation code for both number systems.
// Every figure printed or written here is measured in this process; nothing is
// a constant. Correctness invariants I1..I3 gate the exit code; speed is
// reported (including where the mechanism loses: crossover + exactness overhead).
// Rule `SG-ARCH-400`: <= 400 physical lines.
// ============================================================================
#pragma once
#include "cycraft/quantum_syntax/omega_exact_reversible_arc_engine.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#if defined(__APPLE__)
#include <sys/sysctl.h>
#endif

namespace cycraft::omega::quantum_syntax::era {

struct Op { uint8_t kind; uint32_t id; int64_t q; };

struct Workload {
    std::string name, domain;
    ArcGraph g;
    std::vector<int64_t> in_q;
    std::vector<int32_t> w_q;
    std::vector<Op> ops;
};

struct CrossoverPoint { size_t batch = 0; double incr_us = 0, full_us = 0; uint64_t touched = 0; };

struct WorkloadResult {
    std::string name, domain;
    uint32_t nodes = 0; size_t arcs = 0, ops = 0;
    double exact_incr_us_per_op = 0, fp64_incr_us_per_op = 0, exact_full_us = 0, fp64_full_us = 0;
    double static_cone_us_per_op = 0, no_adapt_us_per_op = 0;
    bool exact64_admitted = false; double exact64_incr_us_per_op = 0, exact64_full_us = 0;
    size_t exact64_rejected = 0, exact64_vs_i128_mismatch = 0;
    double touched_per_op = 0, static_touched_per_op = 0;
    size_t i1_exact_mismatch = 0, i1_fp64_mismatch = 0;  double i1_fp64_max_abs = 0;
    size_t i2_exact_mismatch = 0, i2_fp64_mismatch = 0;  double i2_fp64_max_abs = 0;
    size_t i3_exact_mismatch = 0, i3_fp64_mismatch = 0;
    double quantization_max_abs_vs_fp64 = 0;
    size_t exact_state_bytes = 0, fp64_state_bytes = 0;
    std::vector<CrossoverPoint> crossover;
    size_t crossover_batch = 0;  // first measured batch where incremental >= one full pass (0 = none)
    bool invariants_hold() const {
        return i1_exact_mismatch == 0 && i2_exact_mismatch == 0 && i3_exact_mismatch == 0 &&
               (!exact64_admitted || (exact64_rejected == 0 && exact64_vs_i128_mismatch == 0));
    }
};

class EraBenchmark {
public:
    using Clock = std::chrono::steady_clock;
    static double us_since(Clock::time_point t0) {
        return std::chrono::duration<double, std::micro>(Clock::now() - t0).count();
    }

    // Workload generator: fixed seeds make the input reproducible; it is the
    // benchmark input, not a reported value.
    static Workload make(std::string name, std::string domain, ArcGraph g, double w_amp, size_t m, uint64_t seed) {
        Workload w{std::move(name), std::move(domain), std::move(g), {}, {}, {}};
        std::mt19937_64 rng(seed);
        std::uniform_real_distribution<double> u(-1.0, 1.0), coin(0.0, 1.0);
        auto q_in = [&](double amp) { return static_cast<int64_t>(std::llround(u(rng) * amp * 0x1p32)); };
        auto q_w = [&]() { int32_t q = static_cast<int32_t>(std::lround(u(rng) * w_amp * 0x1p16)); return q == 0 ? 1 : q; };
        w.in_q.resize(w.g.n);
        for (uint32_t v = 0; v < w.g.n; ++v) w.in_q[v] = q_in(w.g.in_off[v + 1] == w.g.in_off[v] ? 1.0 : 0.25);
        w.w_q.resize(w.g.arcs());
        for (auto& x : w.w_q) x = (coin(rng) < 0.85) ? q_w() : 0;
        std::vector<int32_t> shadow = w.w_q;
        std::uniform_int_distribution<uint32_t> pv(0, w.g.n - 1);
        std::uniform_int_distribution<uint32_t> pe(0, static_cast<uint32_t>(w.g.arcs() - 1));
        w.ops.reserve(m);
        for (size_t i = 0; i < m; ++i) {
            const double r = coin(rng);
            if (r < 0.60) { const uint32_t v = pv(rng); w.ops.push_back({0, v, q_in(w.g.in_off[v + 1] == w.g.in_off[v] ? 1.0 : 0.25)}); continue; }
            const uint32_t e = pe(rng);
            const int32_t nq = (r < 0.85) ? q_w() : (shadow[e] != 0 ? 0 : q_w());  // re-weight | Δ⁺/Δ⁻ arc toggle
            shadow[e] = nq; w.ops.push_back({1, e, nq});
        }
        return w;
    }

    template <class T> static bool apply(ArcEngine<T>& eng, const Op& op) {
        return (op.kind == 0) ? eng.set_input(op.id, op.q) : eng.set_weight(op.id, static_cast<int32_t>(op.q));
    }
    template <class T> static double max_abs(const std::vector<T>& a, const std::vector<T>& b, double scale) {
        double m = 0;
        for (size_t i = 0; i < a.size(); ++i) m = std::max(m, std::abs(static_cast<double>(a[i]) - static_cast<double>(b[i])) * scale);
        return m;
    }
    // Timings report the minimum over repetitions: the host is shared, and the
    // minimum is the least noise-contaminated estimate of the same work.
    template <class T> static double time_full(ArcEngine<T> eng, int passes) {
        double best = 1e300;
        for (int p = 0; p < passes; ++p) { auto t0 = Clock::now(); eng.full_recompute(); best = std::min(best, us_since(t0)); }
        return best;
    }

    // Per-op incremental run; afterwards checks I1 (vs full recompute) and I2 (undo all).
    template <class T>
    static void run_incremental(const Workload& w, double& us_per_op, double& touched_per_op,
                                size_t& i1_mis, double& i1_abs, size_t& i2_mis, double& i2_abs, size_t& bytes, int reps, double sweep_density = 0.1) {
        ArcEngine<T> eng(w.g, w.ops.size());
        uint64_t touched = 0;
        us_per_op = 1e300;
        std::vector<typename T::Val> initial;
        for (int rp = 0; rp < reps; ++rp) {  // fresh engine per repetition; checks use the last one
            eng = ArcEngine<T>(w.g, w.ops.size());
            eng.load(w.in_q, w.w_q); eng.set_sweep_density(sweep_density);
            initial = eng.values();
            touched = 0;
            auto t0 = Clock::now();
            for (const auto& op : w.ops) { apply(eng, op); touched += eng.propagate(true); }
            us_per_op = std::min(us_per_op, us_since(t0) / static_cast<double>(w.ops.size()));
        }
        touched_per_op = static_cast<double>(touched) / static_cast<double>(w.ops.size());
        bytes = eng.state_bytes();
        const double scale = std::is_same_v<T, ExactTraits> ? 0x1p-32 : 1.0;
        ArcEngine<T> ref = eng;
        ref.full_recompute();
        i1_mis = count_mismatch(eng.values(), ref.values());
        if constexpr (std::is_same_v<T, ExactTraits>) i1_mis += count_mismatch(eng.accumulators(), ref.accumulators());
        i1_abs = max_abs(eng.values(), ref.values(), scale);
        while (eng.undo_one()) eng.propagate(true);
        i2_mis = count_mismatch(eng.values(), initial);
        i2_abs = max_abs(eng.values(), initial, scale);
    }

    // I3: the same multiset of commutative Δ-ops (input += δ) in forward vs reverse order.
    template <class T> static size_t order_independence(const Workload& w, size_t k, uint64_t seed) {
        std::mt19937_64 rng(seed);
        std::uniform_int_distribution<uint32_t> pv(0, w.g.n - 1);
        std::uniform_int_distribution<int64_t> pd(-(int64_t{1} << 30), int64_t{1} << 30);
        std::vector<std::pair<uint32_t, int64_t>> deltas(k);
        for (auto& d : deltas) d = {pv(rng), pd(rng)};
        ArcEngine<T> a(w.g, 1), b(w.g, 1);
        a.load(w.in_q, w.w_q); b.load(w.in_q, w.w_q);
        for (size_t i = 0; i < k; ++i) {
            const auto& da = deltas[i]; const auto& db = deltas[k - 1 - i];
            a.set_input(da.first, a.input_q(da.first) + da.second, false); a.propagate(true);
            b.set_input(db.first, b.input_q(db.first) + db.second, false); b.propagate(true);
        }
        return count_mismatch(a.values(), b.values());
    }

    static WorkloadResult run(const Workload& w, int reps) {
        WorkloadResult r;
        r.name = w.name; r.domain = w.domain; r.nodes = w.g.n; r.arcs = w.g.arcs(); r.ops = w.ops.size();
        double exact_i1_abs = 0, exact_i2_abs = 0, fp64_touched = 0;  // exact abs errors are implied by mismatch == 0
        run_incremental<ExactTraits>(w, r.exact_incr_us_per_op, r.touched_per_op, r.i1_exact_mismatch, exact_i1_abs,
                                     r.i2_exact_mismatch, exact_i2_abs, r.exact_state_bytes, reps);
        run_incremental<DoubleTraits>(w, r.fp64_incr_us_per_op, fp64_touched, r.i1_fp64_mismatch, r.i1_fp64_max_abs,
                                      r.i2_fp64_mismatch, r.i2_fp64_max_abs, r.fp64_state_bytes, reps);
        ArcEngine<ExactTraits> ex(w.g, 1); ex.load(w.in_q, w.w_q);
        ArcEngine<DoubleTraits> fp(w.g, 1); fp.load(w.in_q, w.w_q);
        ArcGraph real_g = w.g; real_g.frac_bits = kValFrac;  // ungridded fp64 = the real-valued model
        ArcEngine<DoubleTraits> real(real_g, 1); real.load(w.in_q, w.w_q);
        for (uint32_t v = 0; v < w.g.n; ++v)
            r.quantization_max_abs_vs_fp64 = std::max(r.quantization_max_abs_vs_fp64,
                std::abs(static_cast<double>(ex.values()[v]) * 0x1p-32 - real.values()[v]));
        r.exact_full_us = time_full(ex, 3);
        {   // int64-accumulator variant: must reproduce the i128 engine bit for bit.
            ArcEngine<Exact64Traits> e64(w.g, 1); e64.load(w.in_q, w.w_q);
            r.exact64_admitted = e64.admitted();
            if (r.exact64_admitted) {
                r.exact64_full_us = time_full(e64, 3);
                ArcEngine<ExactTraits> ref(w.g, 1); ref.load(w.in_q, w.w_q);
                for (const auto& op : w.ops) { apply(ref, op); ref.propagate(true); }
                r.exact64_incr_us_per_op = 1e300;
                for (int rp = 0; rp < reps; ++rp) {
                    ArcEngine<Exact64Traits> e(w.g, 1); e.load(w.in_q, w.w_q);
                    size_t rejected = 0;
                    auto t0 = Clock::now();
                    for (const auto& op : w.ops) { rejected += !apply(e, op); e.propagate(true); }
                    r.exact64_incr_us_per_op = std::min(r.exact64_incr_us_per_op, us_since(t0) / static_cast<double>(w.ops.size()));
                    r.exact64_rejected = rejected;
                    r.exact64_vs_i128_mismatch = count_mismatch(e.values(), ref.values());
                }
            }
        }
        r.fp64_full_us = time_full(fp, 3);
        {   // Ablation: static descendant cone (no value cut-off), exact arithmetic.
            ArcEngine<ExactTraits> ab(w.g, 1); ab.load(w.in_q, w.w_q);
            const size_t m = std::min<size_t>(w.ops.size(), 64);
            uint64_t touched = 0;
            auto t0 = Clock::now();
            for (size_t i = 0; i < m; ++i) { apply(ab, w.ops[i]); touched += ab.propagate(false); }
            r.static_cone_us_per_op = us_since(t0) / static_cast<double>(m);
            r.static_touched_per_op = static_cast<double>(touched) / static_cast<double>(m);
        }
        {   // Ablation: adaptive sweep switch disabled, same min-of-reps protocol.
            double tp = 0, a1 = 0, a2 = 0; size_t m1 = 0, m2 = 0, by = 0;
            run_incremental<ExactTraits>(w, r.no_adapt_us_per_op, tp, m1, a1, m2, a2, by, reps, 0.0);
            r.i1_exact_mismatch += m1; r.i2_exact_mismatch += m2;  // invariants must hold on this path too
        }
        r.i3_exact_mismatch = order_independence<ExactTraits>(w, 64, 0xE1A3);
        r.i3_fp64_mismatch = order_independence<DoubleTraits>(w, 64, 0xE1A3);
        // Crossover: batch of k input ops (fresh random values, same distribution as
        // the op stream), one propagate, vs one exact full pass. Min of 2 trials.
        std::mt19937_64 rng(0xC0FFEE);
        std::uniform_int_distribution<uint32_t> pv(0, w.g.n - 1);
        std::uniform_real_distribution<double> u(-1.0, 1.0);
        for (size_t k = 1; k <= w.g.n; k *= 4) {
            const int batches = k < 256 ? 4 : 1;
            CrossoverPoint cp{k, 1e300, r.exact_full_us, 0};
            for (int trial = 0; trial < 2; ++trial) {
                ArcEngine<ExactTraits> eng = ex;
                uint64_t touched = 0;
                auto t0 = Clock::now();
                for (int b = 0; b < batches; ++b) {
                    for (size_t i = 0; i < k; ++i) {
                        const uint32_t v = pv(rng);
                        const double amp = (w.g.in_off[v + 1] == w.g.in_off[v]) ? 1.0 : 0.25;
                        eng.set_input(v, static_cast<int64_t>(std::llround(u(rng) * amp * 0x1p32)), false);
                    }
                    touched += eng.propagate(true);
                }
                double us = us_since(t0) / batches;
                if (us < cp.incr_us) { cp.incr_us = us; cp.touched = touched / static_cast<uint64_t>(batches); }
            }
            r.crossover.push_back(cp);
            if (r.crossover_batch == 0 && cp.incr_us >= cp.full_us) r.crossover_batch = k;
        }
        return r;
    }

    static std::string json(const std::vector<WorkloadResult>& rs, const std::string& host, const std::string& profile = "standard") {
        std::ostringstream o;
        o.precision(10);  // default float format: keeps 1e-15-scale errors visible (never rendered as 0)
        o << "{\n  \"id\": \"DISC-000901\",\n  \"title\": \"Exact Reversible Arc Engine (ERA)\",\n"
          << "  \"profile\": \"" << profile << "\",\n"
          << "  \"status\": \"" << (all_ok(rs) ? "FUNCTION_VERIFIED" : "FAILED") << "\",\n"
          << "  \"host\": \"" << host << "\",\n  \"source\": \"singular/verification/era/era_audit.sh era-" << profile << "\",\n  \"workloads\": [\n";
        for (size_t i = 0; i < rs.size(); ++i) {
            const auto& r = rs[i];
            o << "    {\"name\": \"" << r.name << "\", \"domain\": \"" << r.domain << "\", \"nodes\": " << r.nodes
              << ", \"arcs\": " << r.arcs << ", \"ops\": " << r.ops
              << ", \"exact_incr_us_per_op\": " << r.exact_incr_us_per_op << ", \"fp64_incr_us_per_op\": " << r.fp64_incr_us_per_op
              << ", \"exact_full_pass_us\": " << r.exact_full_us << ", \"fp64_full_pass_us\": " << r.fp64_full_us
              << ", \"static_cone_us_per_op\": " << r.static_cone_us_per_op << ", \"no_adaptive_sweep_us_per_op\": " << r.no_adapt_us_per_op
              << ", \"exact64_admitted\": " << (r.exact64_admitted ? "true" : "false") << ", \"exact64_incr_us_per_op\": " << r.exact64_incr_us_per_op
              << ", \"exact64_full_pass_us\": " << r.exact64_full_us << ", \"exact64_rejected_ops\": " << r.exact64_rejected
              << ", \"exact64_vs_i128_mismatch\": " << r.exact64_vs_i128_mismatch
              << ", \"touched_per_op\": " << r.touched_per_op << ", \"static_touched_per_op\": " << r.static_touched_per_op
              << ", \"I1_exact_mismatch\": " << r.i1_exact_mismatch << ", \"I1_fp64_mismatch\": " << r.i1_fp64_mismatch
              << ", \"I1_fp64_max_abs\": " << r.i1_fp64_max_abs
              << ", \"I2_exact_mismatch\": " << r.i2_exact_mismatch << ", \"I2_fp64_mismatch\": " << r.i2_fp64_mismatch
              << ", \"I2_fp64_max_abs\": " << r.i2_fp64_max_abs
              << ", \"I3_exact_mismatch\": " << r.i3_exact_mismatch << ", \"I3_fp64_mismatch\": " << r.i3_fp64_mismatch
              << ", \"quantization_max_abs_vs_fp64\": " << r.quantization_max_abs_vs_fp64
              << ", \"exact_state_bytes\": " << r.exact_state_bytes << ", \"fp64_state_bytes\": " << r.fp64_state_bytes
              << ", \"crossover_batch\": " << r.crossover_batch << ", \"crossover\": [";
            for (size_t j = 0; j < r.crossover.size(); ++j)
                o << (j ? ", " : "") << "{\"batch\": " << r.crossover[j].batch << ", \"incr_us\": " << r.crossover[j].incr_us
                  << ", \"touched\": " << r.crossover[j].touched << "}";
            o << "]}" << (i + 1 < rs.size() ? "," : "") << "\n";
        }
        o << "  ]\n}\n";
        return o.str();
    }

    static std::string markdown(const std::vector<WorkloadResult>& rs, const std::string& host, const std::string& profile = "standard") {
        std::ostringstream o;
        o.setf(std::ios::fixed); o.precision(3);
        o << "# ERA — Exact Reversible Arc Engine: measured evidence (`DISC-000901`, profile=`" << profile << "`)\n\n"
          << "Generated by `singular/verification/era/era_audit.sh era-" << profile << "` on `" << host << "`. All numbers measured in that run.\n\n"
          << "| workload | nodes | arcs | ops | ERA incr µs/op | fp64 incr µs/op | fp64 full pass µs | ERA full pass µs | static-cone µs/op | no-adaptive µs/op | touched/op (dyn vs static) |\n"
          << "|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n";
        for (const auto& r : rs)
            o << "| " << r.name << " | " << r.nodes << " | " << r.arcs << " | " << r.ops << " | " << r.exact_incr_us_per_op
              << " | " << r.fp64_incr_us_per_op << " | " << r.fp64_full_us << " | " << r.exact_full_us << " | "
              << r.static_cone_us_per_op << " | " << r.no_adapt_us_per_op << " | " << r.touched_per_op << " vs " << r.static_touched_per_op << " |\n";
        o << "\n| workload | I1 incr≠full (ERA / fp64) | fp64 max abs | I2 undo≠initial (ERA / fp64) | fp64 max abs | I3 order-dependent nodes (ERA / fp64) | quantization max abs vs fp64 | state bytes (ERA / fp64) | crossover batch |\n"
          << "|---|---|---:|---|---:|---|---:|---|---:|\n";
        for (const auto& r : rs) {
            o << "| " << r.name << " | " << r.i1_exact_mismatch << " / " << r.i1_fp64_mismatch << " | ";
            char b1[32], b2[32], b3[32];
            std::snprintf(b1, sizeof b1, "%.3e", r.i1_fp64_max_abs);
            std::snprintf(b2, sizeof b2, "%.3e", r.i2_fp64_max_abs);
            std::snprintf(b3, sizeof b3, "%.3e", r.quantization_max_abs_vs_fp64);
            o << b1 << " | " << r.i2_exact_mismatch << " / " << r.i2_fp64_mismatch << " | " << b2 << " | "
              << r.i3_exact_mismatch << " / " << r.i3_fp64_mismatch << " | " << b3 << " | "
              << r.exact_state_bytes << " / " << r.fp64_state_bytes << " | "
              << (r.crossover_batch ? std::to_string(r.crossover_batch) : std::string("none ≤ n")) << " |\n";
        }
        o << "\n## Crossover (batched input Δ-ops, one propagate, ERA exact)\n\n| workload | batch k | k/n | incr µs | full pass µs | touched |\n|---|---:|---:|---:|---:|---:|\n";
        for (const auto& r : rs)
            for (const auto& c : r.crossover)
                o << "| " << r.name << " | " << c.batch << " | " << static_cast<double>(c.batch) / r.nodes << " | " << c.incr_us
                  << " | " << c.full_us << " | " << c.touched << " |\n";
        return o.str();
    }

    static std::string host_cpu() {
#if defined(__APPLE__)
        char buf[128] = {0}; size_t len = sizeof buf;
        if (sysctlbyname("machdep.cpu.brand_string", buf, &len, nullptr, 0) == 0) return std::string(buf);
#endif
        return "UNKNOWN";
    }

    static bool all_ok(const std::vector<WorkloadResult>& rs) {
        for (const auto& r : rs) if (!r.invariants_hold()) return false;
        return !rs.empty();
    }

    static Workload with_grid(const Workload& base, uint8_t frac_bits) {
        Workload w = base; w.g.frac_bits = frac_bits;
        w.name = base.name + "-q" + std::to_string(frac_bits);
        return w;
    }

    struct ProfileSpec { const char* name; uint32_t dag_n, net_w, net_fanin; size_t m; int reps; bool emit; };
    static ProfileSpec resolve_profile(const std::string& p) {
        if (p == "quick" || p == "era-quick" || p == "--quick") return {"quick", 16384, 1024, 12, 250, 1, false};
        if (p == "research" || p == "era-research" || p == "--research") return {"research", 200000, 16384, 16, 3000, 2, true};
        return {"standard", 32768, 2048, 12, 500, 2, true};
    }
    static int run_cli(const std::string& root, bool quick) { return run_cli_profile(root, quick ? "quick" : "standard"); }
    static int run_cli_profile(std::string root, std::string prof_arg = "standard") {
        if (root == "quick" || root == "standard" || root == "research" || root == "--quick" || root == "--standard" || root == "--research") {
            prof_arg = root; root = ".";
        }
        const ProfileSpec pf = resolve_profile(prof_arg);
        const Workload a = make("A-dataflow-DAG", "incremental dataflow / spreadsheet / feature store",
                                ArcGraph::random_dag(pf.dag_n, 4, 2048, 0xA11CE), 0.6, pf.m, 0xA1);
        const Workload b = make("B-sparse-ReLU-net", "cross-domain: incremental neural inference",
                                ArcGraph::layered_net(8, pf.net_w, pf.net_fanin, 0xB0B), 0.35, pf.m, 0xB1);
        std::vector<Workload> ws;
        for (uint8_t bits : {32, 16, 8}) ws.push_back(with_grid(a, bits));
        for (uint8_t bits : {32, 8}) ws.push_back(with_grid(b, bits));
        std::vector<WorkloadResult> rs;
        for (const auto& w : ws) {
            std::cout << "[ERA][" << pf.name << "] running " << w.name << " (n=" << w.g.n << ", arcs=" << w.g.arcs() << ", ops=" << w.ops.size() << ")\n" << std::flush;
            rs.push_back(run(w, pf.reps));
            const auto& r = rs.back();
            std::printf("  incr µs/op: ERA=%.3f fp64=%.3f | full pass µs: ERA=%.1f fp64=%.1f | static-cone µs/op=%.3f | no-adaptive µs/op=%.3f\n",
                        r.exact_incr_us_per_op, r.fp64_incr_us_per_op, r.exact_full_us, r.fp64_full_us, r.static_cone_us_per_op, r.no_adapt_us_per_op);
            std::printf("  touched/op: dynamic=%.1f static=%.1f | I1 mismatch ERA=%zu fp64=%zu (max %.3e)\n",
                        r.touched_per_op, r.static_touched_per_op, r.i1_exact_mismatch, r.i1_fp64_mismatch, r.i1_fp64_max_abs);
            std::printf("  I2 undo-all mismatch ERA=%zu fp64=%zu (max %.3e) | I3 order mismatch ERA=%zu fp64=%zu\n",
                        r.i2_exact_mismatch, r.i2_fp64_mismatch, r.i2_fp64_max_abs, r.i3_exact_mismatch, r.i3_fp64_mismatch);
            if (r.exact64_admitted)
                std::printf("  int64-acc: incr µs/op=%.3f full pass µs=%.1f | rejected=%zu | mismatch vs i128=%zu\n",
                            r.exact64_incr_us_per_op, r.exact64_full_us, r.exact64_rejected, r.exact64_vs_i128_mismatch);
            std::printf("  quantization max |ERA-fp64| = %.3e | state bytes ERA=%zu fp64=%zu | crossover batch=%zu\n",
                        r.quantization_max_abs_vs_fp64, r.exact_state_bytes, r.fp64_state_bytes, r.crossover_batch);
        }
        const std::string host = host_cpu() + " / " + __VERSION__;
        if (pf.emit) {
            namespace fs = std::filesystem;
            const std::string jstr = json(rs, host, pf.name), mstr = markdown(rs, host, pf.name);
            for (const std::string& rt : {root, std::string("/Volumes/Lenovo PS8S/workspace/Supercomputing")}) {
                if (!fs::exists(rt)) continue;
                const fs::path disc = fs::path(rt) / "singular/discoveries/registry/DISC-000901";
                const fs::path rep = fs::path(rt) / "singular/verification/reports";
                std::error_code ec;
                fs::create_directories(disc, ec); fs::create_directories(rep, ec);
                std::ofstream(disc / "evidence.json") << jstr;
                std::ofstream(rep / "ERA_EXACT_REVERSIBLE_ARC_EVIDENCE.md") << mstr;
                if (std::string(pf.name) == "research") std::ofstream(rep / "ERA_RESEARCH_FULL_VALIDATION.json") << jstr;
            }
            std::cout << "[ERA][" << pf.name << "] wrote evidence.json & ERA_EXACT_REVERSIBLE_ARC_EVIDENCE.md\n";
        }
        const bool ok = all_ok(rs);
        std::cout << "[ERA][" << pf.name << "] invariants I1/I2/I3 (exact): " << (ok ? "HOLD" : "VIOLATED") << "\n";
        return ok ? 0 : 1;
    }
};

} // namespace cycraft::omega::quantum_syntax::era
