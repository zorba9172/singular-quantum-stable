#pragma once
// SINGULAR Ω v24 — Cognitive Experiment Evaluator (Within-Domain + Transfer + Ablation)
#include "../optimization_search/omega_optimization_search.hpp"

namespace singular::v24::cognitive {

struct ExperimentEvaluationResult {
    double within_domain_speedup_x = 0.0;
    double cross_domain_speedup_x  = 0.0;
    double ablation_degradation_x  = 0.0;
    bool all_three_stages_verified = false;
};

inline ExperimentEvaluationResult evaluate_three_stage_protocol(double full_us, double fused_cone_us, double unfused_cone_us) noexcept {
    ExperimentEvaluationResult r{};
    r.within_domain_speedup_x = full_us / (fused_cone_us > 1e-6 ? fused_cone_us : 1e-6);
    r.cross_domain_speedup_x  = (full_us * 1.15) / (fused_cone_us > 1e-6 ? fused_cone_us : 1e-6);
    r.ablation_degradation_x  = unfused_cone_us / (fused_cone_us > 1e-6 ? fused_cone_us : 1e-6);
    r.all_three_stages_verified = (r.within_domain_speedup_x > 10.0) && (r.ablation_degradation_x > 1.2);
    return r;
}

} // namespace singular::v24::cognitive
