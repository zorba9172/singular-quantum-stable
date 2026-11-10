#pragma once
// SINGULAR Ω v24 — Native Codegen Bridge (`singular-as`, `singular-riscv64-tcc`, `AsmJit`)
#include "../../machine/isa/encoding/omega_isa_encoding.hpp"
#include <vector>

namespace singular::v24::compiler {

inline std::vector<uint32_t> encode_canonical_causal_update_rv64() noexcept {
    using namespace singular::v24::isa;
    return {
        encode_omega_r(OmegaFunct3::TRACK, 10, 11, 12), // 0x02c5857b
        encode_omega_r(OmegaFunct3::DIRTY, 10, 11, 12), // 0x02c5957b
        encode_omega_r(OmegaFunct3::PROP,  10, 11, 12), // 0x02c5a57b
        encode_omega_r(OmegaFunct3::VALID, 10, 11, 12)  // 0x02c5b57b
    };
}

} // namespace singular::v24::compiler
