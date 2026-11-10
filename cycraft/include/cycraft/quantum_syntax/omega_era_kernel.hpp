// ============================================================================
// SINGULAR Ω — ERA kernel (auto int64 → i128 promotion) & independent oracle
// (`DISC-000901`). Rule `SG-ARCH-400`: <= 400 physical lines.
//
// EraKernel runs on the int64 accumulator while the model is admitted
// (Theorem 5 in ERA_FORMAL_MODEL.md) and promotes itself to __int128 the moment an
// op would leave the admitted region — before any accumulator can overflow.
// Promotion is a full evaluation from the current (inputs, weights); because the
// state is a pure function of them (Theorem 1) the values do not change.
//
// EraOracle is an independent reference evaluator written without ArcEngine,
// without the traits, without the CSR: it sorts arcs by destination itself and
// requantizes with floor division instead of shifts. Differential tests compare
// the kernel against it.
// ============================================================================
#pragma once
#include "cycraft/quantum_syntax/omega_exact_reversible_arc_engine.hpp"
#include <atomic>
#include <barrier>
#include <numeric>
#include <optional>
#include <thread>

namespace cycraft::omega::quantum_syntax::era {

class EraKernel {
public:
    EraKernel(const ArcGraph& g, size_t undo_cap) : g_(&g), cap_(undo_cap) {}

    void load(const std::vector<int64_t>& in_q, const std::vector<int32_t>& w_q) {
        e128_.reset();
        e64_.emplace(*g_, cap_);
        e64_->load(in_q, w_q);
        if (!e64_->admitted()) { e64_.reset(); e128_.emplace(*g_, cap_); e128_->load(in_q, w_q); }
    }
    bool set_input(uint32_t v, int64_t q, bool journal = true) {
        if (e64_) { if (e64_->set_input(v, q, journal)) return true; promote(); }
        return e128_->set_input(v, q, journal);
    }
    bool set_weight(uint32_t e, int32_t q, bool journal = true) {
        if (e64_) { if (e64_->set_weight(e, q, journal)) return true; promote(); }
        return e128_->set_weight(e, q, journal);
    }
    uint64_t propagate() { return e64_ ? e64_->propagate(true) : e128_->propagate(true); }
    bool undo_one() {
        if (e64_) {
            if (e64_->journal().size() == 0) return false;
            if (e64_->undo_one()) return true;
            promote();
        }
        return e128_->undo_one();
    }
    void full_recompute() { if (e64_) e64_->full_recompute(); else e128_->full_recompute(); }
    void set_sweep_density(double rho) { if (e64_) e64_->set_sweep_density(rho); else e128_->set_sweep_density(rho); }

    const std::vector<int64_t>& values() const { return e64_ ? e64_->values() : e128_->values(); }
    const std::vector<int64_t>& inputs() const { return e64_ ? e64_->inputs() : e128_->inputs(); }
    const std::vector<int32_t>& weights() const { return e64_ ? e64_->weights() : e128_->weights(); }
    bool wide() const { return !e64_.has_value(); }
    size_t promotions() const { return promotions_; }
    size_t journal_size() const { return e64_ ? e64_->journal().size() : e128_->journal().size(); }
    uint64_t last_changed() const { return e64_ ? e64_->last_changed() : e128_->last_changed(); }
    size_t state_bytes() const { return e64_ ? e64_->state_bytes() : e128_->state_bytes(); }

private:
    void promote() {
        e128_.emplace(*g_, cap_);
        e128_->load(e64_->inputs(), e64_->weights());
        e128_->adopt_journal(e64_->journal());
        e64_.reset();
        ++promotions_;
    }
    const ArcGraph* g_;
    size_t cap_;
    std::optional<ArcEngine<Exact64Traits>> e64_;
    std::optional<ArcEngine<ExactTraits>> e128_;
    size_t promotions_ = 0;
};

// Static check of the int64 admission region (Theorem 5) without building an engine.
inline bool exact64_admissible(const ArcGraph& g, const std::vector<int64_t>& in_q, const std::vector<int32_t>& w_q) {
    for (int64_t q : in_q) if (!Exact64Traits::admits_input(q)) return false;
    std::vector<int64_t> s(g.n, 0);
    for (size_t e = 0; e < w_q.size(); ++e) {
        if (!Exact64Traits::admits_weight(w_q[e])) return false;
        s[g.e_dst[e]] += std::abs(static_cast<int64_t>(w_q[e]));
    }
    for (int64_t x : s) if (x > Exact64Traits::kMaxWeightSumQ) return false;
    return true;
}

// Optimized competitor with the SAME semantics: level-synchronous parallel full
// recompute (nodes grouped by longest-path depth; each level split over a
// persistent thread pool with barriers). Not incremental, no journal.
template <class T>
class ParallelFull {
public:
    ParallelFull(const ArcGraph& g, int threads) : g_(&g), T_(std::max(1, threads)), bar_(std::max(1, threads)) {
        std::vector<uint32_t> depth(g.n, 0);
        uint32_t maxd = 0;
        for (uint32_t v = 0; v < g.n; ++v) {
            for (uint32_t k = g.in_off[v]; k < g.in_off[v + 1]; ++k) depth[v] = std::max(depth[v], depth[g.e_src[g.in_eid[k]]] + 1);
            maxd = std::max(maxd, depth[v]);
        }
        lvl_off_.assign(maxd + 2, 0);
        for (uint32_t v = 0; v < g.n; ++v) ++lvl_off_[depth[v] + 1];
        for (size_t l = 1; l < lvl_off_.size(); ++l) lvl_off_[l] += lvl_off_[l - 1];
        lvl_nodes_.resize(g.n);
        std::vector<uint32_t> cur(lvl_off_.begin(), lvl_off_.end() - 1);
        for (uint32_t v = 0; v < g.n; ++v) lvl_nodes_[cur[depth[v]]++] = v;
        // Segments: maximal runs of consecutive small levels execute serially on worker 0
        // (no barriers inside); each large level is its own parallel segment.
        for (size_t l = 0; l + 1 < lvl_off_.size();) {
            const bool big = lvl_off_[l + 1] - lvl_off_[l] >= static_cast<uint32_t>(256 * T_);
            size_t e = l + 1;
            if (!big) while (e + 1 < lvl_off_.size() && lvl_off_[e + 1] - lvl_off_[e] < static_cast<uint32_t>(256 * T_)) ++e;
            seg_.push_back({lvl_off_[l], lvl_off_[e], big});
            par_ = par_ || big;
            l = e;
        }
        val_.assign(g.n, typename T::Val{});
        for (int t = 1; t < T_; ++t) pool_.emplace_back([this, t] { for (;;) { bar_.arrive_and_wait(); if (stop_) return; run(t); bar_.arrive_and_wait(); } });
    }
    ~ParallelFull() { stop_ = true; if (T_ > 1) bar_.arrive_and_wait(); for (auto& th : pool_) th.join(); }
    ParallelFull(const ParallelFull&) = delete;
    ParallelFull& operator=(const ParallelFull&) = delete;
    void load(const std::vector<int64_t>& in_q, const std::vector<int32_t>& w_q) {
        in_q_ = in_q; w_q_ = w_q; w_.resize(w_q.size());
        for (size_t e = 0; e < w_q.size(); ++e) w_[e] = T::weight(w_q[e]);
        full_recompute();
    }
    bool set_input(uint32_t v, int64_t q, bool = false) { in_q_[v] = q; return true; }
    bool set_weight(uint32_t e, int32_t q, bool = false) { w_q_[e] = q; w_[e] = T::weight(q); return true; }
    void full_recompute() {
        if (T_ == 1 || !any_parallel()) { run(0); return; }  // no parallel level: plain sequential, no wake-ups
        bar_.arrive_and_wait(); run(0); bar_.arrive_and_wait();
    }
    bool any_parallel() const { return par_; }
    const std::vector<typename T::Val>& values() const { return val_; }
    size_t levels() const { return lvl_off_.size() - 1; }
    size_t state_bytes() const { return in_q_.size() * 8 + w_q_.size() * 4 + w_.size() * sizeof(typename T::W) + val_.size() * 8 + lvl_nodes_.size() * 4; }
private:
    void run(int t) {
        const ArcGraph& g = *g_;
        for (const Seg& s : seg_) {
            if (s.parallel || t == 0)
                for (uint32_t i = s.a + (s.parallel ? static_cast<uint32_t>(t) : 0); i < s.b; i += (s.parallel ? static_cast<uint32_t>(T_) : 1)) {
                    const uint32_t v = lvl_nodes_[i];
                    typename T::Acc acc = T::lift_input(in_q_[v]);
                    for (uint32_t k = g.in_off[v]; k < g.in_off[v + 1]; ++k) {
                        const uint32_t e = g.in_eid[k];
                        if (w_q_[e] != 0) acc += T::term(w_[e], val_[g.e_src[e]]);
                    }
                    val_[v] = T::apply(g, acc);
                }
            if (T_ > 1 && any_parallel()) bar_.arrive_and_wait();
        }
    }
    struct Seg { uint32_t a, b; bool parallel; };
    std::vector<Seg> seg_;
    bool par_ = false;  // at least one level is wide enough to split
    const ArcGraph* g_;
    int T_;
    std::barrier<> bar_;
    std::vector<std::thread> pool_;
    std::atomic<bool> stop_{false};
    std::vector<uint32_t> lvl_off_, lvl_nodes_;
    std::vector<int64_t> in_q_;
    std::vector<int32_t> w_q_;
    std::vector<typename T::W> w_;
    std::vector<typename T::Val> val_;
};

struct EraOracle {
    // Round-half-to-even of acc / 2^sh using floor division (no shifts on acc).
    static i128 rne_div(i128 acc, int sh) {
        const i128 d = static_cast<i128>(1) << sh;
        i128 q = acc / d;
        if (acc % d != 0 && acc < 0) --q;  // floor
        const i128 r = acc - q * d;        // 0 <= r < d
        if (2 * r > d || (2 * r == d && (q % 2 != 0))) ++q;
        return q;
    }
    // Returns node values in Q32, or an empty vector if the graph is not src<dst.
    static std::vector<int64_t> evaluate(const ArcGraph& g, const std::vector<int64_t>& in_q,
                                         const std::vector<int32_t>& w_q) {
        std::vector<uint32_t> order(g.e_src.size());
        std::iota(order.begin(), order.end(), 0u);
        std::stable_sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) { return g.e_dst[a] < g.e_dst[b]; });
        for (uint32_t e : order) if (g.e_src[e] >= g.e_dst[e]) return {};
        const int fb = g.frac_bits;
        const i128 hi = static_cast<i128>(8) << fb;   // 8.0 in units of 2^-fb
        const i128 lo = (g.act == Act::ReluClamp) ? 0 : -hi;
        std::vector<int64_t> val(g.n, 0);
        size_t k = 0;
        for (uint32_t v = 0; v < g.n; ++v) {
            i128 acc = static_cast<i128>(in_q[v]) * 65536;  // input Q32 -> accumulator Q48
            for (; k < order.size() && g.e_dst[order[k]] == v; ++k) {
                const uint32_t e = order[k];
                acc += static_cast<i128>(w_q[e]) * static_cast<i128>(val[g.e_src[e]]);
            }
            i128 q = rne_div(acc, 48 - fb);
            q = std::min(hi, std::max(lo, q));
            val[v] = static_cast<int64_t>(q * (static_cast<i128>(1) << (32 - fb)));
        }
        return val;
    }
};

} // namespace cycraft::omega::quantum_syntax::era
