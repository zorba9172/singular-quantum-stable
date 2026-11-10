// ============================================================================
// SINGULAR Ω — ERA audit driver (`singular/verification/era/era_audit.sh era-audit`, DISC-000901)
// Rule `SG-ARCH-400`: <= 400 physical lines.
//   1. differential fuzzing: EraKernel vs the independent EraOracle after every op,
//      incl. rounding ties, int64 admission boundaries, forced promotions, undo-all;
//   2. overflow boundary: a node driven to the exact int64 admission limit;
//   3. replication: LWW-CRDT replicas under reorder/duplication/partition (I3);
//   4. clean interleaved benchmark with median/p95/bootstrap CI (5 variants);
//   5. real weights (safetensors) workload: speed and quality vs fp64.
// Every number written is measured in this process.
// ============================================================================
#pragma once
#include "cycraft/quantum_syntax/omega_era_audit_bench.hpp"
#include "cycraft/quantum_syntax/omega_era_replication.hpp"
#include "cycraft/quantum_syntax/omega_era_skinning_bench.hpp"

namespace cycraft::omega::quantum_syntax::era {

struct FuzzReport {
    size_t cases = 0, checks = 0, mismatches = 0, undo_cases = 0, undo_mismatches = 0;
    size_t promotions = 0, started_wide = 0, rejected_after_promotion = 0, tie_cases = 0;
    bool passed() const { return checks > 0 && mismatches == 0 && undo_mismatches == 0 && rejected_after_promotion == 0; }
};
struct BoundaryReport {
    uint32_t indegree = 0; bool admitted_at_limit = false, oracle_equal_at_limit = false;
    bool rejected_beyond_limit = false, promoted = false, oracle_equal_after_promotion = false;
    double max_abs_acc_log2 = 0;
    bool passed() const { return admitted_at_limit && oracle_equal_at_limit && rejected_beyond_limit && promoted && oracle_equal_after_promotion && max_abs_acc_log2 <= 62.0; }
};
struct RealReport {
    bool ok = false; std::string path, error, note;
    uint32_t nodes = 0; size_t arcs = 0; double max_abs_weight = 0, max_abs_embedding = 0;
    double max_abs_err_q32 = 0, rel_l2_q32 = 0, max_abs_err_q16 = 0, rel_l2_q16 = 0;
    std::vector<CleanBenchResult> bench;
};

class EraAudit {
public:
    static FuzzReport fuzz(size_t cases, uint64_t seed) {
        FuzzReport f;
        std::mt19937_64 rng(seed);
        auto U = [&](uint64_t lo, uint64_t hi) { return lo + rng() % (hi - lo + 1); };
        std::uniform_real_distribution<double> u(-1.0, 1.0);
        for (size_t c = 0; c < cases; ++c) {
            const int mode = static_cast<int>(c % 4);  // 0 normal, 1 rounding ties, 2 weight boundary, 3 input boundary
            const uint32_t n = static_cast<uint32_t>(U(8, 300));
            ArcGraph g = (c % 3 == 2) ? ArcGraph::layered_net(static_cast<uint32_t>(U(2, 5)), std::max<uint32_t>(2, n / 4), static_cast<uint32_t>(U(1, 6)), rng())
                                      : ArcGraph::random_dag(n, static_cast<uint32_t>(U(1, 6)), static_cast<uint32_t>(U(1, 64)), rng());
            static const uint8_t kBits[] = {32, 24, 16, 8, 4, 1};
            g.frac_bits = kBits[U(0, 5)];
            g.act = (rng() & 1) ? Act::HardTanh : Act::ReluClamp;
            auto rand_w = [&]() -> int32_t {
                if (mode == 1) { static const int32_t t[] = {32768, -32768, 16384, -16384, 65536, -98304}; return t[U(0, 5)]; }
                if (mode == 2 && U(0, 9) == 0) return (rng() & 1) ? (int32_t{1} << 26) : -(int32_t{1} << 26);
                return static_cast<int32_t>(std::lround(u(rng) * 0.6 * 0x1p16));
            };
            auto rand_in = [&]() -> int64_t {
                if (mode == 3 && U(0, 19) == 0) return std::llround(u(rng) * 900.0 * 0x1p32);  // beyond int64 admission
                if (mode == 1) return static_cast<int64_t>(U(0, 4096)) * 2 + 1;               // odd Q32 units -> ties
                return std::llround(u(rng) * 0x1p32);
            };
            f.tie_cases += (mode == 1);
            std::vector<int64_t> in_q(g.n); for (auto& x : in_q) x = rand_in();
            std::vector<int32_t> w_q(g.arcs()); for (auto& x : w_q) x = (U(0, 6) == 0) ? 0 : rand_w();
            const size_t steps = 160;
            EraKernel k(g, steps + 8);
            k.load(in_q, w_q);
            f.started_wide += k.wide();
            const auto initial = k.values();
            auto check = [&]() {
                const auto ref = EraOracle::evaluate(g, k.inputs(), k.weights());
                ++f.checks;
                if (ref.empty() || ref != k.values()) ++f.mismatches;
            };
            check();
            size_t journaled = 0;
            for (size_t s = 0; s < steps; ++s) {
                if (journaled > 0 && U(0, 3) == 0) { if (k.undo_one()) --journaled; }
                else {
                    const bool ok = (U(0, 1) == 0) ? k.set_input(static_cast<uint32_t>(U(0, g.n - 1)), rand_in())
                                                   : k.set_weight(static_cast<uint32_t>(U(0, g.arcs() - 1)), (U(0, 4) == 0) ? 0 : rand_w());
                    if (ok) ++journaled; else if (k.wide()) ++f.rejected_after_promotion;
                }
                k.propagate();
                check();
            }
            while (k.undo_one()) k.propagate();
            ++f.undo_cases;
            if (k.values() != initial || k.inputs() != in_q || k.weights() != w_q) ++f.undo_mismatches;
            f.promotions += k.promotions();
            ++f.cases;
        }
        return f;
    }

    static BoundaryReport boundary() {
        BoundaryReport b;
        const uint32_t d = 2047;  // one sink with 2047 sources
        ArcGraph g; g.n = d + 1; g.act = Act::HardTanh;
        for (uint32_t i = 0; i < d; ++i) { g.e_src.push_back(i); g.e_dst.push_back(d); }
        g.finalize();
        b.indegree = d;
        std::vector<int64_t> in_q(g.n, Exact64Traits::kMaxInputQ);  // every input at +8.0
        std::vector<int32_t> w_q(d, 0);
        int64_t budget = Exact64Traits::kMaxWeightSumQ;
        for (uint32_t i = 0; i < d && budget > 0; ++i) {          // Σ|w| exactly at the limit
            const int64_t w = std::min<int64_t>(budget, Exact64Traits::kMaxWeightQ);
            w_q[i] = static_cast<int32_t>(w); budget -= w;
        }
        ArcEngine<Exact64Traits> e64(g, 4); e64.load(in_q, w_q);
        b.admitted_at_limit = e64.admitted() && budget == 0;
        b.oracle_equal_at_limit = e64.values() == EraOracle::evaluate(g, in_q, w_q);
        for (int64_t a : e64.accumulators()) b.max_abs_acc_log2 = std::max(b.max_abs_acc_log2, a == 0 ? 0.0 : std::log2(std::abs(static_cast<double>(a))));
        uint32_t spare = 0; while (spare < d && w_q[spare] == Exact64Traits::kMaxWeightQ) ++spare;
        b.rejected_beyond_limit = !e64.set_weight(spare, w_q[spare] + 1);
        EraKernel k(g, 4); k.load(in_q, w_q);
        k.set_weight(spare, w_q[spare] + 1); k.propagate();
        b.promoted = k.wide() && k.promotions() == 1;
        b.oracle_equal_after_promotion = k.values() == EraOracle::evaluate(g, k.inputs(), k.weights());
        return b;
    }

    static std::vector<EraCleanBench::Batch> single_batches(const Workload& w) {
        std::vector<EraCleanBench::Batch> b; b.reserve(w.ops.size());
        for (const auto& op : w.ops) b.push_back({op});
        return b;
    }

    static RealReport real(const std::string& path, bool quick) {
        RealReport r; r.path = path;
        r.note = "simplified network: up/down MLP projections of layers 0..3 chained with hard-tanh; no gating, norms, attention or residuals; weight provenance (trained or not) not verified";
        RealModel m32 = RealModelLoader::load(path, 4, 32, 8);
        if (!m32.ok) { r.error = m32.error; return r; }
        r.ok = true; r.nodes = m32.g.n; r.arcs = m32.g.arcs(); r.max_abs_weight = m32.max_abs_weight; r.max_abs_embedding = m32.max_abs_embedding;
        auto quality = [&](const RealModel& m, double& max_abs, double& rel_l2) {
            double num = 0, den = 0;
            for (const auto& x : m.emb) {
                std::vector<int64_t> in_q(m.g.n, 0);
                for (uint32_t i = 0; i < m.in_dim; ++i) in_q[i] = RealModelLoader::q32(x[i]);
                EraKernel k(m.g, 1); k.load(in_q, m.w_q);
                const auto ref = RealModelLoader::reference(m, x);
                for (uint32_t o = 0; o < m.out_dim; ++o) {
                    const double a = static_cast<double>(k.values()[m.out_base + o]) * 0x1p-32, b = ref[m.out_base + o];
                    max_abs = std::max(max_abs, std::abs(a - b)); num += (a - b) * (a - b); den += b * b;
                }
            }
            rel_l2 = den > 0 ? std::sqrt(num / den) : 0;
        };
        RealModel m16 = RealModelLoader::load(path, 4, 16, 8);
        quality(m32, r.max_abs_err_q32, r.rel_l2_q32);
        quality(m16, r.max_abs_err_q16, r.rel_l2_q16);
        for (RealModel* m : {&m32, &m16}) {
            std::vector<int64_t> in_q(m->g.n, 0);
            for (uint32_t i = 0; i < m->in_dim; ++i) in_q[i] = RealModelLoader::q32(m->emb[0][i]);
            std::mt19937_64 rng(0x7E41);
            std::vector<EraCleanBench::Batch> single, swap;
            for (int i = 0; i < (quick ? 20 : 60); ++i) {  // one embedding feature edited
                const uint32_t f = static_cast<uint32_t>(rng() % m->in_dim);
                single.push_back({Op{0, f, RealModelLoader::q32(m->emb[1 + rng() % (m->emb.size() - 1)][f])}});
            }
            for (int i = 0; i < (quick ? 6 : 16); ++i) {   // whole token swapped (in_dim input ops)
                EraCleanBench::Batch b; const auto& x = m->emb[i % m->emb.size()];
                for (uint32_t f = 0; f < m->in_dim; ++f) b.push_back(Op{0, f, RealModelLoader::q32(x[f])});
                swap.push_back(std::move(b));
            }
            const std::string q = "-q" + std::to_string(m->g.frac_bits);
            const std::array<bool, kNumVariants> all{true, true, true, true, true};
            r.bench.push_back(EraCleanBench::run("C-real-weights" + q + "-feature-edit", m->g, in_q, m->w_q, single, quick ? 3 : 7, all));
            r.bench.push_back(EraCleanBench::run("C-real-weights" + q + "-token-swap", m->g, in_q, m->w_q, swap, quick ? 3 : 7, all));
        }
        return r;
    }

    static void bench_json(std::ostream& o, const CleanBenchResult& b) {
        o << "{\"name\": \"" << b.name << "\", \"nodes\": " << b.nodes << ", \"arcs\": " << b.arcs << ", \"ops\": " << b.ops
          << ", \"trials\": " << b.trials << ", \"load1_before\": " << b.load_before << ", \"load1_after\": " << b.load_after
          << ", \"era_oracle_mismatch\": " << b.era_oracle_mismatch << ", \"era_state_bytes\": " << b.era_state_bytes << ", \"fp64_state_bytes\": " << b.fp64_state_bytes << ", \"era_int64\": " << (b.era_used_int64 ? "true" : "false") << ", \"variants\": [";
        bool first = true;
        for (int v = 0; v < kNumVariants; ++v) {
            if (!b.enabled[v]) continue;
            o << (first ? "" : ", ") << "{\"variant\": \"" << variant_name(v) << "\", \"median_us\": " << b.per_op[v].median << ", \"p95_us\": " << b.per_op[v].p95 << ", \"cpu_us_per_op\": " << b.cpu_us_per_op[v]
              << ", \"ratio_vs_era_median\": " << b.ratio_vs_era[v].median << ", \"ratio_ci95\": [" << b.ratio_vs_era[v].ci_lo << ", " << b.ratio_vs_era[v].ci_hi << "]}";
            first = false;
        }
        o << "]}";
    }
    static void bench_md(std::ostream& o, const CleanBenchResult& b) {
        char buf[512];
        std::snprintf(buf, sizeof buf, "\n### %s (n=%u, arcs=%zu, ops=%zu, trials=%d, load1 %.1f→%.1f, ERA oracle mismatches=%zu, memory ERA=%.2f MB fp64=%.2f MB)\n\n",
                      b.name.c_str(), b.nodes, b.arcs, b.ops, b.trials, b.load_before, b.load_after, b.era_oracle_mismatch, b.era_state_bytes / 1e6, b.fp64_state_bytes / 1e6);
        o << buf << "| variant | median µs/op | p95 µs/op | throughput ops/s (1/median) | CPU µs/op | cost ÷ ERA (median) | 95% CI |\n|---|---:|---:|---:|---:|---:|---|\n";
        for (int v = 0; v < kNumVariants; ++v) {
            if (!b.enabled[v]) continue;
            std::snprintf(buf, sizeof buf, "| %s | %.2f | %.2f | %.0f | %.1f | %.2f× | [%.2f, %.2f] |\n", variant_name(v), b.per_op[v].median, b.per_op[v].p95,
                          b.per_op[v].median > 0 ? 1e6 / b.per_op[v].median : 0.0, b.cpu_us_per_op[v], b.ratio_vs_era[v].median, b.ratio_vs_era[v].ci_lo, b.ratio_vs_era[v].ci_hi);
            o << buf;
        }
    }

    static int run_cli(const std::string& root, const std::string& cmd) {
        if (cmd != "era-audit" && cmd != "era-audit-quick") return EraBenchmark::run_cli(root, cmd == "era-quick");
        const bool quick = cmd == "era-audit-quick";
        std::cout << "[ERA-AUDIT] 1/6 differential fuzzing vs independent oracle\n" << std::flush;
        const FuzzReport fz = fuzz(quick ? 120 : 600, 0xF022);
        std::printf("  cases=%zu checks=%zu mismatches=%zu | undo-all cases=%zu mismatches=%zu | promotions=%zu started_i128=%zu tie_cases=%zu\n",
                    fz.cases, fz.checks, fz.mismatches, fz.undo_cases, fz.undo_mismatches, fz.promotions, fz.started_wide, fz.tie_cases);
        std::cout << "[ERA-AUDIT] 2/6 int64 admission boundary\n" << std::flush;
        const BoundaryReport bd = boundary();
        std::printf("  indeg=%u admitted@limit=%d oracle@limit=%d max|acc|=2^%.3f rejected beyond=%d promoted=%d oracle after=%d\n",
                    bd.indegree, bd.admitted_at_limit, bd.oracle_equal_at_limit, bd.max_abs_acc_log2, bd.rejected_beyond_limit, bd.promoted, bd.oracle_equal_after_promotion);
        std::cout << "[ERA-AUDIT] 3/6 replication (LWW-CRDT, reorder, duplicates, partition)\n" << std::flush;
        std::vector<ReplicationReport> rp;
        rp.push_back(EraReplication::run(EraBenchmark::make("A-dataflow-DAG", "", ArcGraph::random_dag(quick ? 2048 : 8192, 4, 512, 0xA11CE), 0.6, quick ? 300 : 1200, 0xA1), 5, 0x5EED));
        rp.push_back(EraReplication::run(EraBenchmark::make("B-sparse-ReLU-net", "", ArcGraph::layered_net(6, quick ? 256 : 1024, 12, 0xB0B), 0.35, quick ? 300 : 1200, 0xB1), 5, 0x5EEE));
        for (const auto& r : rp)
            std::printf("  %s: replicas=%zu ops=%zu delivered=%zu dup_sent=%zu dropped=%zu parked=%zu | register mismatch=%zu exact value mismatch=%zu oracle mismatch=%zu | fp64 control mismatch=%zu\n",
                        r.name.c_str(), r.replicas, r.ops_generated, r.delivered, r.duplicates_sent, r.stale_or_dup_dropped, r.parked_during_partition,
                        r.register_state_mismatch, r.exact_value_mismatch, r.oracle_mismatch, r.fp64_value_mismatch);
        std::cout << "[ERA-AUDIT] 4/6 clean interleaved benchmark (synthetic)\n" << std::flush;
        std::vector<CleanBenchResult> cb;
        const std::array<bool, kNumVariants> all{true, true, true, true, true};
        const size_t m = quick ? 100 : 300; const int trials = quick ? 3 : 9;
        const Workload a = EraBenchmark::make("A-dataflow-DAG-q32", "", ArcGraph::random_dag(32768, 4, 2048, 0xA11CE), 0.6, m, 0xA1);
        const Workload b = EraBenchmark::make("B-sparse-ReLU-net-q32", "", ArcGraph::layered_net(8, 2048, 12, 0xB0B), 0.35, m, 0xB1);
        const Workload a8 = EraBenchmark::with_grid(a, 8), b8 = EraBenchmark::with_grid(b, 8);
        for (const Workload* w : {&a, &a8, &b, &b8}) cb.push_back(EraCleanBench::run(w->name, w->g, w->in_q, w->w_q, single_batches(*w), trials, all));
        for (const auto& r : cb) { std::ostringstream s; bench_md(s, r); std::cout << s.str(); }
        std::cout << "[ERA-AUDIT] 5/6 real weights workload (dense)\n" << std::flush;
        const RealReport rr = real("/Volumes/Lenovo PS8S/workspace/model/models/experimental/micro_singular_8layer_bf16.safetensors", quick);
        if (!rr.ok) std::printf("  real workload unavailable: %s\n", rr.error.c_str());
        else {
            std::printf("  n=%u arcs=%zu max|w|=%.4f max|emb|=%.4f | quality vs fp64 (orig weights): q32 max=%.3e relL2=%.3e | q16 max=%.3e relL2=%.3e\n",
                        rr.nodes, rr.arcs, rr.max_abs_weight, rr.max_abs_embedding, rr.max_abs_err_q32, rr.rel_l2_q32, rr.max_abs_err_q16, rr.rel_l2_q16);
            for (const auto& r : rr.bench) { std::ostringstream s; bench_md(s, r); std::cout << s.str(); }
        }
        std::cout << "[ERA-AUDIT] 6/6 real sparse workload: skinning of real rigged glTF assets (pose edits)\n" << std::flush;
        std::vector<SkinReport> sk;
        for (const char* p : {"/Volumes/Lenovo PS8S/workspace/Simulator/plugins/assets/models/Fox.glb",
                              "/Volumes/Lenovo PS8S/workspace/Simulator/plugins/assets/models/CesiumMan.glb"}) {
            sk.push_back(EraSkinning::run(p, quick ? 60 : 300, quick ? 3 : 9, 0x5C1A));
            const auto& s = sk.back();
            if (!s.ok) { std::printf("  %s unavailable: %s\n", p, s.error.c_str()); continue; }
            std::printf("  %s: joints=%zu vertices=%zu nodes=%u arcs=%zu | changed inputs/edit=%.1f touched=%.1f%% of graph | max err vs fp64 LBS=%.3e asset units (extent %.3g) | undo mismatch=%zu oracle mismatch=%zu out-of-range=%zu\n",
                        std::filesystem::path(p).filename().string().c_str(), s.joints, s.vertices, s.nodes, s.arcs, s.mean_changed_inputs,
                        100.0 * s.mean_touched_fraction, s.max_err_asset, s.asset_extent, s.undo_mismatch, s.oracle_mismatch, s.out_of_range_inputs);
            std::ostringstream o; bench_md(o, s.bench); std::cout << o.str();
        }
        bool ok = fz.passed() && bd.passed();
        for (const auto& r : rp) ok = ok && r.passed();
        for (const auto& r : cb) ok = ok && r.era_oracle_mismatch == 0;
        for (const auto& r : rr.bench) ok = ok && r.era_oracle_mismatch == 0;
        for (const auto& s : sk) ok = ok && (!s.ok || (s.undo_mismatch == 0 && s.bench.era_oracle_mismatch == 0));
        if (!quick) write_evidence(root, fz, bd, rp, cb, rr, sk, ok);
        std::cout << "[ERA-AUDIT] correctness verdict: " << (ok ? "HOLD" : "VIOLATED") << " (speed is reported, not gated)\n";
        return ok ? 0 : 1;
    }

    static void write_evidence(const std::string& root, const FuzzReport& fz, const BoundaryReport& bd, const std::vector<ReplicationReport>& rp,
                               const std::vector<CleanBenchResult>& cb, const RealReport& rr, const std::vector<SkinReport>& sk, bool ok) {
        namespace fs = std::filesystem;
        const fs::path disc = fs::path(root) / "singular/discoveries/registry/DISC-000901";
        const fs::path rep = fs::path(root) / "singular/verification/reports";
        std::error_code ec; fs::create_directories(disc, ec); fs::create_directories(rep, ec);
        std::ofstream j(disc / "audit_evidence.json");
        j.precision(10);
        j << "{\n  \"id\": \"DISC-000901\", \"status\": \"RESEARCH_CANDIDATE\", \"correctness\": \"" << (ok ? "HOLD" : "VIOLATED") << "\",\n"
          << "  \"host\": \"" << EraBenchmark::host_cpu() << " / " << __VERSION__ << "\", \"source\": \"singular/verification/era/era_audit.sh era-audit\",\n"
          << "  \"fuzz\": {\"cases\": " << fz.cases << ", \"checks\": " << fz.checks << ", \"mismatches\": " << fz.mismatches << ", \"undo_cases\": " << fz.undo_cases
          << ", \"undo_mismatches\": " << fz.undo_mismatches << ", \"promotions\": " << fz.promotions << ", \"started_i128\": " << fz.started_wide
          << ", \"tie_cases\": " << fz.tie_cases << ", \"rejected_after_promotion\": " << fz.rejected_after_promotion << "},\n"
          << "  \"boundary\": {\"indegree\": " << bd.indegree << ", \"admitted_at_limit\": " << bd.admitted_at_limit << ", \"oracle_equal_at_limit\": " << bd.oracle_equal_at_limit
          << ", \"max_abs_acc_log2\": " << bd.max_abs_acc_log2 << ", \"rejected_beyond_limit\": " << bd.rejected_beyond_limit << ", \"promoted\": " << bd.promoted
          << ", \"oracle_equal_after_promotion\": " << bd.oracle_equal_after_promotion << "},\n  \"replication\": [";
        for (size_t i = 0; i < rp.size(); ++i) {
            const auto& r = rp[i];
            j << (i ? ", " : "") << "{\"name\": \"" << r.name << "\", \"replicas\": " << r.replicas << ", \"ops\": " << r.ops_generated << ", \"delivered\": " << r.delivered
              << ", \"duplicates_sent\": " << r.duplicates_sent << ", \"stale_or_dup_dropped\": " << r.stale_or_dup_dropped << ", \"parked\": " << r.parked_during_partition
              << ", \"register_mismatch\": " << r.register_state_mismatch << ", \"exact_value_mismatch\": " << r.exact_value_mismatch
              << ", \"oracle_mismatch\": " << r.oracle_mismatch << ", \"fp64_control_value_mismatch\": " << r.fp64_value_mismatch << "}";
        }
        j << "],\n  \"clean_bench\": [";
        for (size_t i = 0; i < cb.size(); ++i) { j << (i ? ", " : ""); bench_json(j, cb[i]); }
        j << "],\n  \"real_weights\": {\"ok\": " << (rr.ok ? "true" : "false") << ", \"path\": \"" << rr.path << "\", \"error\": \"" << rr.error << "\", \"note\": \"" << rr.note
          << "\", \"nodes\": " << rr.nodes << ", \"arcs\": " << rr.arcs << ", \"max_abs_weight\": " << rr.max_abs_weight << ", \"max_abs_embedding\": " << rr.max_abs_embedding
          << ", \"q32_max_abs_err\": " << rr.max_abs_err_q32 << ", \"q32_rel_l2\": " << rr.rel_l2_q32 << ", \"q16_max_abs_err\": " << rr.max_abs_err_q16
          << ", \"q16_rel_l2\": " << rr.rel_l2_q16 << ", \"bench\": [";
        for (size_t i = 0; i < rr.bench.size(); ++i) { j << (i ? ", " : ""); bench_json(j, rr.bench[i]); }
        j << "]},\n  \"skinning\": [";
        for (size_t i = 0; i < sk.size(); ++i) {
            const auto& s = sk[i];
            j << (i ? ", " : "") << "{\"path\": \"" << s.path << "\", \"ok\": " << (s.ok ? "true" : "false") << ", \"error\": \"" << s.error
              << "\", \"joints\": " << s.joints << ", \"vertices\": " << s.vertices << ", \"pose_edits\": " << s.pose_edits << ", \"scale\": " << s.scale
              << ", \"mean_changed_inputs\": " << s.mean_changed_inputs << ", \"mean_touched_fraction\": " << s.mean_touched_fraction
              << ", \"max_err_asset_units\": " << s.max_err_asset << ", \"asset_extent\": " << s.asset_extent << ", \"undo_mismatch\": " << s.undo_mismatch
              << ", \"oracle_mismatch\": " << s.oracle_mismatch << ", \"out_of_range_inputs\": " << s.out_of_range_inputs << ", \"bench\": ";
            if (s.ok) bench_json(j, s.bench); else j << "null";
            j << "}";
        }
        j << "]\n}\n";
        std::ofstream md(rep / "ERA_AUDIT_EVIDENCE.md");
        md << "# ERA audit evidence (`singular/verification/era/era_audit.sh era-audit`)\n\nCorrectness: **" << (ok ? "HOLD" : "VIOLATED") << "**. Raw data: `DISC-000901/audit_evidence.json`.\n"
           << "Cost ÷ ERA > 1 means ERA is faster than that variant; CI is a percentile bootstrap over trials.\n";
        for (const auto& r : cb) bench_md(md, r);
        for (const auto& r : rr.bench) bench_md(md, r);
        for (const auto& s : sk) if (s.ok) bench_md(md, s.bench);
        std::cout << "[ERA-AUDIT] wrote " << (disc / "audit_evidence.json").string() << "\n";
    }
};

} // namespace cycraft::omega::quantum_syntax::era
