// ============================================================================
// SINGULAR Ω — ERA replication experiment (`DISC-000901`, claim I3)
// Rule `SG-ARCH-400`: <= 400 physical lines.
//
// I3 is NOT a consequence of ℤ being commutative: concurrent set_weight(2) and
// set_weight(3) on two replicas need a merge rule. ERA's replication semantics:
//   * every input and every candidate arc is a last-writer-wins (LWW) register;
//   * each op carries a unique stamp (Lamport clock << 16 | replica id), a total order;
//   * a replica applies an op iff its stamp is greater than the register's stamp
//     (duplicates and stale writes are no-ops ⇒ idempotent, commutative merge = state CRDT);
//   * the candidate arc set is fixed with src < dst, so every subset is a DAG:
//     concurrent arc insert/delete can never create a cycle.
// Theorem 3 (ERA_FORMAL_MODEL.md): once every replica has received the same set
// of ops, the registers are equal (CRDT convergence) and, by Theorem 1, the node
// values are bit-identical. This file exercises that under reordering,
// duplication, partitions and reconnection, and runs fp64 replicas on the very
// same message schedule as a control.
// ============================================================================
#pragma once
#include "cycraft/quantum_syntax/omega_era_kernel.hpp"
#include "cycraft/quantum_syntax/omega_exact_reversible_arc_benchmark.hpp"
#include <deque>

namespace cycraft::omega::quantum_syntax::era {

struct ReplOp { uint8_t kind; uint32_t id; int64_t q; uint64_t stamp; };

struct ReplicationReport {
    std::string name;
    size_t replicas = 0, ops_generated = 0, delivered = 0, duplicates_sent = 0, stale_or_dup_dropped = 0;
    size_t parked_during_partition = 0, partitions = 0, kernel_promotions = 0;
    size_t crashes = 0, lost_while_down = 0, resent_on_recovery = 0;  // node failure + recovery
    size_t register_state_mismatch = 0;  // replicas whose (inputs, weights) differ from replica 0
    size_t exact_value_mismatch = 0;     // max over replicas of nodes differing from replica 0
    size_t oracle_mismatch = 0;          // replica 0 values vs independent oracle on its registers
    size_t fp64_value_mismatch = 0;      // control: same schedule, fp64 engine
    bool passed() const { return register_state_mismatch == 0 && exact_value_mismatch == 0 && oracle_mismatch == 0 && delivered > 0; }
};

class EraReplication {
public:
    struct Replica {
        EraKernel exact;
        ArcEngine<DoubleTraits> fp;
        std::vector<uint64_t> in_stamp, w_stamp;
        uint64_t clock = 0;
        std::vector<ReplOp> seen;  // every op received or originated (anti-entropy source; bounded by total messages)
        Replica(const ArcGraph& g) : exact(g, 1), fp(g, 1), in_stamp(g.n, 0), w_stamp(g.arcs(), 0) {}
    };

    // Apply iff newer (LWW). Returns false for stale or duplicate deliveries.
    static bool receive(Replica& r, const ReplOp& op) {
        r.clock = std::max(r.clock, op.stamp >> 16);
        r.seen.push_back(op);
        uint64_t& st = (op.kind == 0) ? r.in_stamp[op.id] : r.w_stamp[op.id];
        if (op.stamp <= st) return false;
        st = op.stamp;
        if (op.kind == 0) { r.exact.set_input(op.id, op.q, false); r.fp.set_input(op.id, op.q, false); }
        else { r.exact.set_weight(op.id, static_cast<int32_t>(op.q), false); r.fp.set_weight(op.id, static_cast<int32_t>(op.q), false); }
        r.exact.propagate(); r.fp.propagate();
        return true;
    }

    static ReplicationReport run(const Workload& w, size_t n_rep, uint64_t seed, bool crash = false) {
        ReplicationReport rep;
        rep.name = w.name; rep.replicas = n_rep;
        std::vector<Replica> rs;
        rs.reserve(n_rep);
        for (size_t i = 0; i < n_rep; ++i) { rs.emplace_back(w.g); rs.back().exact.load(w.in_q, w.w_q); rs.back().fp.load(w.in_q, w.w_q); }
        std::vector<std::deque<ReplOp>> inbox(n_rep);
        std::vector<std::pair<size_t, ReplOp>> parked;
        std::mt19937_64 rng(seed);
        std::uniform_real_distribution<double> coin(0.0, 1.0);
        std::uniform_int_distribution<size_t> pick_rep(0, n_rep - 1);
        // Hot registers force genuinely concurrent conflicting writes.
        std::vector<Op> hot;
        for (size_t i = 0; i < std::min<size_t>(16, w.ops.size()); ++i) hot.push_back(w.ops[i]);
        const size_t m = w.ops.size();
        const size_t part_begin = m / 4, part_end = m / 2;  // one partition window
        const size_t isolated = (n_rep >= 3) ? n_rep - 1 : n_rep;  // replica id cut off
        rep.partitions = (n_rep >= 3) ? 1 : 0;
        auto link_down = [&](size_t step, size_t a, size_t b) {
            return rep.partitions && step >= part_begin && step < part_end && ((a == isolated) != (b == isolated));
        };
        auto send = [&](size_t step, size_t from, const ReplOp& op) {
            for (size_t to = 0; to < n_rep; ++to) {
                if (to == from) continue;
                const int copies = coin(rng) < 0.15 ? 2 : 1;  // duplication
                rep.duplicates_sent += copies - 1;
                for (int c = 0; c < copies; ++c) {
                    if (link_down(step, from, to)) { parked.push_back({to, op}); ++rep.parked_during_partition; }
                    else inbox[to].push_back(op);
                }
            }
        };
        auto deliver_one = [&](size_t to) {
            if (inbox[to].empty()) return;
            std::uniform_int_distribution<size_t> pi(0, inbox[to].size() - 1);  // reordering
            const size_t i = pi(rng);
            const ReplOp op = inbox[to][i];
            inbox[to].erase(inbox[to].begin() + static_cast<std::ptrdiff_t>(i));
            ++rep.delivered;
            if (!receive(rs[to], op)) ++rep.stale_or_dup_dropped;
        };
        // Node failure: replica `victim` checkpoints at m/8, crashes at 3m/8 (state and
        // in-flight messages lost, deliveries to it dropped), recovers at 5m/8 from its
        // checkpoint, then anti-entropy: every peer resends every op it has seen.
        const size_t victim = (n_rep >= 3) ? 1 : 0;
        const size_t ck_step = m / 8, crash_step = 3 * m / 8, rec_step = 5 * m / 8;
        bool down = false;
        struct Snap { std::vector<int64_t> in; std::vector<int32_t> w; std::vector<uint64_t> is, ws; uint64_t clock = 0; std::vector<ReplOp> seen; } snap;
        rep.crashes = crash ? 1 : 0;
        std::uniform_real_distribution<double> uval(-1.0, 1.0);
        for (size_t step = 0; step < m; ++step) {
            if (step == part_end) {  // reconnection: parked messages flow again
                for (auto& [to, op] : parked) inbox[to].push_back(op);
                parked.clear();
            }
            if (crash && step == ck_step) {
                const Replica& v = rs[victim];
                snap = {v.exact.inputs(), v.exact.weights(), v.in_stamp, v.w_stamp, v.clock, v.seen};
            }
            if (crash && step == crash_step) { down = true; rep.lost_while_down += inbox[victim].size(); inbox[victim].clear(); }
            if (crash && step == rec_step) {
                down = false;
                rep.lost_while_down += inbox[victim].size(); inbox[victim].clear();  // sent while down: never received
                Replica& v = rs[victim];
                v.exact.load(snap.in, snap.w); v.fp.load(snap.in, snap.w);
                v.in_stamp = snap.is; v.w_stamp = snap.ws; v.clock = snap.clock; v.seen = snap.seen;
                for (size_t p = 0; p < n_rep; ++p)
                    if (p != victim) for (const ReplOp& op : rs[p].seen) { inbox[victim].push_back(op); ++rep.resent_on_recovery; }
            }
            size_t from = pick_rep(rng);
            if (down && from == victim) from = (victim + 1) % n_rep;
            Op o = w.ops[step];
            if (coin(rng) < 0.3) {  // write a hot register with a fresh value
                o = hot[step % hot.size()];
                o.q = (o.kind == 0) ? std::llround(uval(rng) * 0x1p32) : std::lround(uval(rng) * 0.6 * 0x1p16);
            }
            Replica& r = rs[from];
            const ReplOp op{o.kind, o.id, o.q, ((++r.clock) << 16) | from};
            receive(r, op);
            send(step, from, op);
            ++rep.ops_generated;
            const int burst = static_cast<int>(coin(rng) * 2.0 * static_cast<double>(n_rep));
            for (int b = 0; b < burst; ++b) {
                const size_t to = pick_rep(rng);
                if (down && to == victim) { if (!inbox[to].empty()) { inbox[to].pop_front(); ++rep.lost_while_down; } continue; }
                deliver_one(to);
            }
        }
        for (auto& [to, op] : parked) inbox[to].push_back(op);
        parked.clear();
        for (bool any = true; any;) {  // drain everything (bounded: finite message count)
            any = false;
            for (size_t to = 0; to < n_rep; ++to) if (!inbox[to].empty()) { deliver_one(to); any = true; }
        }
        for (size_t i = 0; i < n_rep; ++i) {
            rep.kernel_promotions += rs[i].exact.promotions();
            if (rs[i].exact.inputs() != rs[0].exact.inputs() || rs[i].exact.weights() != rs[0].exact.weights()) ++rep.register_state_mismatch;
            rep.exact_value_mismatch = std::max(rep.exact_value_mismatch, count_mismatch(rs[i].exact.values(), rs[0].exact.values()));
            rep.fp64_value_mismatch = std::max(rep.fp64_value_mismatch, count_mismatch(rs[i].fp.values(), rs[0].fp.values()));
        }
        const auto oracle = EraOracle::evaluate(w.g, rs[0].exact.inputs(), rs[0].exact.weights());
        rep.oracle_mismatch = oracle.empty() ? w.g.n : count_mismatch(rs[0].exact.values(), oracle);
        return rep;
    }
};

} // namespace cycraft::omega::quantum_syntax::era
