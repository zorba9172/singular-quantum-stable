// SINGULAR Ω v24 causal_machine — Ω-IR: typed, effect-aware elementwise IR with
// verified operation fusion.
//  * Values are SSA nodes; every vector value has the program's element count, Const is a
//    broadcast scalar (type system: Scalar | Vector).
//  * Effects: Load(buf) reads, Store(buf) writes, Shift reads a[i-1] (a neighbour access),
//    all other ops are pure.
//  * A plan partitions the node list into contiguous groups executed as one strip-mined kernel.
//    Legality (checked, never assumed): a Shift operand must be materialized by an earlier group
//    (a neighbour read across a strip boundary needs the full array).
//  * Numerics: fusion never reorders or reassociates per-element operations, and the executor
//    is compiled with -ffp-contract=off, so fused and unfused plans must agree bit-for-bit.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace omega_ir {

enum class Op : uint8_t { Load, Const, Add, Sub, Mul, Max, Shift, Store };
enum class Type : uint8_t { Scalar, Vector, Effect };

struct Node {
    Op op;
    int a = -1, b = -1;  // operand node ids
    int buf = -1;        // Load/Store buffer id
    float c = 0.f;       // Const value
};

struct Program {
    size_t n = 0;     // elements per vector
    int n_bufs = 0;   // external buffers (inputs and outputs)
    std::vector<Node> nodes;
    int add(Node x) { nodes.push_back(x); return static_cast<int>(nodes.size()) - 1; }
};

inline Type type_of(const Program& p, int id) {
    switch (p.nodes[id].op) {
    case Op::Const: return Type::Scalar;
    case Op::Store: return Type::Effect;
    default: return Type::Vector;
    }
}

inline bool binary(Op o) { return o == Op::Add || o == Op::Sub || o == Op::Mul || o == Op::Max; }

// Type and SSA verifier. Throws on the first violation.
inline void verify(const Program& p) {
    for (size_t i = 0; i < p.nodes.size(); ++i) {
        const Node& x = p.nodes[i];
        auto operand = [&](int id) {
            if (id < 0 || static_cast<size_t>(id) >= i) throw std::runtime_error("Ω-IR: operand is not a dominating SSA value at node " + std::to_string(i));
            if (type_of(p, id) == Type::Effect) throw std::runtime_error("Ω-IR: effect used as value at node " + std::to_string(i));
        };
        switch (x.op) {
        case Op::Load:
        case Op::Store:
            if (x.buf < 0 || x.buf >= p.n_bufs) throw std::runtime_error("Ω-IR: bad buffer id at node " + std::to_string(i));
            if (x.op == Op::Store) { operand(x.a); if (type_of(p, x.a) != Type::Vector) throw std::runtime_error("Ω-IR: store of scalar"); }
            break;
        case Op::Const: break;
        case Op::Shift:
            operand(x.a);
            if (type_of(p, x.a) != Type::Vector) throw std::runtime_error("Ω-IR: shift of scalar");
            break;
        default:
            operand(x.a); operand(x.b);
            if (type_of(p, x.a) == Type::Scalar && type_of(p, x.b) == Type::Scalar) throw std::runtime_error("Ω-IR: scalar-only arithmetic must be constant folded");
        }
    }
}

template <class T>
inline T apply(Op o, T x, T y) {
    switch (o) {
    case Op::Add: return x + y;
    case Op::Sub: return x - y;
    case Op::Mul: return x * y;
    default: return x > y ? x : y;  // Max
    }
}

// Reference interpreter: whole-array, one node at a time, in element type T.
template <class T>
inline void interpret(const Program& p, std::vector<std::vector<T>>& bufs) {
    std::vector<std::vector<T>> val(p.nodes.size());
    for (size_t i = 0; i < p.nodes.size(); ++i) {
        const Node& x = p.nodes[i];
        auto get = [&](int id, size_t e) { return p.nodes[id].op == Op::Const ? static_cast<T>(p.nodes[id].c) : val[id][e]; };
        if (x.op == Op::Const) continue;
        if (x.op == Op::Store) { bufs[x.buf] = val[x.a]; continue; }
        val[i].resize(p.n);
        for (size_t e = 0; e < p.n; ++e) {
            if (x.op == Op::Load) val[i][e] = bufs[x.buf][e];
            else if (x.op == Op::Shift) val[i][e] = e ? val[x.a][e - 1] : T(0);
            else val[i][e] = apply<T>(x.op, get(x.a, e), get(x.b, e));
        }
    }
}

using Plan = std::vector<int>;  // group id per node, non-decreasing, contiguous groups

inline Plan plan_unfused(const Program& p) { Plan g(p.nodes.size()); for (size_t i = 0; i < g.size(); ++i) g[i] = static_cast<int>(i); return g; }

inline int n_groups(const Plan& g) { return g.empty() ? 0 : g.back() + 1; }

inline std::string legality_error(const Program& p, const Plan& g) {
    if (g.size() != p.nodes.size()) return "plan size mismatch";
    for (size_t i = 0; i < g.size(); ++i) {
        if (i == 0 ? g[i] != 0 : (g[i] != g[i - 1] && g[i] != g[i - 1] + 1)) return "groups must be contiguous and ordered";
        if (p.nodes[i].op == Op::Shift && g[p.nodes[i].a] == g[i])
            return "node " + std::to_string(i) + ": shift operand computed in the same strip-mined group (needs halo)";
    }
    return "";
}

// Fuse greedily as far as legality allows (the classic baseline heuristic).
inline Plan plan_greedy(const Program& p) {
    Plan g(p.nodes.size(), 0);
    for (size_t i = 1; i < g.size(); ++i) {
        g[i] = g[i - 1];
        if (p.nodes[i].op == Op::Shift && g[p.nodes[i].a] == g[i]) g[i] = g[i - 1] + 1;
    }
    return g;
}

// A value is materialized (full array in memory) if it is used by a later group, stored, or shifted.
inline std::vector<char> materialized(const Program& p, const Plan& g) {
    std::vector<char> m(p.nodes.size(), 0);
    for (size_t i = 0; i < p.nodes.size(); ++i) {
        const Node& x = p.nodes[i];
        for (int o : {x.a, x.b})
            if (o >= 0 && p.nodes[o].op != Op::Const && (g[o] != g[i] || x.op == Op::Shift)) m[o] = 1;
    }
    return m;
}

struct Features { double bytes, passes, elem_ops; };

inline Features features(const Program& p, const Plan& g) {
    auto m = materialized(p, g);
    double bytes = 0, ops = 0;
    for (size_t i = 0; i < p.nodes.size(); ++i) {
        const Node& x = p.nodes[i];
        if (x.op == Op::Const) continue;
        if (x.op == Op::Load || x.op == Op::Store) bytes += 4.0 * p.n;
        if (m[i]) bytes += 8.0 * p.n;  // written once, read back at least once
        if (x.op != Op::Store) ops += static_cast<double>(p.n);
    }
    return {bytes, static_cast<double>(n_groups(g)), ops};
}

// Strip-mined executor. Materialized values live in full arrays, group-local values in
// strip scratch (strip of `strip` elements stays in L1).
class Executor {
 public:
    Executor(const Program& p, const Plan& g, size_t strip = 1024) : p_(p), g_(g), strip_(strip) {
        std::string e = legality_error(p, g);
        if (!e.empty()) throw std::runtime_error("Ω-IR: illegal plan: " + e);
        mat_ = materialized(p, g);
        full_.resize(p.nodes.size());
        scratch_.resize(p.nodes.size());
        for (size_t i = 0; i < p.nodes.size(); ++i) {
            if (p.nodes[i].op == Op::Const || p.nodes[i].op == Op::Store || p.nodes[i].op == Op::Load) continue;
            if (mat_[i]) full_[i].assign(p.n, 0.f);
            else scratch_[i].assign(strip, 0.f);
        }
    }

    void run(std::vector<std::vector<float>>& bufs) {
        const size_t N = p_.nodes.size();
        size_t i0 = 0;
        while (i0 < N) {
            size_t i1 = i0;
            while (i1 < N && g_[i1] == g_[i0]) ++i1;
            for (size_t s = 0; s < p_.n; s += strip_) {
                size_t len = std::min(strip_, p_.n - s);
                for (size_t i = i0; i < i1; ++i) eval(i, s, len, bufs);
            }
            i0 = i1;
        }
    }

 private:
    const Program& p_;
    Plan g_;
    size_t strip_;
    std::vector<char> mat_;
    std::vector<std::vector<float>> full_, scratch_;

    // Pointer to elements [s, s+len) of value id (nullptr for Const).
    const float* src(int id, size_t s, std::vector<std::vector<float>>& bufs) const {
        const Node& x = p_.nodes[id];
        if (x.op == Op::Load) return bufs[x.buf].data() + s;
        if (x.op == Op::Const) return nullptr;
        return mat_[id] ? full_[id].data() + s : scratch_[id].data();
    }

    void eval(size_t i, size_t s, size_t len, std::vector<std::vector<float>>& bufs) {
        const Node& x = p_.nodes[i];
        if (x.op == Op::Const || x.op == Op::Load) return;
        if (x.op == Op::Store) {
            const float* a = src(x.a, s, bufs);
            std::memcpy(bufs[x.buf].data() + s, a, len * sizeof(float));
            return;
        }
        float* d = mat_[i] ? full_[i].data() + s : scratch_[i].data();
        if (x.op == Op::Shift) {  // operand is materialized (legality)
            const float* a = full_[x.a].empty() ? bufs[p_.nodes[x.a].buf].data() : full_[x.a].data();
            for (size_t e = 0; e < len; ++e) d[e] = (s + e) ? a[s + e - 1] : 0.f;
            return;
        }
        const float *a = src(x.a, s, bufs), *b = src(x.b, s, bufs);
        const float ca = p_.nodes[x.a].c, cb = p_.nodes[x.b].c;
        switch (x.op) {
        case Op::Add: kernel(d, a, b, ca, cb, len, [](float u, float v) { return u + v; }); break;
        case Op::Sub: kernel(d, a, b, ca, cb, len, [](float u, float v) { return u - v; }); break;
        case Op::Mul: kernel(d, a, b, ca, cb, len, [](float u, float v) { return u * v; }); break;
        default: kernel(d, a, b, ca, cb, len, [](float u, float v) { return u > v ? u : v; }); break;
        }
    }

    template <class F>
    static void kernel(float* __restrict d, const float* a, const float* b, float ca, float cb, size_t len, F f) {
        if (a && b) for (size_t e = 0; e < len; ++e) d[e] = f(a[e], b[e]);
        else if (a) for (size_t e = 0; e < len; ++e) d[e] = f(a[e], cb);
        else for (size_t e = 0; e < len; ++e) d[e] = f(ca, b[e]);
    }
};

} // namespace omega_ir
