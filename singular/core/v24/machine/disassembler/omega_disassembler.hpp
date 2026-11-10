#pragma once
// SINGULAR Ω v24 — Native Disassembler Module (`0x7B` R-Type)
#include "../isa/encoding/omega_isa_encoding.hpp"
#include <sstream>
#include <string>

namespace singular::v24::machine {

inline std::string disassemble_word(uint32_t word) {
    using namespace singular::v24::isa;
    auto d = decode_omega_r(word);
    if (!d.valid) return "<illegal>";
    static const char* names[4] = {"omega.track", "omega.dirty", "omega.prop", "omega.valid"};
    std::ostringstream os;
    os << names[static_cast<uint32_t>(d.funct3)] << " x" << d.rd << ", x" << d.rs1 << ", x" << d.rs2;
    return os.str();
}

} // namespace singular::v24::machine
