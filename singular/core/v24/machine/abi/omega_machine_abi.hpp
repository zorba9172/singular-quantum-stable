#pragma once
// SINGULAR Ω v24 — Machine ABI Specification (RV64 & ARM64 Register & CSR Conventions)
#include <cstdint>

namespace singular::v24::machine {

constexpr uint32_t CSR_MOMEGAEN   = 0x7C0u;
constexpr uint32_t ABI_ARG_REG0   = 10u; // x10 / a0
constexpr uint32_t ABI_ARG_REG1   = 11u; // x11 / a1
constexpr uint32_t ABI_ARG_REG2   = 12u; // x12 / a2
constexpr uint32_t ABI_RET_REG    = 10u; // x10 / a0

} // namespace singular::v24::machine
