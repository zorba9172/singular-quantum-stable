#pragma once
// SINGULAR Ω v24 — Native Reverse-Mode Autograd & Exact TwoSum Gradient Accumulator
#include <cmath>
#include <cstddef>
#include <vector>

namespace singular::v24::cognitive {

struct AutogradTapeNode {
    double val = 0.0, grad = 0.0, err_comp = 0.0;
    int lhs = -1, rhs = -1;
    double w_lhs = 0.0, w_rhs = 0.0;
};

class NativeAutogradEngine {
public:
    std::vector<AutogradTapeNode> tape;

    int var(double v) {
        tape.push_back({v, 0.0, 0.0, -1, -1, 0.0, 0.0});
        return static_cast<int>(tape.size() - 1);
    }

    int mul_add_tanh(int x, int w, int b) {
        double z = tape[x].val * tape[w].val + tape[b].val;
        double y = std::tanh(z);
        double dt = 1.0 - y * y;
        tape.push_back({y, 0.0, 0.0, x, w, dt * tape[w].val, dt * tape[x].val});
        return static_cast<int>(tape.size() - 1);
    }

    static void twosum_acc(double& s, double& e, double d) noexcept {
        volatile double sum = s + d;
        volatile double v   = sum - s;
        e += (s - (sum - v)) + (d - v);
        s = sum;
    }

    void backward(int root_idx) noexcept {
        if (root_idx < 0 || static_cast<size_t>(root_idx) >= tape.size()) return;
        tape[root_idx].grad = 1.0;
        for (int i = root_idx; i >= 0; --i) {
            const auto& n = tape[i];
            double g = n.grad + n.err_comp;
            if (n.lhs >= 0) twosum_acc(tape[n.lhs].grad, tape[n.lhs].err_comp, g * n.w_lhs);
            if (n.rhs >= 0) twosum_acc(tape[n.rhs].grad, tape[n.rhs].err_comp, g * n.w_rhs);
        }
    }
};

} // namespace singular::v24::cognitive
