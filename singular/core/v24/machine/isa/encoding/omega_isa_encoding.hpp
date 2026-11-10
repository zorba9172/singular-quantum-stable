#pragma once
// SINGULAR Ω v24 — Canonical Ω-ISA R-Type Encoder/Decoder (opcode=0x7B, funct7=0x01)
#include <cstdint>

namespace singular::v24::isa {

constexpr uint32_t OMEGA_OPCODE_CUSTOM3 = 0x7Bu;
constexpr uint32_t OMEGA_FUNCT7_ARC     = 0x01u;

enum class OmegaFunct3 : uint32_t {
    TRACK = 0x0u,
    DIRTY = 0x1u,
    PROP  = 0x2u,
    VALID = 0x3u
};

struct DecodedOmegaInsn {
    bool valid = false;
    OmegaFunct3 funct3 = OmegaFunct3::TRACK;
    uint32_t rd = 0, rs1 = 0, rs2 = 0;
};

constexpr uint32_t encode_omega_r(OmegaFunct3 f3, uint32_t rd, uint32_t rs1, uint32_t rs2) noexcept {
    return (OMEGA_FUNCT7_ARC << 25) | ((rs2 & 0x1Fu) << 20) | ((rs1 & 0x1Fu) << 15) |
           ((static_cast<uint32_t>(f3) & 0x7u) << 12) | ((rd & 0x1Fu) << 7) | OMEGA_OPCODE_CUSTOM3;
}

constexpr DecodedOmegaInsn decode_omega_r(uint32_t word) noexcept {
    DecodedOmegaInsn d{};
    uint32_t opcode = word & 0x7Fu;
    uint32_t f3     = (word >> 12) & 0x7u;
    uint32_t f7     = (word >> 25) & 0x7Fu;
    if (opcode != OMEGA_OPCODE_CUSTOM3 || f7 != OMEGA_FUNCT7_ARC || f3 > 3u) return d;
    d.valid  = true;
    d.funct3 = static_cast<OmegaFunct3>(f3);
    d.rd     = (word >> 7) & 0x1Fu;
    d.rs1    = (word >> 15) & 0x1Fu;
    d.rs2    = (word >> 20) & 0x1Fu;
    return d;
}

} // namespace singular::v24::isa
