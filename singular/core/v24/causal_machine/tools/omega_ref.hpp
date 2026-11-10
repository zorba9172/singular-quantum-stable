// SINGULAR Ω v24 causal_machine — independent reference semantics for Xomegacausal.
// This is the executable form of isa/omega_isa.yaml "semantics" fields. It is written
// separately from the Spike extension (fixed-point mask closure) and from the RTL
// (one closure step per clock): the reference uses an explicit worklist search.
#pragma once
#include <array>
#include <cstdint>

#include "../isa/gen/omega_isa_gen.h"

namespace omega_ref {

enum class Priv : uint8_t { U = 0, S = 1, M = 3 };

struct Table {
    std::array<uint32_t, OMEGA_ENTRIES> version{};
    std::array<uint8_t, OMEGA_ENTRIES> dirty{};
    std::array<uint64_t, OMEGA_ENTRIES> deps{};
    void reset() { *this = Table{}; }
    bool operator==(const Table& o) const { return version == o.version && dirty == o.dirty && deps == o.deps; }
};

struct Decoded {
    bool ours = false;   // opcode is OMEGA_OPCODE
    bool legal = false;  // defined encoding of this extension
    uint32_t funct3 = 0, rd = 0, rs1 = 0, rs2 = 0;
};

inline Decoded decode(uint32_t insn) {
    Decoded d;
    d.ours = (insn & 0x7Fu) == OMEGA_OPCODE;
    d.rd = (insn >> 7) & 31u;
    d.funct3 = (insn >> 12) & 7u;
    d.rs1 = (insn >> 15) & 31u;
    d.rs2 = (insn >> 20) & 31u;
    if (!d.ours) return d;
    for (const auto& desc : OMEGA_INSNS) {
        if ((insn & desc.mask) == desc.match) {
            d.legal = true;
            break;
        }
    }
    return d;
}

inline bool permitted(Priv p, uint64_t momegaen) {
    if (p == Priv::M) return true;
    if (p == Priv::S) return (momegaen >> OMEGA_CSR_S_BIT) & 1u;
    return (momegaen >> OMEGA_CSR_U_BIT) & 1u;
}

inline uint64_t err(uint64_t code) { return OMEGA_ERR_FLAG | code; }

// Transitive closure of dependents reachable from `src` by one or more edges.
inline uint64_t closure_worklist(const Table& t, uint32_t src) {
    uint64_t seen = 0;
    uint32_t stack[OMEGA_ENTRIES];
    int sp = 0;
    stack[sp++] = src;
    while (sp > 0) {
        uint32_t n = stack[--sp];
        uint64_t succ = t.deps[n];
        for (uint32_t j = 0; j < OMEGA_ENTRIES; ++j) {
            if (((succ >> j) & 1u) && !((seen >> j) & 1u)) {
                seen |= (1ull << j);
                stack[sp++] = j;
            }
        }
    }
    return seen;
}

// Executes one legal, permitted Xomegacausal instruction. Returns the rd value.
inline uint64_t execute(Table& t, uint32_t funct3, uint64_t a, uint64_t b) {
    const uint64_t n = OMEGA_ENTRIES;
    switch (funct3) {
    case OMEGA_F3_TRACK:
        if (a >= n || b >= n) return err(OMEGA_ERR_INVALID_ID);
        if (a == b) return err(OMEGA_ERR_SELF_DEP);
        t.deps[a] |= (1ull << b);
        return t.deps[a];
    case OMEGA_F3_DIRTY:
        if (a >= n) return err(OMEGA_ERR_INVALID_ID);
        if (b != t.version[a]) return err(OMEGA_ERR_VERSION_MISMATCH);
        t.version[a] = static_cast<uint32_t>(t.version[a] + 1u);
        t.dirty[a] = 1;
        return t.version[a];
    case OMEGA_F3_PROP: {
        if (a >= n) return err(OMEGA_ERR_INVALID_ID);
        if (!t.dirty[a]) return 0;
        uint64_t reach = closure_worklist(t, static_cast<uint32_t>(a));
        for (uint32_t j = 0; j < n; ++j) {
            if (((reach >> j) & 1u) && !t.dirty[j]) {
                t.dirty[j] = 1;
                t.version[j] = static_cast<uint32_t>(t.version[j] + 1u);
            }
        }
        return reach & ~(1ull << a);
    }
    case OMEGA_F3_VALID:
        if (a >= n) return err(OMEGA_ERR_INVALID_ID);
        return (!t.dirty[a] && b == t.version[a]) ? 1u : 0u;
    case OMEGA_F3_CLEAN:
        if (a >= n) return err(OMEGA_ERR_INVALID_ID);
        t.dirty[a] = 0;
        return t.version[a];
    default:
        return err(0);  // unreachable for legal encodings
    }
}

} // namespace omega_ref
