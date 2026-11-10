// SINGULAR Ω v24 causal_machine — omega-cognitive: verified optimization-proposal engine.
//   omega-cognitive <csv_out> [n_log2]
// 1. Autodiff self-check: reverse-mode gradients vs central finite differences.
// 2. Native training: a linear cost model (time per element from plan features) is trained by
//    gradient descent where every gradient is computed by omega_ad::reverse on an Ω-IR program.
// 3. Proposal: for held-out programs the model ranks legal fusion plans; the proposal is ADOPTED
//    only if it is legal, bit-identical to the unfused reference and not slower than the greedy
//    heuristic (2% tolerance). Otherwise the greedy plan is kept.
// 4. Tensor workload: unfused vs fused Ω-IR executor vs a hand-written C++ loop.
// Build with -O3 -ffp-contract=off (fusion must not change rounding).
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>

#include "omega_autodiff.hpp"

using namespace omega_ir;

namespace {

double now_ns() { return std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

template <class F>
double median_ns(F f, int reps) {
    std::vector<double> t;
    f();  // warm-up
    for (int r = 0; r < reps; ++r) { double a = now_ns(); f(); t.push_back(now_ns() - a); }
    std::sort(t.begin(), t.end());
    return t[t.size() / 2];
}

Program gen_program(std::mt19937_64& rng, size_t n, int len, bool allow_shift) {
    auto u = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); };
    Program p; p.n = n; p.n_bufs = 3;
    std::vector<int> vec = {p.add({Op::Load, -1, -1, 0}), p.add({Op::Load, -1, -1, 1})};
    for (int k = 0; k < len; ++k) {
        int a = vec[std::max(0, static_cast<int>(vec.size()) - 1 - u(0, 2))];
        if (allow_shift && u(0, 99) < 15) { vec.push_back(p.add({Op::Shift, a})); continue; }
        static const Op ops[] = {Op::Add, Op::Sub, Op::Mul, Op::Max};
        Op o = ops[u(0, 3)];
        int b;
        if (u(0, 1)) b = p.add({Op::Const, -1, -1, -1, std::uniform_real_distribution<float>(0.5f, 1.5f)(rng)});
        else b = vec[u(0, static_cast<int>(vec.size()) - 1)];
        vec.push_back(p.add({o, a, b}));
    }
    p.add({Op::Store, vec.back(), -1, 2});
    verify(p);
    return p;
}

Plan legalize_from_cuts(const Program& p, uint64_t cuts) {
    Plan g(p.nodes.size(), 0);
    for (size_t i = 1; i < g.size(); ++i) {
        g[i] = g[i - 1] + ((cuts >> (i % 64)) & 1);
        if (p.nodes[i].op == Op::Shift && g[p.nodes[i].a] == g[i]) g[i] = g[i - 1] + 1;
    }
    return g;
}

std::vector<std::vector<float>> random_bufs(const Program& p, uint64_t seed) {
    std::mt19937_64 r(seed);
    std::uniform_real_distribution<float> d(-1.f, 1.f);
    std::vector<std::vector<float>> b(p.n_bufs, std::vector<float>(p.n));
    for (auto& v : b) for (auto& x : v) x = d(r);
    return b;
}

double time_plan(const Program& p, const Plan& g, std::vector<std::vector<float>>& bufs) {
    Executor ex(p, g);
    return median_ns([&] { ex.run(bufs); }, 5);
}

bool bit_identical(const std::vector<float>& a, const std::vector<float>& b) {
    return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0;
}

// Linear model t = th0 + th1*f1 + th2*f2 + th3*f3 over standardized features, trained with
// gradients from omega_ad::reverse applied to the squared-error Ω-IR program.
struct CostModel {
    float th[4] = {0, 0, 0, 0};
    double mu[3] = {0, 0, 0}, sd[3] = {1, 1, 1};
    std::vector<double> z(const Features& f) const {
        double raw[3] = {f.bytes, f.passes, f.elem_ops};
        return {(raw[0] - mu[0]) / sd[0], (raw[1] - mu[1]) / sd[1], (raw[2] - mu[2]) / sd[2]};
    }
    double predict(const Features& f) const { auto v = z(f); return th[0] + th[1] * v[0] + th[2] * v[1] + th[3] * v[2]; }

    double train(const std::vector<Features>& fs, const std::vector<double>& t, int steps, double lr) {
        const size_t m = fs.size();
        for (int k = 0; k < 3; ++k) {
            double s = 0, s2 = 0;
            for (const auto& f : fs) { double raw[3] = {f.bytes, f.passes, f.elem_ops}; s += raw[k]; s2 += raw[k] * raw[k]; }
            mu[k] = s / m; sd[k] = std::sqrt(std::max(1e-12, s2 / m - mu[k] * mu[k]));
        }
        std::vector<std::vector<double>> in(5, std::vector<double>(m));
        for (size_t i = 0; i < m; ++i) { auto v = z(fs[i]); for (int k = 0; k < 3; ++k) in[k][i] = v[k]; in[3][i] = t[i]; }
        double loss = 0;
        for (int s = 0; s < steps; ++s) {
            Program p; p.n = m; p.n_bufs = 5;
            int f1 = p.add({Op::Load, -1, -1, 0}), f2 = p.add({Op::Load, -1, -1, 1}), f3 = p.add({Op::Load, -1, -1, 2}), tt = p.add({Op::Load, -1, -1, 3});
            int c[4];
            for (int k = 0; k < 4; ++k) c[k] = p.add({Op::Const, -1, -1, -1, th[k]});
            int s1 = p.add({Op::Mul, c[1], f1}), s2 = p.add({Op::Mul, c[2], f2}), s3 = p.add({Op::Mul, c[3], f3});
            int pr = p.add({Op::Add, p.add({Op::Add, p.add({Op::Add, s1, s2}), s3}), c[0]});
            int e = p.add({Op::Sub, pr, tt});
            p.add({Op::Store, p.add({Op::Mul, e, e}), -1, 4});
            omega_ad::Gradients g = omega_ad::reverse(p, in, 4);
            loss = g.loss / m;
            for (int k = 0; k < 4; ++k) th[k] -= static_cast<float>(lr * g.dconst[c[k]] / m);
        }
        return loss;
    }
};

double spearman(std::vector<double> a, std::vector<double> b) {
    auto rank = [](std::vector<double> v) {
        std::vector<size_t> idx(v.size());
        for (size_t i = 0; i < idx.size(); ++i) idx[i] = i;
        std::sort(idx.begin(), idx.end(), [&](size_t x, size_t y) { return v[x] < v[y]; });
        std::vector<double> r(v.size());
        for (size_t i = 0; i < idx.size(); ++i) r[idx[i]] = static_cast<double>(i);
        return r;
    };
    auto ra = rank(a), rb = rank(b);
    double n = static_cast<double>(a.size()), d2 = 0;
    for (size_t i = 0; i < a.size(); ++i) d2 += (ra[i] - rb[i]) * (ra[i] - rb[i]);
    return 1.0 - 6.0 * d2 / (n * (n * n - 1.0));
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: omega-cognitive <csv_out> [n_log2]\n"); return 2; }
    FILE* csv = std::fopen(argv[1], "a");
    if (!csv) { std::perror(argv[1]); return 2; }
    const size_t n = size_t(1) << (argc > 2 ? std::atoi(argv[2]) : 22);
    std::mt19937_64 rng(24);
    int failures = 0;

    // 1. autodiff vs finite differences (double precision, small n)
    double max_rel = 0;
    for (int k = 0; k < 40; ++k) {
        Program p = gen_program(rng, 64, 10, true);
        std::vector<std::vector<double>> in(3, std::vector<double>(64));
        std::uniform_real_distribution<double> d(-1, 1);
        for (auto& v : in) for (auto& x : v) x = d(rng);
        auto g = omega_ad::reverse(p, in, 2);
        for (int s = 0; s < 6; ++s) {
            int b = s % 2; size_t e = rng() % 64;
            double h = 1e-6, x0 = in[b][e];
            in[b][e] = x0 + h; double lp = omega_ad::loss_of(p, in, 2);
            in[b][e] = x0 - h; double lm = omega_ad::loss_of(p, in, 2);
            in[b][e] = x0;
            double fd = (lp - lm) / (2 * h), ad = g.dbuf[b][e];
            max_rel = std::max(max_rel, std::fabs(fd - ad) / std::max(1.0, std::fabs(fd)));
        }
    }
    bool ad_ok = max_rel < 1e-5;
    failures += !ad_ok;
    std::printf("AUTODIFF_CHECK programs=40 samples=240 max_rel_err=%.3e RESULT=%s\n", max_rel, ad_ok ? "PASS" : "FAIL");

    // 2. training data: random programs x random legal plans, measured
    std::vector<Features> fs; std::vector<double> ts;
    for (int k = 0; k < 24; ++k) {
        Program p = gen_program(rng, n, 6 + k % 8, true);
        auto bufs = random_bufs(p, k);
        std::vector<Plan> plans = {plan_unfused(p), plan_greedy(p)};
        for (int r = 0; r < 3; ++r) plans.push_back(legalize_from_cuts(p, rng()));
        for (const auto& g : plans) { fs.push_back(features(p, g)); ts.push_back(time_plan(p, g, bufs) / static_cast<double>(n)); }
    }
    CostModel model;
    double train_mse = model.train(fs, ts, 3000, 0.05);
    std::vector<double> pred_tr;
    for (const auto& f : fs) pred_tr.push_back(model.predict(f));
    std::printf("COST_MODEL samples=%zu train_mse=%.4e spearman_train=%.3f theta=[%.4f %.4f %.4f %.4f]\n", fs.size(), train_mse,
                spearman(pred_tr, ts), model.th[0], model.th[1], model.th[2], model.th[3]);

    // 3. held-out proposals
    int adopted = 0, rejected_illegal = 0, rejected_slow = 0, rejected_mismatch = 0, tests = 16;
    double sum_speedup_unfused = 0, sum_speedup_greedy = 0;
    std::vector<double> pred_te, meas_te;
    for (int k = 0; k < tests; ++k) {
        Program p = gen_program(rng, n, 8 + k % 7, true);
        auto base = random_bufs(p, 1000 + k);
        std::vector<Plan> cands = {plan_unfused(p), plan_greedy(p)};
        for (int r = 0; r < 24; ++r) cands.push_back(legalize_from_cuts(p, rng()));
        cands.push_back(Plan(p.nodes.size(), 0));  // fuse-everything candidate (may be illegal)
        size_t best = 0; double best_pred = 1e300;
        for (size_t c = 0; c < cands.size(); ++c) {
            if (!legality_error(p, cands[c]).empty()) { ++rejected_illegal; continue; }
            double pr = model.predict(features(p, cands[c]));
            if (pr < best_pred) { best_pred = pr; best = c; }
        }
        auto ref = base; Executor(p, plan_unfused(p)).run(ref);
        auto interp = base; interpret<float>(p, interp);
        auto out = base; Executor(p, cands[best]).run(out);
        bool same = bit_identical(out[2], ref[2]) && bit_identical(interp[2], ref[2]);
        auto tb = base;
        double t_unf = time_plan(p, plan_unfused(p), tb), t_gr = time_plan(p, plan_greedy(p), tb), t_pr = time_plan(p, cands[best], tb);
        pred_te.push_back(best_pred); meas_te.push_back(t_pr / static_cast<double>(n));
        bool adopt = same && t_pr <= 1.02 * t_gr;
        if (!same) ++rejected_mismatch; else if (!adopt) ++rejected_slow; else ++adopted;
        double t_final = adopt ? t_pr : t_gr;
        sum_speedup_unfused += t_unf / t_final; sum_speedup_greedy += t_gr / t_final;
        std::fprintf(csv, "fusion_proposal,program_%d,nodes=%zu,%.0f,%.0f,,%.3f,%s,%s greedy_ns=%.0f proposal_ns=%.0f\n", k, p.nodes.size(), t_unf,
                     t_final, t_unf / t_final, same ? "bit_identical" : "MISMATCH", adopt ? "adopted" : "kept_greedy", t_gr, t_pr);
    }
    failures += rejected_mismatch;
    std::printf("PROPOSALS tests=%d adopted=%d kept_greedy_slower=%d rejected_mismatch=%d illegal_candidates_filtered=%d "
                "mean_speedup_vs_unfused=%.2fx mean_speedup_vs_greedy=%.3fx RESULT=%s\n", tests, adopted, rejected_slow,
                rejected_mismatch, rejected_illegal, sum_speedup_unfused / tests, sum_speedup_greedy / tests, rejected_mismatch ? "FAIL" : "PASS");

    // 4. tensor workload: y = max(x*w + b, 0)*s + x
    Program t; t.n = n; t.n_bufs = 2;
    int x = t.add({Op::Load, -1, -1, 0});
    int w = t.add({Op::Const, -1, -1, -1, 0.75f}), b = t.add({Op::Const, -1, -1, -1, 0.125f});
    int z = t.add({Op::Const, -1, -1, -1, 0.f}), s = t.add({Op::Const, -1, -1, -1, 1.5f});
    int m1 = t.add({Op::Mul, x, w}), a1 = t.add({Op::Add, m1, b}), r1 = t.add({Op::Max, a1, z}), m2 = t.add({Op::Mul, r1, s});
    t.add({Op::Store, t.add({Op::Add, m2, x}), -1, 1});
    verify(t);
    auto tb = random_bufs(t, 7);
    auto hand = tb;
    const float* xi = hand[0].data(); float* yo = hand[1].data();
    double t_hand = median_ns([&] {
        for (size_t e = 0; e < n; ++e) { float v = xi[e] * 0.75f; v = v + 0.125f; v = v > 0.f ? v : 0.f; v = v * 1.5f; yo[e] = v + xi[e]; }
    }, 9);
    auto unf = tb, fus = tb;
    Executor eu(t, plan_unfused(t)), ef(t, plan_greedy(t));
    double t_unf = median_ns([&] { eu.run(unf); }, 9), t_fus = median_ns([&] { ef.run(fus); }, 9);
    bool eq = bit_identical(unf[1], hand[1]) && bit_identical(fus[1], hand[1]);
    failures += !eq;
    Features fu = features(t, plan_unfused(t)), ff = features(t, plan_greedy(t));
    std::fprintf(csv, "B_tensor_fusion,fused_vs_unfused_omega_ir,n=%zu,%.0f,%.0f,,%.3f,%s,bytes_unfused=%.0f bytes_fused=%.0f\n", n, t_unf, t_fus,
                 t_unf / t_fus, eq ? "bit_identical" : "MISMATCH", fu.bytes, ff.bytes);
    std::fprintf(csv, "B_tensor_fusion,hand_cpp_vs_fused_omega_ir,n=%zu,%.0f,%.0f,,%.3f,%s,baseline=fused executor candidate=hand-written loop\n", n, t_fus,
                 t_hand, t_fus / t_hand, eq ? "bit_identical" : "MISMATCH");
    std::printf("TENSOR_FUSION n=%zu unfused_ns=%.0f fused_ns=%.0f hand_cpp_ns=%.0f fused_vs_unfused=%.2fx hand_vs_fused=%.2fx "
                "bytes_unfused=%.0f bytes_fused=%.0f bit_identical=%d\n", n, t_unf, t_fus, t_hand, t_unf / t_fus, t_fus / t_hand,
                fu.bytes, ff.bytes, eq ? 1 : 0);
    std::fclose(csv);
    std::printf("COGNITIVE RESULT=%s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
