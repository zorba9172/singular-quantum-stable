#pragma once
// SINGULAR Ω v24 — `.sga` Canonical Operator Table (`Ω.TRACK`, `Ω.DIRTY`, `Ω.PROP`, `Ω.VALID`, `∂`)
#include <array>
#include <cstdint>
#include <string_view>

namespace singular::v24::language {

struct SgaOpSpec {
    std::string_view symbol;
    std::string_view mnemonic;
    uint32_t opcode;
    uint32_t funct3;
    bool is_hardware_arc_op;
};

constexpr std::array<SgaOpSpec, 5> kSgaOperators = {{
    {"Ω.TRACK", "omega.track", 0x7Bu, 0x0u, true},
    {"Ω.DIRTY", "omega.dirty", 0x7Bu, 0x1u, true},
    {"Ω.PROP",  "omega.prop",  0x7Bu, 0x2u, true},
    {"Ω.VALID", "omega.valid", 0x7Bu, 0x3u, true},
    {"∂",       "math.deriv",  0x00u, 0x0u, false}
}};

} // namespace singular::v24::language
