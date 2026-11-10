// ============================================================================
// SINGULAR Ω — Exact Reversible Arc Engine (ERA, `DISC-000901`)
//
// Mechanism: incremental (Δ⁺) / decremental (Δ⁻) / undo (↶) maintenance of a
// weighted dataflow DAG  v_i = act( b_i + Σ_{j→i} w_ji · v_j )  where every
// accumulator lives in an exact abelian group (fixed-point ℤ, __int128) and the
// only rounding step (requantize + activation) is a pure function of that exact
// accumulator. Consequences (each one is measured by the benchmark header):
//   I1  incremental state == full recompute state, bit for bit, after any op mix;
//   I2  undoing any op prefix restores the earlier state bit for bit (0 ULP);
//   I3  the final state is independent of the order in which ops are applied
//       (replicas converge without coordination);
//   I4  propagation stops at nodes whose requantized output did not change
//       (dynamic value cut-off: the cone actually touched ⊆ static descendant cone).
// The same template instantiated with `DoubleTraits` is the IEEE-754 baseline,
// so both sides run the identical graph, op stream and propagation code.
// Rule `SG-ARCH-400`: <= 400 physical lines.
// ============================================================================
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <bit>
#include <climits>
#include <cstddef>
#include <random>
#include <vector>

namespace cycraft::omega::quantum_syntax::era {

using i128 = __int128;
inline constexpr int kValFrac = 32;               // node values / inputs: Q31.32 in int64
inline constexpr int kWFrac = 16;                 // arc weights: Q15.16 in int32
inline constexpr int64_t kClampQ = int64_t{8} << kValFrac;  // activation range [-8, 8]

enum class Act : uint8_t { HardTanh = 0, ReluClamp = 1 };

// Fixed candidate-arc set; an arc is "present" iff its weight is non-zero, so
// Δ⁺ arc insertion / Δ⁻ arc deletion are weight transitions 0 ↔ w.
// Invariant: every arc satisfies src < dst, hence node id order is topological.
struct ArcGraph {
    uint32_t n = 0;
    Act act = Act::HardTanh;
    // Output precision grid (fractional bits, 1..32). Node outputs are rounded to
    // multiples of 2^-frac_bits by BOTH full recompute and incremental paths, so a
    // coarser grid enlarges the value cut-off (I4) without breaking I1..I3.
    uint8_t frac_bits = kValFrac;
    std::vector<uint32_t> e_src, e_dst;
    std::vector<uint32_t> in_off, in_eid, out_off, out_eid, out_dst;

    void finalize() {
        const size_t m = e_src.size();
        in_off.assign(n + 1, 0); out_off.assign(n + 1, 0);
        for (size_t e = 0; e < m; ++e) { ++in_off[e_dst[e] + 1]; ++out_off[e_src[e] + 1]; }
        for (uint32_t v = 0; v < n; ++v) { in_off[v + 1] += in_off[v]; out_off[v + 1] += out_off[v]; }
        in_eid.assign(m, 0); out_eid.assign(m, 0); out_dst.assign(m, 0);
        std::vector<uint32_t> ci(in_off.begin(), in_off.end() - 1), co(out_off.begin(), out_off.end() - 1);
        for (uint32_t e = 0; e < m; ++e) { in_eid[ci[e_dst[e]]++] = e; out_dst[co[e_src[e]]] = e_dst[e]; out_eid[co[e_src[e]]++] = e; }
    }
    size_t arcs() const { return e_src.size(); }

    // Topology A (dataflow / spreadsheet / feature store): sparse random DAG with
    // locality window; the first n/8 nodes are pure sources.
    static ArcGraph random_dag(uint32_t n, uint32_t fanin, uint32_t window, uint64_t seed) {
        ArcGraph g; g.n = n; g.act = Act::HardTanh;
        std::mt19937_64 rng(seed);
        const uint32_t sources = std::max<uint32_t>(1, n / 8);
        for (uint32_t v = sources; v < n; ++v) {
            const uint32_t lo = (v > window) ? v - window : 0;
            std::uniform_int_distribution<uint32_t> pick(lo, v - 1);
            for (uint32_t k = 0; k < fanin; ++k) { g.e_src.push_back(pick(rng)); g.e_dst.push_back(v); }
        }
        g.finalize();
        return g;
    }
    // Topology B (cross-domain: sparse neural inference): layered ReLU network,
    // each unit reads `fanin` random units of the previous layer.
    static ArcGraph layered_net(uint32_t layers, uint32_t width, uint32_t fanin, uint64_t seed) {
        ArcGraph g; g.n = layers * width; g.act = Act::ReluClamp;
        std::mt19937_64 rng(seed);
        std::uniform_int_distribution<uint32_t> pick(0, width - 1);
        for (uint32_t l = 1; l < layers; ++l)
            for (uint32_t u = 0; u < width; ++u)
                for (uint32_t k = 0; k < fanin; ++k) {
                    g.e_src.push_back((l - 1) * width + pick(rng));
                    g.e_dst.push_back(l * width + u);
                }
        g.finalize();
        return g;
    }
};

// Exact traits: accumulator in ℤ (scale 2^-(kValFrac+kWFrac)); values in ℤ (scale 2^-kValFrac).
// Every node value satisfies |v| <= kClampQ = 2^35 (the activation saturates), on
// every path, at every depth. Hence (see ERA_FORMAL_MODEL.md, Theorem 5):
//  * A = __int128 is unconditionally overflow-free: |acc| < 2^79 + 2^32·2^31·2^35 < 2^99.
//  * A = int64_t is overflow-free iff the engine admits only
//      |input_q| <= 2^35, |w_q| <= 2^26 per arc, and per node Σ|w_q| <= 2^27 - 2^16,
//    which bounds every stored accumulator by 2^51 + (2^27 - 2^16)·2^35 = 2^62 and every
//    incremental delta by 2^62. Inadmissible ops are rejected (EraKernel then promotes to i128).
template <class A>
struct ExactZ {
    using Acc = A; using Val = int64_t; using W = int32_t;
    static constexpr bool kBounded = sizeof(A) == 8;
    static constexpr int64_t kMaxInputQ = kClampQ;                          // |input| <= 8.0
    static constexpr int32_t kMaxWeightQ = int32_t{1} << 26;                // |w| <= 1024.0
    static constexpr int64_t kMaxWeightSumQ = (int64_t{1} << 27) - (int64_t{1} << 16);  // Σ|w| <= ~2047.0
    static Acc lift_input(int64_t q) { return static_cast<Acc>(q) * (Acc{1} << kWFrac); }
    static W weight(int32_t q) { return q; }
    static Acc term(W w, Val v) { return static_cast<Acc>(w) * static_cast<Acc>(v); }
    static Val apply(const ArcGraph& g, Acc acc) {
        // requantize 2^-48 -> 2^-frac_bits with round-half-to-even, saturate, re-express in Q32
        const int drop = kValFrac - g.frac_bits, sh = kWFrac + drop;
        Acc q = acc >> sh;
        const Acc r = acc - q * (Acc{1} << sh), half = Acc{1} << (sh - 1);
        if (r > half || (r == half && (q & 1))) ++q;
        const Acc hi = static_cast<Acc>(kClampQ) >> drop;
        const Acc lo = (g.act == Act::ReluClamp) ? 0 : -hi;
        if (q < lo) q = lo;
        if (q > hi) q = hi;
        return static_cast<Val>(q) << drop;
    }
    static bool admits_input(int64_t q) { return !kBounded || (q >= -kMaxInputQ && q <= kMaxInputQ); }
    static bool admits_weight(int32_t q) { return !kBounded || (q >= -kMaxWeightQ && q <= kMaxWeightQ); }
};
using ExactTraits = ExactZ<i128>;
using Exact64Traits = ExactZ<int64_t>;

// IEEE-754 binary64 baseline traits: identical quantized inputs, identical activation.
struct DoubleTraits {
    using Acc = double; using Val = double; using W = double;
    static constexpr const char* name = "fp64";
    static Acc lift_input(int64_t q) { return static_cast<double>(q) * 0x1p-32; }
    static W weight(int32_t q) { return static_cast<double>(q) * 0x1p-16; }
    static Acc term(W w, Val v) { return w * v; }
    static Val apply(const ArcGraph& g, Acc acc) {
        if (g.frac_bits < kValFrac) {  // same grid, round-half-to-even (default FP rounding mode)
            const double s = std::ldexp(1.0, g.frac_bits);
            acc = std::nearbyint(acc * s) / s;
        }
        const double lo = (g.act == Act::ReluClamp) ? 0.0 : -8.0;
        return std::min(8.0, std::max(lo, acc));
    }
    static constexpr bool kBounded = false;
    static constexpr int64_t kMaxWeightSumQ = INT64_MAX;
    static bool admits_input(int64_t) { return true; }
    static bool admits_weight(int32_t) { return true; }
};

// Bounded undo journal (Rule 3): fixed-capacity ring; the oldest record is
// overwritten when full, so undo depth == capacity, memory never grows.
struct UndoRecord { uint8_t kind; uint32_t id; int64_t old_q; };
class UndoRing {
public:
    explicit UndoRing(size_t cap) : buf_(std::max<size_t>(1, cap)) {}
    void push(const UndoRecord& r) {
        buf_[head_] = r; head_ = (head_ + 1) % buf_.size();
        if (size_ < buf_.size()) ++size_; else ++dropped_;
    }
    bool pop(UndoRecord& r) {
        if (size_ == 0) return false;
        head_ = (head_ + buf_.size() - 1) % buf_.size(); r = buf_[head_]; --size_; return true;
    }
    size_t size() const { return size_; }
    size_t dropped() const { return dropped_; }
    size_t bytes() const { return buf_.size() * sizeof(UndoRecord); }
private:
    std::vector<UndoRecord> buf_;
    size_t head_ = 0, size_ = 0, dropped_ = 0;
};

template <class T>
class ArcEngine {
public:
    using Acc = typename T::Acc; using Val = typename T::Val; using W = typename T::W;
    enum : uint8_t { kInput = 0, kWeight = 1 };

    ArcEngine(const ArcGraph& g, size_t undo_cap) : g_(&g), undo_(undo_cap) {
        in_q_.assign(g.n, 0); w_q_.assign(g.arcs(), 0); w_.assign(g.arcs(), W{});
        acc_.assign(g.n, Acc{}); val_.assign(g.n, Val{}); dirty_.assign((g.n + 63) / 64, 0);
        wsum_.assign(T::kBounded ? g.n : 0, 0);
    }
    // false when a bounded accumulator (int64) cannot guarantee exactness for this model
    bool admitted() const { return admitted_; }
    // Load quantized inputs/biases and weights, then one full evaluation.
    void load(const std::vector<int64_t>& in_q, const std::vector<int32_t>& w_q) {
        in_q_ = in_q; w_q_ = w_q;
        for (int64_t q : in_q_) admitted_ = admitted_ && T::admits_input(q);
        for (int32_t q : w_q_) admitted_ = admitted_ && T::admits_weight(q);
        if constexpr (T::kBounded) {
            std::fill(wsum_.begin(), wsum_.end(), 0);
            for (size_t e = 0; e < w_q_.size(); ++e) wsum_[g_->e_dst[e]] += std::abs(static_cast<int64_t>(w_q_[e]));
            for (int64_t s : wsum_) admitted_ = admitted_ && s <= T::kMaxWeightSumQ;
        }
        for (size_t e = 0; e < w_q_.size(); ++e) w_[e] = T::weight(w_q_[e]);
        if (sweep_density_ > 0 && sketch_.empty()) build_sketch();  // model-load cost: O(arcs·K), 4K bytes/node
        full_recompute();
    }
    // Baseline path: evaluate every node from scratch in topological order.
    void full_recompute() { sweep_from(0); }
    // Re-evaluate nodes [start, n) from scratch. Nodes < start must already be
    // final. In ℤ the rebuilt accumulators equal the incrementally maintained
    // ones exactly, so switching paths mid-propagation preserves I1..I3.
    uint64_t sweep_from(uint32_t start) {  // returns the number of values that changed
        const ArcGraph& g = *g_;
        uint64_t changed = 0;
        for (uint32_t v = start; v < g.n; ++v) {
            Acc a = T::lift_input(in_q_[v]);
            for (uint32_t k = g.in_off[v]; k < g.in_off[v + 1]; ++k) {
                const uint32_t e = g.in_eid[k];
                if (w_q_[e] != 0) a += T::term(w_[e], val_[g.e_src[e]]);
            }
            const Val nv = T::apply(g, a);
            changed += !(nv == val_[v]);
            acc_[v] = a; val_[v] = nv;
        }
        return changed;
    }
    // Adaptive crossover (cost model): before propagating, estimate the static
    // reachability cone C of the dirty set with a K-min-values sketch and keep an
    // EWMA of the observed change rate (changed / visited). If changes are not
    // being pruned (rate >= 0.8) and C >= theta * (n - v0), per-node propagation
    // would visit most of the remainder at ~2-3x the per-node cost of a sequential
    // sweep, so the remainder is swept instead. 0 = never switch. Results are
    // bit-identical either way (Theorem 1).
    void set_sweep_density(double theta) { sweep_density_ = theta; }
    double change_rate_estimate() const { return change_ewma_; }
    // Δ⁺/Δ⁻ on a node input (source value or bias). Deferred until propagate().
    // Returns false (state untouched) if the value is outside the admitted range.
    bool set_input(uint32_t v, int64_t q, bool journal = true) {
        if (!T::admits_input(q)) return false;
        if (journal) undo_.push({kInput, v, in_q_[v]});
        acc_[v] += T::lift_input(q) - T::lift_input(in_q_[v]);
        in_q_[v] = q; enqueue(v);
        return true;
    }
    // Δ⁺ arc insertion (0 → w), Δ⁻ arc deletion (w → 0) or re-weighting.
    bool set_weight(uint32_t e, int32_t q, bool journal = true) {
        if (!T::admits_weight(q)) return false;
        const uint32_t d = g_->e_dst[e];
        if constexpr (T::kBounded) {  // per-node Σ|w| admission (Theorem 5)
            const int64_t ns = wsum_[d] - std::abs(static_cast<int64_t>(w_q_[e])) + std::abs(static_cast<int64_t>(q));
            if (ns > T::kMaxWeightSumQ) return false;
            wsum_[d] = ns;
        }
        if (journal) undo_.push({kWeight, e, w_q_[e]});
        const W nw = T::weight(q);
        acc_[d] += T::term(nw, val_[g_->e_src[e]]) - (w_q_[e] != 0 ? T::term(w_[e], val_[g_->e_src[e]]) : Acc{});
        w_q_[e] = q; w_[e] = nw; enqueue(d);
        return true;
    }
    // Topological change propagation with dynamic value cut-off (I4). Because
    // node ids are a topological order, the frontier is a dirty bitset scanned
    // upward from the lowest dirty word: O(touched + span/64), no heap. With
    // cutoff=false it degrades to classic dirty-marking of all descendants
    // (static cone) — used as the ablation.
    uint64_t propagate(bool cutoff = true) {
        const ArcGraph& g = *g_;
        uint64_t touched = 0, changed_n = 0;
        if (cutoff && sweep_density_ > 0 && change_ewma_ >= 0.8 && lo_word_ <= hi_word_) {
            size_t w0 = lo_word_; while (w0 <= hi_word_ && dirty_[w0] == 0) ++w0;
            if (w0 <= hi_word_) {
                const uint32_t v0 = static_cast<uint32_t>(w0 * 64 + std::countr_zero(dirty_[w0]));
                const double cone = estimate_dirty_cone();
                if (cone >= sweep_density_ * static_cast<double>(g.n - v0)) {
                    std::fill(dirty_.begin() + static_cast<std::ptrdiff_t>(w0), dirty_.end(), uint64_t{0});
                    lo_word_ = SIZE_MAX; hi_word_ = 0;
                    const uint64_t ch = sweep_from(v0);
                    change_ewma_ = 0.5 * change_ewma_ + 0.5 * std::min(1.0, static_cast<double>(ch) / std::max(1.0, cone));
                    last_changed_ = ch;
                    return g.n - v0;
                }
            }
        }
        for (size_t wi = lo_word_; wi <= hi_word_ && lo_word_ <= hi_word_; ++wi) {
            uint64_t bits;
            while ((bits = dirty_[wi]) != 0) {
                const uint32_t v = static_cast<uint32_t>(wi * 64 + std::countr_zero(bits));
                dirty_[wi] = bits & (bits - 1); ++touched;
                const Val nv = T::apply(g, acc_[v]);
                const bool changed = !(nv == val_[v]);
                if (!changed && cutoff) continue;
                changed_n += changed;
                const Val old = val_[v]; val_[v] = nv;
                for (uint32_t k = g.out_off[v]; k < g.out_off[v + 1]; ++k) {
                    const uint32_t e = g.out_eid[k], d = g.out_dst[k];
                    if (changed && w_q_[e] != 0) acc_[d] += T::term(w_[e], nv) - T::term(w_[e], old);
                    enqueue(d);
                }
            }
        }
        lo_word_ = SIZE_MAX; hi_word_ = 0;
        last_changed_ = changed_n;
        if (cutoff && touched > 0)
            change_ewma_ = 0.5 * change_ewma_ + 0.5 * static_cast<double>(changed_n) / static_cast<double>(touched);
        return touched;
    }
    // ↶ : revert the most recent journaled op through the same incremental path.
    // LIFO undo only revisits earlier admitted states, so a bounded engine never
    // rejects it; if it ever did, the record is restored and false is returned.
    bool undo_one() {
        UndoRecord r{};
        if (!undo_.pop(r)) return false;
        const bool ok = (r.kind == kInput) ? set_input(r.id, r.old_q, false)
                                           : set_weight(r.id, static_cast<int32_t>(r.old_q), false);
        if (!ok) undo_.push(r);
        return ok;
    }

    const std::vector<Val>& values() const { return val_; }
    const std::vector<Acc>& accumulators() const { return acc_; }
    int64_t input_q(uint32_t v) const { return in_q_[v]; }
    int32_t weight_q(uint32_t e) const { return w_q_[e]; }
    const UndoRing& journal() const { return undo_; }
    uint64_t last_changed() const { return last_changed_; }  // values changed by the last propagate
    const std::vector<int64_t>& inputs() const { return in_q_; }
    const std::vector<int32_t>& weights() const { return w_q_; }
    void adopt_journal(const UndoRing& j) { undo_ = j; }
    size_t state_bytes() const {
        return in_q_.size() * 8 + w_q_.size() * 4 + w_.size() * sizeof(W) + acc_.size() * sizeof(Acc) +
               val_.size() * sizeof(Val) + dirty_.size() * 8 + wsum_.size() * 8 + sketch_.size() * 4 + undo_.bytes();
    }

private:
    static uint32_t node_hash(uint32_t v) {  // splitmix64 finalizer, top 32 bits
        uint64_t z = static_cast<uint64_t>(v) + 0x9E3779B97F4A7C15ull;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull; z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return static_cast<uint32_t>((z ^ (z >> 31)) >> 32);
    }
    // Merge K-min list b into a (both ascending, UINT32_MAX = empty slot).
    static void kmin_merge(uint32_t* a, const uint32_t* b) {
        uint32_t out[kSketch]; int i = 0, j = 0, k = 0;
        while (k < kSketch) {
            const uint32_t x = (i < kSketch) ? a[i] : UINT32_MAX, y = (j < kSketch) ? b[j] : UINT32_MAX;
            const uint32_t m = std::min(x, y);
            if (m == UINT32_MAX) { while (k < kSketch) out[k++] = UINT32_MAX; break; }
            out[k++] = m;
            i += (x == m); j += (y == m);
        }
        std::copy(out, out + kSketch, a);
    }
    // Reachability sketch S(v) = K smallest hashes of {v} ∪ descendants(v), O(arcs·K), built once.
    void build_sketch() {
        const ArcGraph& g = *g_;
        sketch_.assign(static_cast<size_t>(g.n) * kSketch, UINT32_MAX);
        for (uint32_t v = g.n; v-- > 0;) {
            uint32_t* s = &sketch_[static_cast<size_t>(v) * kSketch];
            s[0] = node_hash(v);
            for (uint32_t k = g.out_off[v]; k < g.out_off[v + 1]; ++k)
                kmin_merge(s, &sketch_[static_cast<size_t>(g.out_dst[k]) * kSketch]);
        }
    }
    double estimate_dirty_cone() {
        if (sketch_.empty()) build_sketch();
        uint32_t acc[kSketch]; std::fill(acc, acc + kSketch, UINT32_MAX);
        for (size_t wi = lo_word_; wi <= hi_word_; ++wi)
            for (uint64_t bits = dirty_[wi]; bits; bits &= bits - 1)
                kmin_merge(acc, &sketch_[(wi * 64 + static_cast<size_t>(std::countr_zero(bits))) * kSketch]);
        int cnt = 0; while (cnt < kSketch && acc[cnt] != UINT32_MAX) ++cnt;
        if (cnt < kSketch) return cnt;  // fewer than K distinct nodes: exact
        return (kSketch - 1) * 4294967296.0 / (static_cast<double>(acc[kSketch - 1]) + 1.0);
    }
    void enqueue(uint32_t v) {
        const size_t wi = v >> 6;
        const uint64_t bit = uint64_t{1} << (v & 63);
        dirty_[wi] |= bit;
        lo_word_ = std::min(lo_word_, wi); hi_word_ = std::max(hi_word_, wi);
    }
    const ArcGraph* g_;
    std::vector<int64_t> in_q_;
    std::vector<int32_t> w_q_;
    std::vector<W> w_;
    std::vector<Acc> acc_;
    std::vector<Val> val_;
    std::vector<uint64_t> dirty_;
    size_t lo_word_ = SIZE_MAX, hi_word_ = 0;
    bool admitted_ = true;
    std::vector<int64_t> wsum_;  // per-node Σ|w_q| (bounded accumulators only)
    double sweep_density_ = 0.4;  // break-even: per-node propagation costs ~2.5x a sweep step per node
    double change_ewma_ = 0.5;  // EWMA of changed / visited (neutral start)
    uint64_t last_changed_ = 0;
    static constexpr int kSketch = 8;
    std::vector<uint32_t> sketch_;  // K-min-values reachability sketch, n * kSketch, built lazily
    UndoRing undo_;
};

// Count positions where two value vectors differ (bitwise for ℤ, exact == for fp64).
template <class V>
inline size_t count_mismatch(const std::vector<V>& a, const std::vector<V>& b) {
    size_t c = 0;
    for (size_t i = 0; i < a.size(); ++i) c += !(a[i] == b[i]);
    return c;
}

} // namespace cycraft::omega::quantum_syntax::era
