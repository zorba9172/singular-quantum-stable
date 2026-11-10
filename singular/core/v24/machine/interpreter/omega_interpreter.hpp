#pragma once
// SINGULAR Ω v24 — Reference Machine Interpreter (`ArchitecturalStateTable` + RV64 GPRs)
#include "../isa/semantics/omega_isa_semantics.hpp"
#include <array>

namespace singular::v24::machine {

struct ReferenceMachineInterpreter {
    std::array<uint64_t, 32> x{};
    singular::v24::isa::ArchitecturalStateTable state_table{};

    bool step_word(uint32_t word) noexcept {
        using namespace singular::v24::isa;
        auto d = decode_omega_r(word);
        if (!d.valid) return false;
        uint64_t v1 = x[d.rs1], v2 = x[d.rs2], res = 0;
        switch (d.funct3) {
            case OmegaFunct3::TRACK: res = state_table.exec_track(v1, v2); break;
            case OmegaFunct3::DIRTY: res = state_table.exec_dirty(v1, v2); break;
            case OmegaFunct3::PROP:  res = state_table.exec_prop(v1);      break;
            case OmegaFunct3::VALID: res = state_table.exec_valid(v1, v2); break;
        }
        if (d.rd != 0) x[d.rd] = res;
        return true;
    }
};

} // namespace singular::v24::machine
