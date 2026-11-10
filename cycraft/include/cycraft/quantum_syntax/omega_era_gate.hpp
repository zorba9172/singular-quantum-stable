// ============================================================================
// SINGULAR Ω — ERA acceptance gate (`singular/verification/era/era_audit.sh era-gate`)
// Five tests requested by the owner (2026-11-10), one engine (ERA fabric):
//   G1 scalability ×1/×10/×100/×1000 (memory-bounded), G2 exact break-even
//   fraction, G3 distributed reproducibility incl. node crash + recovery,
//   G4 long random sequences (overflow/saturation/quantization/undo/rollback),
//   G5 competitive benchmark incl. an optimized same-semantics competitor
//   (level-parallel full recompute), CPU time, memory and data-transfer cost.
// Raw per-op samples go to CSV; provenance is written by era_audit.sh.
// Rule `SG-ARCH-400`: <= 400 physical lines.
// ============================================================================
#pragma once
#include "cycraft/quantum_syntax/omega_era_audit.hpp"

namespace cycraft::omega::quantum_syntax::era {

struct ScaleRow { std::string workload; uint32_t mult = 0; CleanBenchResult b; size_t peak_rss = 0; double changed_per_op = 0; };
struct BreakEven {
    std::string workload; uint32_t n = 0;
    std::vector<double> f, era_us, full_us, par_us;
    double fstar_full = -1, fstar_par = -1;  // -1: ERA cheaper on the whole grid
};
struct LongRun {
    size_t cases = 0, ops = 0, checks = 0, mismatches = 0, rollbacks = 0, rollback_mismatch = 0, undos = 0, promotions = 0, saturated = 0;
    bool passed() const { return checks > 0 && mismatches == 0 && rollback_mismatch == 0; }
};

class EraGate {
public:
    static Workload make_wl(char kind, uint32_t n, uint8_t bits, size_t ops) {
        Workload w = (kind == 'A')
            ? EraBenchmark::make("A-dataflow-DAG", "", ArcGraph::random_dag(n, 4, 2048, 0xA11CE), 0.6, ops, 0xA1)
            : EraBenchmark::make("B-sparse-ReLU-net", "", ArcGraph::layered_net(8, n / 8, 12, 0xB0B), 0.35, ops, 0xB1);
        return EraBenchmark::with_grid(w, bits);
    }

    // G1 + G5: the same clean bench (6 variants) across sizes.
    static std::vector<ScaleRow> scalability(bool quick) {
        std::vector<ScaleRow> rows;
        struct Spec { char kind; uint8_t bits; uint32_t max_mult; };
        // B is capped at ×100: ×1000 would need ~50M arcs (> 1.6 GB per engine copy) next to other agents' builds.
        const Spec specs[] = {{'A', 8, quick ? 10u : 1000u}, {'B', 32, quick ? 10u : 100u}, {'A', 32, quick ? 10u : 100u}};
        const std::array<bool, kNumVariants> all{true, true, true, true, true, true};
        for (const Spec& sp : specs)
            for (uint32_t mult = 1; mult <= sp.max_mult; mult *= 10) {
                const uint32_t n = 4096 * mult;
                const size_t ops = mult >= 1000 ? 40 : (mult >= 100 ? 100 : 200);
                const int trials = mult >= 1000 ? 3 : 5;
                const Workload w = make_wl(sp.kind, n, sp.bits, ops);
                std::printf("  [G1] %s ×%u (n=%u, arcs=%zu, ops=%zu, trials=%d)\n", w.name.c_str(), mult, w.g.n, w.g.arcs(), ops, trials);
                std::fflush(stdout);
                ScaleRow r; r.workload = w.name; r.mult = mult;
                r.b = EraCleanBench::run(w.name + "-x" + std::to_string(mult), w.g, w.in_q, w.w_q, EraAudit::single_batches(w), trials, all);
                r.peak_rss = peak_rss_bytes();
                {   // values changed per op (for state-delta transfer cost)
                    EraKernel k(w.g, 1); k.load(w.in_q, w.w_q);
                    double ch = 0;
                    for (const auto& op : w.ops) { EraCleanBench::apply_batch(k, {op}); k.propagate(); ch += static_cast<double>(k.last_changed()); }
                    r.changed_per_op = ch / static_cast<double>(w.ops.size());
                }
                rows.push_back(std::move(r));
            }
        return rows;
    }

    // G2: batch of k = f·n fresh input values, one propagate, vs full recompute.
    static BreakEven break_even(char kind, uint8_t bits, uint32_t n, bool quick) {
        BreakEven be;
        const Workload w = make_wl(kind, n, bits, 1);
        be.workload = w.name; be.n = w.g.n;
        EraKernel base(w.g, 1); base.load(w.in_q, w.w_q);
        const bool i64 = exact64_admissible(w.g, w.in_q, w.w_q);
        std::mt19937_64 rng(0xBE);
        std::uniform_int_distribution<uint32_t> pv(0, w.g.n - 1);
        std::uniform_real_distribution<double> u(-1.0, 1.0);
        const int trials = quick ? 2 : 5;
        const std::vector<double> grid = quick ? std::vector<double>{1e-4, 1e-2, 0.3} :
            std::vector<double>{1e-5, 3e-5, 1e-4, 3e-4, 1e-3, 3e-3, 1e-2, 3e-2, 0.1, 0.2, 0.3, 0.5, 1.0};
        ParallelFull<ExactTraits> par128(w.g, 4);
        ParallelFull<Exact64Traits> par64(w.g, 4);
        if (i64) par64.load(w.in_q, w.w_q); else par128.load(w.in_q, w.w_q);
        for (double f : grid) {
            const size_t k = std::max<size_t>(1, static_cast<size_t>(std::llround(f * w.g.n)));
            std::vector<double> te, tf, tp;
            for (int t = 0; t < trials; ++t) {
                EraCleanBench::Batch b;
                for (size_t i = 0; i < k; ++i) {
                    const uint32_t v = pv(rng);
                    const double amp = (w.g.in_off[v + 1] == w.g.in_off[v]) ? 1.0 : 0.25;
                    b.push_back(Op{0, v, std::llround(u(rng) * amp * 0x1p32)});
                }
                EraKernel ke = base; EraKernel kf = base;
                auto t0 = EraCleanBench::Clock::now(); EraCleanBench::apply_batch(ke, b); ke.propagate(); te.push_back(EraCleanBench::now_us(t0));
                t0 = EraCleanBench::Clock::now(); EraCleanBench::apply_batch(kf, b); kf.full_recompute(); tf.push_back(EraCleanBench::now_us(t0));
                if (i64) { t0 = EraCleanBench::Clock::now(); EraCleanBench::apply_batch(par64, b); par64.full_recompute(); tp.push_back(EraCleanBench::now_us(t0)); par64.load(w.in_q, w.w_q); }
                else { t0 = EraCleanBench::Clock::now(); EraCleanBench::apply_batch(par128, b); par128.full_recompute(); tp.push_back(EraCleanBench::now_us(t0)); par128.load(w.in_q, w.w_q); }
            }
            be.f.push_back(f); be.era_us.push_back(quantile(te, 0.5)); be.full_us.push_back(quantile(tf, 0.5)); be.par_us.push_back(quantile(tp, 0.5));
        }
        auto cross = [&](const std::vector<double>& other) {
            for (size_t i = 0; i < be.f.size(); ++i)
                if (be.era_us[i] >= other[i]) {
                    if (i == 0) return be.f[0];  // ERA already slower at the smallest batch
                    const double r0 = std::log(be.era_us[i - 1] / other[i - 1]), r1 = std::log(be.era_us[i] / other[i]);
                    const double a = (r1 == r0) ? 1.0 : -r0 / (r1 - r0);  // log-linear interpolation of the ratio crossing 1
                    return std::exp(std::log(be.f[i - 1]) + a * (std::log(be.f[i]) - std::log(be.f[i - 1])));
                }
            return -1.0;
        };
        be.fstar_full = cross(be.full_us); be.fstar_par = cross(be.par_us);
        return be;
    }

    // G4: long random sequences with checkpoints and rollback, saturation-heavy
    // inputs, extreme grids and int64 boundary weights; oracle every 512 ops.
    static LongRun long_run(size_t cases, size_t ops, uint64_t seed) {
        LongRun lr;
        std::mt19937_64 rng(seed);
        auto U = [&](uint64_t lo, uint64_t hi) { return lo + rng() % (hi - lo + 1); };
        std::uniform_real_distribution<double> u(-1.0, 1.0);
        for (size_t c = 0; c < cases; ++c) {
            const uint32_t n = static_cast<uint32_t>(U(200, 2000));
            ArcGraph g = (c & 1) ? ArcGraph::layered_net(static_cast<uint32_t>(U(3, 6)), n / 4, static_cast<uint32_t>(U(2, 8)), rng())
                                 : ArcGraph::random_dag(n, static_cast<uint32_t>(U(2, 6)), static_cast<uint32_t>(U(8, 256)), rng());
            static const uint8_t kBits[] = {1, 2, 3, 8, 16, 32};
            g.frac_bits = kBits[c % 6];
            g.act = (rng() & 1) ? Act::HardTanh : Act::ReluClamp;
            auto rand_in = [&]() -> int64_t {
                const uint64_t r = U(0, 99);
                if (r < 30) return (rng() & 1) ? kClampQ : -kClampQ;                       // saturating
                if (r < 33) return std::llround(u(rng) * 50.0 * 0x1p32);                   // inadmissible for int64
                return std::llround(u(rng) * 2.0 * 0x1p32);
            };
            auto rand_w = [&]() -> int32_t {
                const uint64_t r = U(0, 99);
                if (r < 3) return (rng() & 1) ? (int32_t{1} << 26) : -(int32_t{1} << 26);  // int64 boundary
                if (r < 15) return 0;                                                       // arc deletion
                return static_cast<int32_t>(std::lround(u(rng) * 1.5 * 0x1p16));
            };
            std::vector<int64_t> in_q(g.n); for (auto& x : in_q) x = std::llround(u(rng) * 0x1p32);
            std::vector<int32_t> w_q(g.arcs()); for (auto& x : w_q) x = static_cast<int32_t>(std::lround(u(rng) * 0.6 * 0x1p16));
            EraKernel k(g, ops + 8); k.load(in_q, w_q);
            struct Ck { size_t journal; std::vector<int64_t> vals, in; std::vector<int32_t> w; };
            std::vector<Ck> cks;  // bounded: at most 64 kept
            auto check = [&] { ++lr.checks; const auto ref = EraOracle::evaluate(g, k.inputs(), k.weights()); if (ref.empty() || ref != k.values()) ++lr.mismatches; };
            for (size_t s = 0; s < ops; ++s) {
                const uint64_t r = U(0, 99);
                if (r < 65) { if (U(0, 1)) k.set_input(static_cast<uint32_t>(U(0, g.n - 1)), rand_in()); else k.set_weight(static_cast<uint32_t>(U(0, g.arcs() - 1)), rand_w()); }
                else if (r < 85) {
                    if (k.undo_one()) ++lr.undos;
                    // a checkpoint whose journal prefix was undone is no longer reachable by undo
                    while (!cks.empty() && cks.back().journal > k.journal_size()) cks.pop_back();
                }
                else if (r < 95) { if (cks.size() >= 64) cks.erase(cks.begin()); k.propagate(); cks.push_back({k.journal_size(), k.values(), k.inputs(), k.weights()}); }
                else if (!cks.empty()) {  // rollback to a random live checkpoint
                    const size_t i = static_cast<size_t>(U(0, cks.size() - 1));
                    if (cks[i].journal <= k.journal_size()) {
                        while (k.journal_size() > cks[i].journal) { k.undo_one(); ++lr.undos; }
                        k.propagate(); ++lr.rollbacks;
                        if (k.values() != cks[i].vals || k.inputs() != cks[i].in || k.weights() != cks[i].w) ++lr.rollback_mismatch;
                        cks.resize(i + 1);
                    }
                }
                k.propagate();
                if ((s & 511) == 511) check();
            }
            check();
            for (int64_t v : k.values()) lr.saturated += (v == kClampQ || v == -kClampQ);
            lr.promotions += k.promotions();
            lr.ops += ops; ++lr.cases;
        }
        return lr;
    }

    static int run_cli(const std::string& root, bool quick) {
        namespace fs = std::filesystem;
        std::cout << "[ERA-GATE] G1/G5 scalability + competitive benchmark\n" << std::flush;
        const auto sc = scalability(quick);
        for (const auto& r : sc) { std::ostringstream s; EraAudit::bench_md(s, r.b); std::cout << s.str() << "  peak RSS=" << r.peak_rss / 1e6 << " MB, values changed/op=" << r.changed_per_op << "\n"; }
        std::cout << "[ERA-GATE] G2 break-even\n" << std::flush;
        std::vector<BreakEven> be;
        const uint32_t nbe = quick ? 40960 : 409600;
        for (auto [kind, bits] : {std::pair<char, uint8_t>{'A', 8}, {'B', 32}, {'A', 32}}) {
            be.push_back(break_even(kind, bits, nbe, quick));
            const auto& b = be.back();
            std::printf("  %s n=%u: f*(vs int full 1T)=%s  f*(vs level-parallel 4T)=%s\n", b.workload.c_str(), b.n,
                        b.fstar_full < 0 ? "none (ERA cheaper on whole grid)" : std::to_string(b.fstar_full).c_str(),
                        b.fstar_par < 0 ? "none (ERA cheaper on whole grid)" : std::to_string(b.fstar_par).c_str());
            for (size_t i = 0; i < b.f.size(); ++i) std::printf("    f=%-8g ERA=%10.1f µs  full=%10.1f µs  par4=%10.1f µs\n", b.f[i], b.era_us[i], b.full_us[i], b.par_us[i]);
        }
        std::cout << "[ERA-GATE] G3 distributed reproducibility (reorder, duplicates, partition, crash + recovery)\n" << std::flush;
        std::vector<ReplicationReport> rp;
        for (bool crash : {false, true}) {
            rp.push_back(EraReplication::run(make_wl('A', quick ? 2048 : 8192, 32, quick ? 300 : 1500), 5, 0x5EED, crash));
            rp.push_back(EraReplication::run(make_wl('B', quick ? 2048 : 8192, 32, quick ? 300 : 1500), 5, 0x5EEE, crash));
        }
        for (const auto& r : rp)
            std::printf("  %s crash=%zu: delivered=%zu dup=%zu lost_while_down=%zu resent=%zu | register mismatch=%zu exact mismatch=%zu oracle mismatch=%zu | fp64 control=%zu\n",
                        r.name.c_str(), r.crashes, r.delivered, r.duplicates_sent, r.lost_while_down, r.resent_on_recovery, r.register_state_mismatch,
                        r.exact_value_mismatch, r.oracle_mismatch, r.fp64_value_mismatch);
        std::cout << "[ERA-GATE] G4 long random sequences\n" << std::flush;
        const LongRun lr = long_run(quick ? 6 : 40, quick ? 3000 : 20000, 0x10A6);
        std::printf("  cases=%zu ops=%zu oracle checks=%zu mismatches=%zu | rollbacks=%zu mismatch=%zu | undos=%zu promotions=%zu saturated nodes=%zu\n",
                    lr.cases, lr.ops, lr.checks, lr.mismatches, lr.rollbacks, lr.rollback_mismatch, lr.undos, lr.promotions, lr.saturated);
        bool ok = lr.passed();
        for (const auto& r : rp) ok = ok && r.passed();
        for (const auto& r : sc) ok = ok && r.b.era_oracle_mismatch == 0;
        if (!quick) write(root, sc, be, rp, lr, ok);
        std::cout << "[ERA-GATE] correctness verdict: " << (ok ? "HOLD" : "VIOLATED") << " (performance reported, not gated)\n";
        return ok ? 0 : 1;
    }

    static void write(const std::string& root, const std::vector<ScaleRow>& sc, const std::vector<BreakEven>& be,
                      const std::vector<ReplicationReport>& rp, const LongRun& lr, bool ok) {
        namespace fs = std::filesystem;
        const fs::path disc = fs::path(root) / "singular/discoveries/registry/DISC-000901";
        const fs::path raw = fs::path(root) / "singular/verification/reports/era_raw";
        std::error_code ec; fs::create_directories(raw, ec);
        std::ofstream csv(raw / "gate_latencies.csv");
        csv << "workload,mult,variant,sample_index,latency_us\n";
        for (const auto& r : sc)
            for (int v = 0; v < kNumVariants; ++v)
                if (r.b.enabled[v]) for (size_t i = 0; i < r.b.samples[v].size(); ++i) csv << r.workload << ',' << r.mult << ",\"" << variant_name(v) << "\"," << i << ',' << r.b.samples[v][i] << '\n';
        std::ofstream j(disc / "gate_evidence.json");
        j.precision(10);
        j << "{\n  \"id\": \"DISC-000901\", \"gate\": \"owner acceptance gate 2026-11-10\", \"correctness\": \"" << (ok ? "HOLD" : "VIOLATED") << "\",\n"
          << "  \"provenance\": \"singular/verification/reports/era_raw/provenance_era-gate.txt\", \"raw_samples\": \"singular/verification/reports/era_raw/gate_latencies.csv\",\n"
          << "  \"energy\": \"not measured (no privileged power counters); cpu_us_per_op is the proxy\",\n"
          << "  \"op_message_bytes\": 21, \"state_delta_bytes_per_changed_value\": 12,\n  \"scalability\": [";
        for (size_t i = 0; i < sc.size(); ++i) {
            j << (i ? ",\n    " : "\n    ") << "{\"workload\": \"" << sc[i].workload << "\", \"mult\": " << sc[i].mult << ", \"peak_rss_bytes\": " << sc[i].peak_rss
              << ", \"values_changed_per_op\": " << sc[i].changed_per_op << ", \"bench\": ";
            EraAudit::bench_json(j, sc[i].b);
            j << "}";
        }
        j << "\n  ],\n  \"break_even\": [";
        for (size_t i = 0; i < be.size(); ++i) {
            const auto& b = be[i];
            j << (i ? ", " : "") << "{\"workload\": \"" << b.workload << "\", \"n\": " << b.n << ", \"fstar_vs_int_full_1T\": " << b.fstar_full
              << ", \"fstar_vs_level_parallel_4T\": " << b.fstar_par << ", \"grid\": [";
            for (size_t q = 0; q < b.f.size(); ++q)
                j << (q ? ", " : "") << "{\"f\": " << b.f[q] << ", \"era_us\": " << b.era_us[q] << ", \"int_full_us\": " << b.full_us[q] << ", \"par4_us\": " << b.par_us[q] << "}";
            j << "]}";
        }
        j << "],\n  \"replication\": [";
        for (size_t i = 0; i < rp.size(); ++i) {
            const auto& r = rp[i];
            j << (i ? ", " : "") << "{\"name\": \"" << r.name << "\", \"crashes\": " << r.crashes << ", \"delivered\": " << r.delivered << ", \"duplicates\": " << r.duplicates_sent
              << ", \"lost_while_down\": " << r.lost_while_down << ", \"resent_on_recovery\": " << r.resent_on_recovery << ", \"register_mismatch\": " << r.register_state_mismatch
              << ", \"exact_value_mismatch\": " << r.exact_value_mismatch << ", \"oracle_mismatch\": " << r.oracle_mismatch << ", \"fp64_control_mismatch\": " << r.fp64_value_mismatch << "}";
        }
        j << "],\n  \"long_run\": {\"cases\": " << lr.cases << ", \"ops\": " << lr.ops << ", \"checks\": " << lr.checks << ", \"mismatches\": " << lr.mismatches
          << ", \"rollbacks\": " << lr.rollbacks << ", \"rollback_mismatch\": " << lr.rollback_mismatch << ", \"undos\": " << lr.undos << ", \"promotions\": " << lr.promotions
          << ", \"saturated_nodes\": " << lr.saturated << "}\n}\n";
        std::ofstream md(fs::path(root) / "singular/verification/reports/ERA_GATE_EVIDENCE.md");
        md << "# ERA acceptance gate evidence\n\nCorrectness: **" << (ok ? "HOLD" : "VIOLATED") << "**. Raw: `DISC-000901/gate_evidence.json`, `era_raw/gate_latencies.csv`, `era_raw/provenance_era-gate.txt`.\n";
        for (const auto& r : sc) EraAudit::bench_md(md, r.b);
        std::cout << "[ERA-GATE] wrote " << (disc / "gate_evidence.json").string() << "\n";
    }
};

} // namespace cycraft::omega::quantum_syntax::era
