#pragma once
// SINGULAR Ω v24 — Kernel Capability Guard (`owner_cap` & Bounds Isolation)
#include <cstdint>

namespace singular::v24::kernel {

inline bool verify_capability_and_bounds(uint64_t state_id, uint16_t caller_cap, uint16_t owner_cap, uint64_t& out_err) noexcept {
    if (state_id >= 1024) { out_err = 1; return false; }
    if (caller_cap != owner_cap) { out_err = 3; return false; }
    out_err = 0;
    return true;
}

} // namespace singular::v24::kernel
