#pragma once
// SINGULAR Ω v24 — 3-Workload Benchmark Suite (Incremental Graph, Fused Tensor, Tokamak MHD)
#include "../../compiler/fusion_engine/omega_dependency_fusion_engine.hpp"
#include <chrono>

namespace singular::v24::verification {

struct WorkloadBenchmarkSummary {
    double w1_inc_graph_speedup_x  = 0.0;
    double w2_fused_tensor_speedup_x = 0.0;
    double w3_tokamak_cone_speedup_x = 0.0;
};

} // namespace singular::v24::verification
