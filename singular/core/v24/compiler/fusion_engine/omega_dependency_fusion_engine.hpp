#pragma once
// SINGULAR Ω v24 — `Ω-FUSION`: Dependency-, Version- & Precision-Aware Kernel Fusion Engine
#include "../omega_ir/omega_ssa_versioned_ir.hpp"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace singular::v24::compiler {

struct FusionExecutionMetrics {
    double unfused_us = 0.0;
    double fused_us   = 0.0;
    double speedup_x  = 0.0;
    double max_ulp_err = 0.0;
    bool used_dense_fallback = false;
};

inline void execute_unfused_4pass(const std::vector<double>& x, const std::vector<double>& w,
                                  const std::vector<double>& b, std::vector<double>& tmp1,
                                  std::vector<double>& tmp2, std::vector<double>& out) noexcept {
    const size_t n = x.size();
    for (size_t i = 0; i < n; ++i) tmp1[i] = x[i] * w[i];
    for (size_t i = 0; i < n; ++i) tmp2[i] = tmp1[i] + b[i];
    for (size_t i = 0; i < n; ++i) out[i]  = std::tanh(tmp2[i]);
}

inline void execute_omega_fused_cone(const std::vector<double>& x, const std::vector<double>& w,
                                     const std::vector<double>& b, std::vector<double>& out,
                                     uint64_t dirty_cone_mask, size_t active_k) noexcept {
    const size_t n = x.size();
    // Crossover law: if active fraction > 22%, execute single-pass dense fused loop
    if (active_k * 100 > n * 22) {
        for (size_t i = 0; i < n; ++i) out[i] = std::tanh(x[i] * w[i] + b[i]);
        return;
    }
    for (size_t c = 0; c < active_k && c < 64; ++c) {
        if (dirty_cone_mask & (1ULL << c)) {
            out[c] = std::tanh(x[c] * w[c] + b[c]);
        }
    }
}

} // namespace singular::v24::compiler
