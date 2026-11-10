// SINGULAR Ω v24 causal_machine — reverse-mode automatic differentiation over Ω-IR.
// Loss = sum of the elements written to `target_buf`. Returns d loss / d input-buffer
// elements and d loss / d Const nodes. Loads that follow a Store to the same buffer are
// differentiated through the stored value (store-to-load forwarding), so memory effects
// are respected. Max routes the adjoint to the larger operand (ties go to the left operand).
#pragma once
#include <map>

#include "../compiler/omega_ir.hpp"

namespace omega_ad {

struct Gradients {
    double loss = 0;
    std::vector<std::vector<double>> dbuf;  // per external buffer
    std::map<int, double> dconst;            // Const node id -> derivative
};

inline Gradients reverse(const omega_ir::Program& p, const std::vector<std::vector<double>>& inputs, int target_buf) {
    using omega_ir::Op;
    const size_t N = p.nodes.size(), n = p.n;
    std::vector<std::vector<double>> val(N);
    std::vector<int> alias(N, -1);          // Load -> forwarded stored value node
    std::vector<int> last_store(p.n_bufs, -1);
    std::vector<std::vector<double>> mem = inputs;
    auto get = [&](int id, size_t e) { return p.nodes[id].op == Op::Const ? static_cast<double>(p.nodes[id].c) : val[id][e]; };
    for (size_t i = 0; i < N; ++i) {  // forward
        const auto& x = p.nodes[i];
        if (x.op == Op::Const) continue;
        if (x.op == Op::Store) { mem[x.buf] = val[x.a]; last_store[x.buf] = x.a; continue; }
        val[i].assign(n, 0.0);
        if (x.op == Op::Load) { alias[i] = last_store[x.buf]; val[i] = mem[x.buf]; continue; }
        for (size_t e = 0; e < n; ++e) {
            if (x.op == Op::Shift) val[i][e] = e ? val[x.a][e - 1] : 0.0;
            else val[i][e] = omega_ir::apply<double>(x.op, get(x.a, e), get(x.b, e));
        }
    }
    Gradients g;
    g.dbuf.assign(p.n_bufs, std::vector<double>(n, 0.0));
    std::vector<std::vector<double>> adj(N, std::vector<double>(n, 0.0));
    int final_store = -1;
    for (size_t i = 0; i < N; ++i)
        if (p.nodes[i].op == Op::Store && p.nodes[i].buf == target_buf) final_store = static_cast<int>(i);
    if (final_store < 0) throw std::runtime_error("autodiff: target buffer is never stored");
    for (size_t e = 0; e < n; ++e) {
        adj[p.nodes[final_store].a][e] += 1.0;
        g.loss += val[p.nodes[final_store].a][e];
    }
    for (size_t ii = N; ii-- > 0;) {  // backward
        const auto& x = p.nodes[ii];
        auto& d = adj[ii];
        switch (x.op) {
        case Op::Store: case Op::Const: break;
        case Op::Load:
            for (size_t e = 0; e < n; ++e) {
                if (alias[ii] >= 0) adj[alias[ii]][e] += d[e];
                else g.dbuf[x.buf][e] += d[e];
            }
            break;
        case Op::Shift:
            for (size_t e = 1; e < n; ++e) adj[x.a][e - 1] += d[e];
            break;
        default:
            for (size_t e = 0; e < n; ++e) {
                double va = get(x.a, e), vb = get(x.b, e), da = 0, db = 0;
                switch (x.op) {
                case Op::Add: da = d[e]; db = d[e]; break;
                case Op::Sub: da = d[e]; db = -d[e]; break;
                case Op::Mul: da = d[e] * vb; db = d[e] * va; break;
                default: (va >= vb ? da : db) = d[e]; break;
                }
                if (p.nodes[x.a].op == Op::Const) g.dconst[x.a] += da; else adj[x.a][e] += da;
                if (p.nodes[x.b].op == Op::Const) g.dconst[x.b] += db; else adj[x.b][e] += db;
            }
        }
    }
    return g;
}

// Loss evaluated with the double-precision reference interpreter (for finite differences).
inline double loss_of(const omega_ir::Program& p, std::vector<std::vector<double>> bufs, int target_buf) {
    omega_ir::interpret<double>(p, bufs);
    double s = 0;
    for (double v : bufs[target_buf]) s += v;
    return s;
}

} // namespace omega_ad
