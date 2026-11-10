#pragma once
// SINGULAR Ω v24 — 1,024-Entry Architectural State Table & Error Semantics
#include "../encoding/omega_isa_encoding.hpp"
#include <array>
#include <cstdint>

namespace singular::v24::isa {

enum OmegaErrorCode : uint64_t {
    OMEGA_OK                = 0,
    ERR_INVALID_STATE_ID    = 1,
    ERR_VERSION_MISMATCH    = 2,
    ERR_CAPABILITY_DENIED   = 3,
    ERR_VERSION_OVERFLOW    = 4
};

struct StateEntry {
    uint32_t version    = 0;
    uint8_t  dirty      = 0;
    uint64_t deps_mask  = 0;
    uint32_t valid_cert = 0;
    uint16_t owner_cap  = 0x0001;
};

struct ArchitecturalStateTable {
    static constexpr size_t CAPACITY = 1024;
    std::array<StateEntry, CAPACITY> entries{};
    std::array<uint64_t, 16> dirty_bank{};
    uint16_t active_cap = 0x0001;

    uint64_t exec_track(uint64_t state_id, uint64_t dep_bit) noexcept {
        if (state_id >= CAPACITY || dep_bit >= 64) return ERR_INVALID_STATE_ID;
        if (entries[state_id].owner_cap != active_cap) return ERR_CAPABILITY_DENIED;
        entries[state_id].deps_mask |= (1ULL << dep_bit);
        return OMEGA_OK;
    }

    uint64_t exec_dirty(uint64_t state_id, uint64_t expected_ver) noexcept {
        if (state_id >= CAPACITY) return ERR_INVALID_STATE_ID;
        if (entries[state_id].owner_cap != active_cap) return ERR_CAPABILITY_DENIED;
        if (entries[state_id].version != expected_ver) return ERR_VERSION_MISMATCH;
        if (entries[state_id].version == 0xFFFFFFFFu) return ERR_VERSION_OVERFLOW;
        entries[state_id].version += 1;
        entries[state_id].dirty    = 1;
        dirty_bank[state_id >> 6] |= (1ULL << (state_id & 63));
        return OMEGA_OK;
    }

    uint64_t exec_prop(uint64_t state_id) noexcept {
        if (state_id >= CAPACITY) return 0;
        if (entries[state_id].owner_cap != active_cap || !entries[state_id].dirty) return 0;
        uint64_t mask = entries[state_id].deps_mask | (1ULL << (state_id & 63));
        dirty_bank[0] |= entries[state_id].deps_mask;
        uint64_t m = entries[state_id].deps_mask;
        while (m != 0ULL) {
            uint32_t b = static_cast<uint32_t>(__builtin_ctzll(m));
            entries[b].dirty = 1;
            m &= (m - 1ULL);
        }
        return mask;
    }

    uint64_t exec_valid(uint64_t state_id, uint64_t req_ver) noexcept {
        if (state_id >= CAPACITY || entries[state_id].owner_cap != active_cap) return 0;
        if (entries[state_id].dirty == 0 && entries[state_id].version == req_ver) return 1;
        entries[state_id].dirty      = 0;
        dirty_bank[state_id >> 6]   &= ~(1ULL << (state_id & 63));
        entries[state_id].valid_cert = entries[state_id].version;
        return 0;
    }
};

} // namespace singular::v24::isa
