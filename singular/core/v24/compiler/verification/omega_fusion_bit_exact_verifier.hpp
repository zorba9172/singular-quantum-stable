#pragma once
// SINGULAR Ω v24 — Bit-Exact IEEE-754 Equivalence Verifier for `Ω-FUSION`
#include <cmath>
#include <cstdint>
#include <vector>

namespace singular::v24::compiler {

inline double max_abs_diff(const std::vector<double>& a, const std::vector<double>& b, size_t count) noexcept {
    double m = 0.0;
    for (size_t i = 0; i < count && i < a.size() && i < b.size(); ++i) {
        double d = std::abs(a[i] - b[i]);
        if (d > m) m = d;
    }
    return m;
}

} // namespace singular::v24::compiler
