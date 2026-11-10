#pragma once
// SINGULAR Ω v24 — Exhaustive 131,072-Case Bijective Codec & Illegal Instruction Fuzzer
#include "../../machine/isa/encoding/omega_isa_encoding.hpp"
#include <cstddef>

namespace singular::v24::verification {

struct FuzzCodecSummary {
    size_t valid_roundtrips = 0;
    size_t illegal_rejected = 0;
};

inline FuzzCodecSummary run_exhaustive_codec_fuzzer() noexcept {
    using namespace singular::v24::isa;
    FuzzCodecSummary s{};
    for (uint32_t f3 = 0; f3 < 4; ++f3) {
        for (uint32_t rd = 0; rd < 32; ++rd) {
            for (uint32_t rs1 = 0; rs1 < 32; ++rs1) {
                for (uint32_t rs2 = 0; rs2 < 32; ++rs2) {
                    uint32_t w = encode_omega_r(static_cast<OmegaFunct3>(f3), rd, rs1, rs2);
                    auto d = decode_omega_r(w);
                    if (d.valid && encode_omega_r(d.funct3, d.rd, d.rs1, d.rs2) == w) ++s.valid_roundtrips;
                }
            }
        }
    }
    for (uint32_t bad_f7 = 0; bad_f7 < 128; ++bad_f7) {
        if (bad_f7 == OMEGA_FUNCT7_ARC) continue;
        uint32_t w = (bad_f7 << 25) | OMEGA_OPCODE_CUSTOM3;
        if (!decode_omega_r(w).valid) ++s.illegal_rejected;
    }
    for (uint32_t bad_f3 = 4; bad_f3 < 8; ++bad_f3) {
        uint32_t w = (OMEGA_FUNCT7_ARC << 25) | (bad_f3 << 12) | OMEGA_OPCODE_CUSTOM3;
        if (!decode_omega_r(w).valid) ++s.illegal_rejected;
    }
    if (!decode_omega_r(0xFFFFFFFFu).valid) ++s.illegal_rejected;
    if (!decode_omega_r(0x00000000u).valid) ++s.illegal_rejected;
    return s;
}

} // namespace singular::v24::verification
