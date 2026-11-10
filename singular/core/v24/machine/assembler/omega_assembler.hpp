#pragma once
// SINGULAR Ω v24 — Native Assembler Module (`omega.track/dirty/prop/valid`)
#include "../isa/encoding/omega_isa_encoding.hpp"
#include <string>

namespace singular::v24::machine {

inline uint32_t assemble_mnemonic(const std::string& op, uint32_t rd, uint32_t rs1, uint32_t rs2) noexcept {
    using namespace singular::v24::isa;
    if (op == "omega.track") return encode_omega_r(OmegaFunct3::TRACK, rd, rs1, rs2);
    if (op == "omega.dirty") return encode_omega_r(OmegaFunct3::DIRTY, rd, rs1, rs2);
    if (op == "omega.prop")  return encode_omega_r(OmegaFunct3::PROP,  rd, rs1, rs2);
    if (op == "omega.valid") return encode_omega_r(OmegaFunct3::VALID, rd, rs1, rs2);
    return 0u;
}

} // namespace singular::v24::machine
