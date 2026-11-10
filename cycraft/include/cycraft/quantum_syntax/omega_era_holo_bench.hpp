// ============================================================================
// SINGULAR Ω — ERA-CGH experiment (`DISC-000902`): animated rigged character →
// exact skinning (ERA kernel) → exact incremental hologram (ExactHologram).
// Measures per pose edit, against 3 alternatives, with median/p95/bootstrap CI:
// speed, bit-identity (incremental vs full, undo-all, node-partition order) and
// quality vs standard fp64 CGH; writes numerical reconstructions as BMP.
// Rule `SG-ARCH-400`: <= 400 physical lines.
// ============================================================================
#pragma once
#include "cycraft/quantum_syntax/omega_era_holography.hpp"
#include "cycraft/quantum_syntax/omega_era_skinning_bench.hpp"

namespace cycraft::omega::quantum_syntax::era {

struct HoloReport {
    bool ok = false; std::string path, error, images;
    size_t points = 0, edits = 0; int W = 0, H = 0, trials = 0;
    double mean_moved_points = 0, load_before = 0, load_after = 0;
    std::array<Dist, 4> per_edit{}; std::array<Ratio, 4> ratio_vs_era{};
    size_t era_incr_vs_full_mismatch = 0, era_undo_mismatch = 0, era_partition_order_mismatch = 0;
    size_t fp64_incr_vs_full_mismatch = 0, fp64_undo_mismatch = 0, fp64_partition_order_mismatch = 0;
    double fp64_incr_vs_full_max_abs = 0, fp64_undo_max_abs = 0;
    double rel_l2_cgh_quantization = 0, rel_l2_end_to_end = 0;
    size_t era_bytes = 0, fp64_bytes = 0;
    bool exact_ok() const { return ok && era_incr_vs_full_mismatch == 0 && era_undo_mismatch == 0 && era_partition_order_mismatch == 0; }
};

class EraHolo {
public:
    enum { kStdFull = 0, kIntFull, kFp64Incr, kEraIncr };
    static const char* name(int v) {
        static const char* n[] = {"standard: fp64 skinning + fp64 full CGH (sin/cos)", "same semantics: integer full skinning + exact full CGH",
                                  "fp64 skinning + fp64 incremental CGH (drifts)", "ERA: exact incremental skinning + exact incremental CGH"};
        return n[v];
    }
    // Normalized skinned coordinate u (|u| <= 4) -> object space: ±0.3 mm lateral, ±0.15 mm depth around z0.
    static PointQ place(double ux, double uy, double uz, const HoloSetup& s) {
        return {to_pm(ux * 0.3e-3 / 4.0), to_pm(uy * 0.3e-3 / 4.0), to_pm(s.z0 + uz * 0.15e-3 / 4.0)};
    }
    static std::vector<PointQ> era_points(const std::vector<int64_t>& vals, uint32_t base, size_t nv, const HoloSetup& s) {
        std::vector<PointQ> p(nv);
        for (size_t v = 0; v < nv; ++v) {
            const auto c = [&](int r) { return static_cast<double>(vals[base + 3 * v + static_cast<size_t>(r)]) * 0x1p-32; };
            p[v] = place(c(0), c(1), c(2), s);
        }
        return p;
    }
    static std::vector<PointQ> fp64_points(const Rig& rig, const std::vector<M4>& S, const HoloSetup& s) {
        std::vector<PointQ> p(rig.rest.size());
        for (size_t v = 0; v < p.size(); ++v) { const auto q = RigLoader::lbs(rig, S, v); p[v] = place(q[0] * rig.scale, q[1] * rig.scale, q[2] * rig.scale, s); }
        return p;
    }
    static bool same(const PointQ& a, const PointQ& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }

    static HoloReport run(const std::string& path, const std::string& image_dir, size_t edits, int trials, uint64_t seed) {
        HoloReport r; r.path = path; r.trials = trials;
        Rig rig = RigLoader::load_glb(path);
        if (!rig.ok) { r.error = rig.error; return r; }
        HoloSetup s; r.W = s.W; r.H = s.H;
        std::vector<int32_t> w_q;
        const ArcGraph g = EraSkinning::build(rig, w_q);
        size_t oor = 0;
        const std::vector<int64_t> in0 = EraSkinning::inputs(rig, RigLoader::skin_matrices(rig), g.n, oor);
        Rig posed = rig;
        const auto ps = EraSkinning::pose_stream(rig, g.n, in0, edits, seed, posed, oor);
        if (oor) { r.error = "skinning inputs out of ±8 range"; return r; }
        const uint32_t base = 12 * static_cast<uint32_t>(rig.joints.size());
        const size_t nv = rig.rest.size();
        r.ok = true; r.points = nv; r.edits = ps.batches.size();
        const std::vector<M4> S0 = RigLoader::skin_matrices(rig);
        std::array<std::vector<double>, 4> pooled, trial_mean;
        double moved_total = 0;
        r.load_before = host_load1();
        for (int t = 0; t < trials; ++t)
            for (int k = 0; k < 4; ++k) {
                const int v = (k + t) % 4;
                std::vector<double> lat;
                auto timed = [&](auto&& body) { for (size_t e = 0; e < ps.batches.size(); ++e) { auto t0 = EraCleanBench::Clock::now(); body(e); lat.push_back(EraCleanBench::now_us(t0)); } };
                if (v == kStdFull) {
                    Fp64Hologram h(s);
                    timed([&](size_t e) { h.full(fp64_points(rig, ps.skin[e], s)); });
                    r.fp64_bytes = h.bytes();
                } else if (v == kIntFull) {
                    EraKernel k2(g, 1); k2.load(in0, w_q); ExactHologram h(s);
                    timed([&](size_t e) { EraCleanBench::apply_batch(k2, ps.batches[e]); k2.full_recompute(); h.full(era_points(k2.values(), base, nv, s)); });
                } else if (v == kFp64Incr) {
                    Fp64Hologram h(s); auto pts = fp64_points(rig, S0, s); h.full(pts);
                    timed([&](size_t e) {
                        const auto np = fp64_points(rig, ps.skin[e], s);
                        for (size_t q = 0; q < nv; ++q) if (!same(np[q], pts[q])) { h.apply(pts[q], -1.0); h.apply(np[q], 1.0); pts[q] = np[q]; }
                    });
                } else {
                    EraKernel k2(g, 1); k2.load(in0, w_q); ExactHologram h(s);
                    auto pts = era_points(k2.values(), base, nv, s); h.full(pts);
                    double moved = 0;
                    std::vector<PointQ> batch; std::vector<int> signs;
                    timed([&](size_t e) {
                        EraCleanBench::apply_batch(k2, ps.batches[e]); k2.propagate();
                        const auto np = era_points(k2.values(), base, nv, s);
                        batch.clear(); signs.clear();
                        for (size_t q = 0; q < nv; ++q)
                            if (!same(np[q], pts[q])) { batch.push_back(pts[q]); signs.push_back(-1); batch.push_back(np[q]); signs.push_back(+1); pts[q] = np[q]; ++moved; }
                        h.apply_points(batch.data(), signs.data(), batch.size(), 1);
                    });
                    moved_total = moved;
                    r.era_bytes = h.bytes() + k2.state_bytes();
                }
                double sum = 0; for (double x : lat) sum += x;
                trial_mean[v].push_back(sum / static_cast<double>(lat.size()));
                pooled[v].insert(pooled[v].end(), lat.begin(), lat.end());
            }
        r.load_after = host_load1();
        r.mean_moved_points = moved_total / static_cast<double>(ps.batches.size());
        for (int v = 0; v < 4; ++v) {
            r.per_edit[v] = {quantile(pooled[v], 0.5), quantile(pooled[v], 0.95)};
            std::vector<double> ratios;
            for (size_t t = 0; t < trial_mean[v].size(); ++t) ratios.push_back(trial_mean[v][t] / trial_mean[kEraIncr][t]);
            r.ratio_vs_era[v] = bootstrap_median(ratios, 0xC6 + static_cast<uint64_t>(v));
        }
        verify(r, rig, g, w_q, in0, ps, s, base, nv, image_dir);
        return r;
    }

    // Exactness, undo, partition order, quality and reconstructions (untimed).
    static void verify(HoloReport& r, const Rig& rig, const ArcGraph& g, const std::vector<int32_t>& w_q, const std::vector<int64_t>& in0,
                       const EraSkinning::PoseStream& ps, const HoloSetup& s, uint32_t base, size_t nv, const std::string& image_dir) {
        struct Move { size_t idx; PointQ from, to; };
        std::vector<Move> journal;  // bounded by edits * points
        EraKernel k(g, 1); k.load(in0, w_q);
        ExactHologram h(s), h0(s);
        auto pts = era_points(k.values(), base, nv, s);
        h.full(pts); h0 = h;
        Fp64Hologram f(s), f0(s);
        auto fpts = fp64_points(rig, RigLoader::skin_matrices(rig), s);
        f.full(fpts); f0 = f;
        std::vector<std::pair<size_t, std::pair<PointQ, PointQ>>> fjournal;
        for (size_t e = 0; e < ps.batches.size(); ++e) {
            EraCleanBench::apply_batch(k, ps.batches[e]); k.propagate();
            const auto np = era_points(k.values(), base, nv, s);
            for (size_t q = 0; q < nv; ++q) if (!same(np[q], pts[q])) { h.apply(pts[q], -1); h.apply(np[q], +1); journal.push_back({q, pts[q], np[q]}); pts[q] = np[q]; }
            const auto fp = fp64_points(rig, ps.skin[e], s);
            for (size_t q = 0; q < nv; ++q) if (!same(fp[q], fpts[q])) { f.apply(fpts[q], -1.0); f.apply(fp[q], 1.0); fjournal.push_back({q, {fpts[q], fp[q]}}); fpts[q] = fp[q]; }
        }
        ExactHologram hf(s); hf.full(pts);
        r.era_incr_vs_full_mismatch = h.mismatch(hf);
        Fp64Hologram ff(s); ff.full(fpts);
        r.fp64_incr_vs_full_mismatch = f.mismatch(ff); r.fp64_incr_vs_full_max_abs = f.max_abs_diff(ff);
        // Quality: (a) CGH quantization alone (same points), (b) end-to-end vs standard pipeline.
        Fp64Hologram fq(s); fq.full(pts);
        double num = 0, den = 0, num2 = 0;
        for (size_t q = 0; q < fq.data().size(); ++q) {
            const auto a = h.field(q);
            num += std::norm(a - fq.data()[q]); den += std::norm(fq.data()[q]); num2 += std::norm(a - ff.data()[q]);
        }
        r.rel_l2_cgh_quantization = std::sqrt(num / den); r.rel_l2_end_to_end = std::sqrt(num2 / den);
        // Partition order: 4 node-partitions summed forward vs reverse.
        auto partitioned = [&](auto make, bool reverse) {
            auto total = make();
            std::vector<decltype(make())> parts;
            for (int pi = 0; pi < 4; ++pi) { parts.push_back(make()); for (size_t q = static_cast<size_t>(pi); q < nv; q += 4) parts.back().apply(pts[q], 1); }
            for (int pi = 0; pi < 4; ++pi) total.add(parts[static_cast<size_t>(reverse ? 3 - pi : pi)]);
            return total;
        };
        const auto ea = partitioned([&] { return ExactHologram(s); }, false), eb = partitioned([&] { return ExactHologram(s); }, true);
        r.era_partition_order_mismatch = ea.mismatch(eb) + ea.mismatch(hf);
        const auto fa = partitioned([&] { return Fp64Hologram(s); }, false), fb = partitioned([&] { return Fp64Hologram(s); }, true);
        r.fp64_partition_order_mismatch = fa.mismatch(fb);
        // Undo all edits in reverse order.
        for (auto it = journal.rbegin(); it != journal.rend(); ++it) { h.apply(it->to, -1); h.apply(it->from, +1); }
        r.era_undo_mismatch = h.mismatch(h0);
        for (auto it = fjournal.rbegin(); it != fjournal.rend(); ++it) { f.apply(it->second.second, -1.0); f.apply(it->second.first, 1.0); }
        r.fp64_undo_mismatch = f.mismatch(f0); r.fp64_undo_max_abs = f.max_abs_diff(f0);
        if (!image_dir.empty()) {
            std::error_code ec; std::filesystem::create_directories(image_dir, ec);
            std::vector<std::complex<double>> fe(static_cast<size_t>(s.W) * s.H);
            for (size_t q = 0; q < fe.size(); ++q) fe[q] = hf.field(q);
            const std::string stem = std::filesystem::path(r.path).stem().string();
            HoloOptics::write_bmp(image_dir + "/" + stem + "_era_reconstruction.bmp", HoloOptics::reconstruct(fe, s, -s.z0), s.W, s.H);
            HoloOptics::write_bmp(image_dir + "/" + stem + "_fp64_reconstruction.bmp", HoloOptics::reconstruct(ff.data(), s, -s.z0), s.W, s.H);
            std::vector<double> phase(fe.size());
            for (size_t q = 0; q < fe.size(); ++q) phase[q] = (std::arg(fe[q]) + M_PI) * (std::arg(fe[q]) + M_PI);
            HoloOptics::write_bmp(image_dir + "/" + stem + "_era_hologram_phase.bmp", phase, s.W, s.H);
            r.images = image_dir;
        }
    }

    // Acceleration + resolution scaling of the exact CGH kernel (contract v2).
    struct AccelRow {
        int size = 0, threads = 0; size_t points = 0, edits = 0;
        double ref_ms = 0, v1_ms = 0, vt_ms = 0, fp64q_ms = 0;
        size_t mism_v1 = 0, mism_vt = 0, mism_fp64q = 0;
        double era_incr_us = 0, int_full_us = 0, moved = 0;
    };
    static std::vector<AccelRow> accel(const std::string& path, bool quick, int threads) {
        std::vector<AccelRow> rows;
        Rig rig = RigLoader::load_glb(path);
        if (!rig.ok) return rows;
        std::vector<int32_t> w_q;
        const ArcGraph g = EraSkinning::build(rig, w_q);
        size_t oor = 0;
        const auto in0 = EraSkinning::inputs(rig, RigLoader::skin_matrices(rig), g.n, oor);
        Rig posed = rig;
        const auto ps = EraSkinning::pose_stream(rig, g.n, in0, quick ? 4 : 12, 0xACCE, posed, oor);
        const uint32_t base = 12 * static_cast<uint32_t>(rig.joints.size());
        const size_t nv = rig.rest.size();
        using Clk = EraCleanBench::Clock;
        auto ms = [](Clk::time_point t0) { return EraCleanBench::now_us(t0) / 1000.0; };
        for (int S : (quick ? std::vector<int>{128, 256} : std::vector<int>{128, 256, 512})) {
            HoloSetup s; s.W = s.H = S;
            AccelRow a; a.size = S; a.threads = threads; a.points = nv; a.edits = ps.batches.size();
            EraKernel k(g, 1); k.load(in0, w_q);
            const auto pts = era_points(k.values(), base, nv, s);
            QuantHologram<int64_t> hr(s), h1(s), ht(s); QuantHologram<double> hd(s);
            auto t0 = Clk::now(); for (const auto& p : pts) hr.apply_reference(p, 1); a.ref_ms = ms(t0);
            t0 = Clk::now(); h1.full(pts, 1); a.v1_ms = ms(t0);
            t0 = Clk::now(); ht.full(pts, threads); a.vt_ms = ms(t0);
            t0 = Clk::now(); hd.full(pts, 1); a.fp64q_ms = ms(t0);
            a.mism_v1 = h1.mismatch(hr); a.mism_vt = ht.mismatch(hr); a.mism_fp64q = hd.mismatch(hr);
            // Per pose edit at this resolution, both on `threads` workers: ERA incremental vs integer full.
            std::vector<double> inc, full;
            EraKernel ki(g, 1); ki.load(in0, w_q); EraKernel kf(g, 1); kf.load(in0, w_q);
            auto cur = pts; std::vector<PointQ> batch; std::vector<int> signs; double moved = 0;
            ExactHologram hf(s);
            for (const auto& b : ps.batches) {
                t0 = Clk::now();
                EraCleanBench::apply_batch(ki, b); ki.propagate();
                const auto np = era_points(ki.values(), base, nv, s);
                batch.clear(); signs.clear();
                for (size_t q = 0; q < nv; ++q)
                    if (!same(np[q], cur[q])) { batch.push_back(cur[q]); signs.push_back(-1); batch.push_back(np[q]); signs.push_back(1); cur[q] = np[q]; ++moved; }
                ht.apply_points(batch.data(), signs.data(), batch.size(), threads);
                inc.push_back(EraCleanBench::now_us(t0));
                t0 = Clk::now();
                EraCleanBench::apply_batch(kf, b); kf.full_recompute(); hf.full(era_points(kf.values(), base, nv, s), threads);
                full.push_back(EraCleanBench::now_us(t0));
            }
            a.mism_vt += ht.mismatch(hf);  // incremental (threaded, batched) must equal full at the end
            a.era_incr_us = quantile(inc, 0.5); a.int_full_us = quantile(full, 0.5);
            a.moved = moved / static_cast<double>(ps.batches.size());
            rows.push_back(a);
        }
        return rows;
    }

    static int run_cli(const std::string& root, bool quick) {
        namespace fs = std::filesystem;
        const std::string img = quick ? "" : (fs::path(root) / "singular/verification/reports/era_holo").string();
        std::vector<HoloReport> rs;
        for (const char* p : {"/Volumes/Lenovo PS8S/workspace/Simulator/plugins/assets/models/Fox.glb",
                              "/Volumes/Lenovo PS8S/workspace/Simulator/plugins/assets/models/CesiumMan.glb"}) {
            std::cout << "[ERA-HOLO] " << fs::path(p).filename().string() << "\n" << std::flush;
            rs.push_back(run(p, img, quick ? 6 : 24, quick ? 2 : 5, 0x401A));
            const auto& r = rs.back();
            if (!r.ok) { std::printf("  unavailable: %s\n", r.error.c_str()); continue; }
            std::printf("  points=%zu hologram=%dx%d edits=%zu moved points/edit=%.1f | load1 %.1f→%.1f | memory ERA=%.2f MB fp64 holo=%.2f MB\n",
                        r.points, r.W, r.H, r.edits, r.mean_moved_points, r.load_before, r.load_after, r.era_bytes / 1e6, r.fp64_bytes / 1e6);
            for (int v = 0; v < 4; ++v)
                std::printf("  %-62s median=%10.1f µs p95=%10.1f µs  cost÷ERA=%7.2f× [%.2f, %.2f]\n", name(v), r.per_edit[v].median, r.per_edit[v].p95,
                            r.ratio_vs_era[v].median, r.ratio_vs_era[v].ci_lo, r.ratio_vs_era[v].ci_hi);
            std::printf("  exact: incr≠full=%zu undo≠initial=%zu partition-order=%zu | fp64: incr≠full=%zu (max %.3e) undo≠initial=%zu (max %.3e) partition-order=%zu\n",
                        r.era_incr_vs_full_mismatch, r.era_undo_mismatch, r.era_partition_order_mismatch, r.fp64_incr_vs_full_mismatch,
                        r.fp64_incr_vs_full_max_abs, r.fp64_undo_mismatch, r.fp64_undo_max_abs, r.fp64_partition_order_mismatch);
            std::printf("  quality: relL2 CGH quantization=%.3e | end-to-end vs standard fp64 pipeline=%.3e\n", r.rel_l2_cgh_quantization, r.rel_l2_end_to_end);
        }
        bool ok = !rs.empty();
        for (const auto& r : rs) ok = ok && (!r.ok || r.exact_ok());
        std::cout << "[ERA-HOLO] acceleration & resolution scaling (Fox, contract v2)\n" << std::flush;
        const int threads = 4;
        const auto ac = accel("/Volumes/Lenovo PS8S/workspace/Simulator/plugins/assets/models/Fox.glb", quick, threads);
        for (const auto& a : ac) {
            std::printf("  %4d²: full build ms: scalar-ref=%.1f vec=%.1f (%.2fx) vec+%dT=%.1f (%.2fx) fp64-quant=%.1f | mismatch vs ref: vec=%zu vecT+incr=%zu fp64q=%zu"
                        " | per edit (%dT): ERA incr=%.0f µs vs int full=%.0f µs (%.2fx), moved=%.0f\n",
                        a.size, a.ref_ms, a.v1_ms, a.ref_ms / a.v1_ms, a.threads, a.vt_ms, a.ref_ms / a.vt_ms, a.fp64q_ms, a.mism_v1, a.mism_vt, a.mism_fp64q,
                        a.threads, a.era_incr_us, a.int_full_us, a.int_full_us / a.era_incr_us, a.moved);
            ok = ok && a.mism_v1 == 0 && a.mism_vt == 0;
        }
        if (!quick) {
            const fs::path disc = fs::path(root) / "singular/discoveries/registry/DISC-000902";
            std::error_code ec; fs::create_directories(disc, ec);
            std::ofstream j(disc / "evidence.json");
            j.precision(10);
            j << "{\n  \"id\": \"DISC-000902\", \"status\": \"RESEARCH_CANDIDATE\", \"exact\": " << (ok ? "true" : "false")
              << ", \"source\": \"singular/verification/era/era_audit.sh era-holo\", \"host\": \"" << EraBenchmark::host_cpu() << "\",\n  \"runs\": [";
            for (size_t i = 0; i < rs.size(); ++i) {
                const auto& r = rs[i];
                j << (i ? ",\n" : "\n") << "    {\"asset\": \"" << r.path << "\", \"ok\": " << (r.ok ? "true" : "false") << ", \"error\": \"" << r.error << "\", \"points\": " << r.points
                  << ", \"W\": " << r.W << ", \"H\": " << r.H << ", \"edits\": " << r.edits << ", \"trials\": " << r.trials << ", \"mean_moved_points\": " << r.mean_moved_points
                  << ", \"load1\": [" << r.load_before << ", " << r.load_after << "], \"era_bytes\": " << r.era_bytes << ", \"fp64_holo_bytes\": " << r.fp64_bytes
                  << ", \"era_incr_vs_full_mismatch\": " << r.era_incr_vs_full_mismatch << ", \"era_undo_mismatch\": " << r.era_undo_mismatch
                  << ", \"era_partition_order_mismatch\": " << r.era_partition_order_mismatch << ", \"fp64_incr_vs_full_mismatch\": " << r.fp64_incr_vs_full_mismatch
                  << ", \"fp64_incr_vs_full_max_abs\": " << r.fp64_incr_vs_full_max_abs << ", \"fp64_undo_mismatch\": " << r.fp64_undo_mismatch
                  << ", \"fp64_undo_max_abs\": " << r.fp64_undo_max_abs << ", \"fp64_partition_order_mismatch\": " << r.fp64_partition_order_mismatch
                  << ", \"rel_l2_cgh_quantization\": " << r.rel_l2_cgh_quantization << ", \"rel_l2_end_to_end\": " << r.rel_l2_end_to_end << ", \"variants\": [";
                for (int v = 0; v < 4; ++v)
                    j << (v ? ", " : "") << "{\"variant\": \"" << name(v) << "\", \"median_us\": " << r.per_edit[v].median << ", \"p95_us\": " << r.per_edit[v].p95
                      << ", \"cost_over_era\": " << r.ratio_vs_era[v].median << ", \"ci95\": [" << r.ratio_vs_era[v].ci_lo << ", " << r.ratio_vs_era[v].ci_hi << "]}";
                j << "]}";
            }
            j << "\n  ],\n  \"acceleration\": [";
            for (size_t i = 0; i < ac.size(); ++i) {
                const auto& a = ac[i];
                j << (i ? ", " : "") << "{\"size\": " << a.size << ", \"threads\": " << a.threads << ", \"points\": " << a.points << ", \"edits\": " << a.edits
                  << ", \"full_ms\": {\"scalar_reference\": " << a.ref_ms << ", \"vectorized_1T\": " << a.v1_ms << ", \"vectorized_T\": " << a.vt_ms << ", \"fp64_same_quantized_terms\": " << a.fp64q_ms
                  << "}, \"mismatch_vs_reference\": {\"vectorized_1T\": " << a.mism_v1 << ", \"vectorized_T_plus_incremental\": " << a.mism_vt << ", \"fp64_same_quantized_terms\": " << a.mism_fp64q
                  << "}, \"per_edit_median_us\": {\"era_incremental\": " << a.era_incr_us << ", \"integer_full\": " << a.int_full_us << "}, \"mean_moved_points\": " << a.moved << "}";
            }
            j << "]\n}\n";
            std::cout << "[ERA-HOLO] wrote " << (disc / "evidence.json").string() << " and images in " << img << "\n";
        }
        std::cout << "[ERA-HOLO] exactness verdict: " << (ok ? "HOLD" : "VIOLATED") << " (speed reported, not gated)\n";
        return ok ? 0 : 1;
    }
};

} // namespace cycraft::omega::quantum_syntax::era
