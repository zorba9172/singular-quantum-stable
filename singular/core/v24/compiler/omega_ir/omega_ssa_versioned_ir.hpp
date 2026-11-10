#pragma once
// SINGULAR Ω v24 — Typed, Effect- & Version-Aware `Ω-IR` Specification
#include <cstdint>
#include <string>
#include <vector>

namespace singular::v24::compiler {

enum class OmegaIrOp : uint8_t {
    LOAD, MUL, ADD, TANH, STORE,
    OMEGA_TRACK, OMEGA_DIRTY, OMEGA_PROP, OMEGA_VALID, OMEGA_FUSED_CONE
};

struct OmegaIrNode {
    OmegaIrOp op = OmegaIrOp::LOAD;
    uint32_t dst = 0, src1 = 0, src2 = 0;
    uint32_t state_slot = 0, expected_version = 0;
    bool has_side_effect = false;
};

struct OmegaIrModule {
    std::string name;
    std::vector<OmegaIrNode> nodes;
};

} // namespace singular::v24::compiler
