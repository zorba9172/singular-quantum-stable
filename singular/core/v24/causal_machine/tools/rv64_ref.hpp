// SINGULAR Ω v24 causal_machine — reference RV64 machine (subset) with Xomegacausal.
// Executes the same bare-metal ELF images Spike runs and produces a normalized commit
// trace (priv, pc, insn, x-register write) for differential comparison with Spike.
// Supported: RV64I subset used by singular-as, MUL, Zicsr on the modeled CSRs, MRET,
// illegal-instruction traps (mcause 2) and Xomegacausal with momegaen gating.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "omega_ref.hpp"

namespace rv64_ref {

struct Commit {
    uint8_t priv;
    uint64_t pc;
    uint32_t insn;
    int rd = -1;  // -1: no x-register write
    uint64_t value = 0;
};

inline std::string format(const Commit& c) {
    char b[96];
    if (c.rd > 0) std::snprintf(b, sizeof b, "%u 0x%016llx (0x%08x) x%d 0x%016llx", c.priv, (unsigned long long)c.pc, c.insn, c.rd, (unsigned long long)c.value);
    else std::snprintf(b, sizeof b, "%u 0x%016llx (0x%08x)", c.priv, (unsigned long long)c.pc, c.insn);
    return b;
}

struct OmegaEvent {  // operands seen by each executed Xomegacausal instruction (feeds the RTL testbench)
    uint32_t insn;
    uint64_t rs1, rs2;
    uint8_t priv;
    uint64_t momegaen;
    bool trapped;
    uint64_t result;
};

class Machine {
 public:
    Machine(uint64_t base, std::vector<uint8_t> image, uint64_t tohost) : base_(base), mem_(std::move(image)), tohost_(tohost) {
        mem_.resize(mem_.size() + (1u << 20), 0);  // 1 MiB of zeroed RAM after the image
        pc_ = base;
        // Architectural state left by Spike's reset ROM before it jumps to the ELF entry:
        // t0 = entry, a0 = mhartid (0), a1 = device-tree address (ROM base + 0x20).
        x[5] = base;
        x[11] = 0x1020;
    }

    omega_ref::Table table;
    std::vector<OmegaEvent> omega_events;
    uint64_t x[32] = {};
    bool halted = false;
    uint64_t exit_code = 0, instret = 0;

    // Executes one instruction; returns the commit record (nullopt when it trapped).
    std::optional<Commit> step() {
        const uint64_t pc = pc_;
        uint32_t insn = static_cast<uint32_t>(load(pc, 4));
        Commit c{static_cast<uint8_t>(priv_), pc, insn};
        uint64_t next = pc + 4;
        uint32_t op = insn & 0x7F, rd = (insn >> 7) & 31, f3 = (insn >> 12) & 7, rs1 = (insn >> 15) & 31, rs2 = (insn >> 20) & 31,
                 f7 = insn >> 25;
        int64_t imm_i = static_cast<int32_t>(insn) >> 20;
        int64_t imm_s = static_cast<int32_t>(static_cast<uint32_t>(static_cast<int32_t>(insn & 0xFE000000u) >> 20) | ((insn >> 7) & 31));
        int64_t imm_b = static_cast<int32_t>(static_cast<uint32_t>(static_cast<int32_t>(insn & 0x80000000u) >> 19) | ((insn & 0x80) << 4) | ((insn >> 20) & 0x7E0) | ((insn >> 7) & 0x1E));
        int64_t imm_u = static_cast<int32_t>(insn & 0xFFFFF000u);
        int64_t imm_j = static_cast<int32_t>(static_cast<uint32_t>(static_cast<int32_t>(insn & 0x80000000u) >> 11) | (insn & 0xFF000) | ((insn >> 9) & 0x800) | ((insn >> 20) & 0x7FE));
        const uint64_t a = x[rs1], b = x[rs2];
        bool write = true;
        uint64_t v = 0;
        switch (op) {
        case 0x37: v = static_cast<uint64_t>(imm_u); break;
        case 0x17: v = pc + static_cast<uint64_t>(imm_u); break;
        case 0x6F: v = pc + 4; next = pc + static_cast<uint64_t>(imm_j); break;
        case 0x67:
            if (f3 != 0) return trap(insn);
            v = pc + 4; next = (a + static_cast<uint64_t>(imm_i)) & ~1ull; break;
        case 0x63: {
            bool t;
            switch (f3) {
            case 0: t = a == b; break;
            case 1: t = a != b; break;
            case 4: t = static_cast<int64_t>(a) < static_cast<int64_t>(b); break;
            case 5: t = static_cast<int64_t>(a) >= static_cast<int64_t>(b); break;
            case 6: t = a < b; break;
            case 7: t = a >= b; break;
            default: return trap(insn);
            }
            if (t) next = pc + static_cast<uint64_t>(imm_b);
            write = false;
            break;
        }
        case 0x03: {
            uint64_t addr = a + static_cast<uint64_t>(imm_i);
            switch (f3) {
            case 0: v = static_cast<uint64_t>(static_cast<int8_t>(load(addr, 1))); break;
            case 1: v = static_cast<uint64_t>(static_cast<int16_t>(load(addr, 2))); break;
            case 2: v = static_cast<uint64_t>(static_cast<int32_t>(load(addr, 4))); break;
            case 3: v = load(addr, 8); break;
            case 4: v = load(addr, 1); break;
            case 5: v = load(addr, 2); break;
            case 6: v = load(addr, 4); break;
            default: return trap(insn);
            }
            break;
        }
        case 0x23: {
            if (f3 > 3) return trap(insn);
            uint64_t addr = a + static_cast<uint64_t>(imm_s);
            store(addr, b, 1u << f3);
            if (addr == tohost_ && (b & 1)) {
                halted = true;
                exit_code = b >> 1;
            }
            write = false;
            break;
        }
        case 0x13: {
            unsigned sh = (insn >> 20) & 0x3F;
            switch (f3) {
            case 0: v = a + static_cast<uint64_t>(imm_i); break;
            case 2: v = static_cast<int64_t>(a) < imm_i; break;
            case 3: v = a < static_cast<uint64_t>(imm_i); break;
            case 4: v = a ^ static_cast<uint64_t>(imm_i); break;
            case 6: v = a | static_cast<uint64_t>(imm_i); break;
            case 7: v = a & static_cast<uint64_t>(imm_i); break;
            case 1: if ((insn >> 26) != 0) return trap(insn); v = a << sh; break;
            case 5:
                if ((insn >> 26) == 0) v = a >> sh;
                else if ((insn >> 26) == 0x10) v = static_cast<uint64_t>(static_cast<int64_t>(a) >> sh);
                else return trap(insn);
                break;
            }
            break;
        }
        case 0x1B:
            if (f3 != 0) return trap(insn);
            v = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(a + static_cast<uint64_t>(imm_i)))));
            break;
        case 0x33:
            if (f7 == 0x01 && f3 == 0) { v = a * b; break; }
            if (f7 != 0 && !(f7 == 0x20 && (f3 == 0 || f3 == 5))) return trap(insn);
            switch (f3) {
            case 0: v = f7 ? a - b : a + b; break;
            case 1: v = a << (b & 63); break;
            case 2: v = static_cast<int64_t>(a) < static_cast<int64_t>(b); break;
            case 3: v = a < b; break;
            case 4: v = a ^ b; break;
            case 5: v = f7 ? static_cast<uint64_t>(static_cast<int64_t>(a) >> (b & 63)) : a >> (b & 63); break;
            case 6: v = a | b; break;
            case 7: v = a & b; break;
            }
            break;
        case 0x73: {
            if (insn == 0x30200073u) {  // mret
                if (priv_ != 3) return trap(insn);
                next = mepc_;
                priv_ = static_cast<unsigned>((mstatus_ >> 11) & 3);
                mstatus_ &= ~(3ull << 11);
                write = false;
                break;
            }
            uint32_t csr = insn >> 20;
            if (f3 < 1 || f3 > 3 || !csr_known(csr) || priv_ < ((csr >> 8) & 3)) return trap(insn);
            uint64_t old = csr_read(csr);
            bool do_write = f3 == 1 || rs1 != 0;
            if (do_write && (csr >> 10) == 3) return trap(insn);  // read-only CSR
            v = old;
            if (do_write) csr_write(csr, f3 == 1 ? a : f3 == 2 ? (old | a) : (old & ~a));
            break;
        }
        case OMEGA_OPCODE: {
            omega_ref::Decoded d = omega_ref::decode(insn);
            bool ok = d.legal && omega_ref::permitted(static_cast<omega_ref::Priv>(priv_), momegaen_);
            omega_events.push_back({insn, a, b, static_cast<uint8_t>(priv_), momegaen_, !ok, 0});
            if (!ok) return trap(insn);
            v = omega_ref::execute(table, d.funct3, a, b);
            omega_events.back().result = v;
            break;
        }
        default:
            return trap(insn);
        }
        if (write && rd != 0) {
            x[rd] = v;
            c.rd = static_cast<int>(rd);
            c.value = v;
        }
        pc_ = next;
        ++instret;
        return c;
    }

 private:
    uint64_t base_;
    std::vector<uint8_t> mem_;
    uint64_t tohost_, pc_;
    unsigned priv_ = 3;
    uint64_t mstatus_ = 0, mtvec_ = 0, mepc_ = 0, mcause_ = 0, mtval_ = 0, mscratch_ = 0, momegaen_ = 0;

    std::optional<Commit> trap(uint32_t insn) {  // illegal instruction
        mepc_ = pc_;
        mcause_ = 2;
        mtval_ = insn;
        mstatus_ = (mstatus_ & ~(3ull << 11)) | (static_cast<uint64_t>(priv_) << 11);
        priv_ = 3;
        pc_ = mtvec_ & ~3ull;
        return std::nullopt;
    }
    bool csr_known(uint32_t c) const {
        return c == 0x300 || c == 0x305 || c == 0x340 || c == 0x341 || c == 0x342 || c == 0x343 || c == OMEGA_CSR_MOMEGAEN;
    }
    uint64_t csr_read(uint32_t c) const {
        switch (c) {
        case 0x300: return mstatus_;
        case 0x305: return mtvec_;
        case 0x340: return mscratch_;
        case 0x341: return mepc_;
        case 0x342: return mcause_;
        case 0x343: return mtval_;
        default: return momegaen_;
        }
    }
    void csr_write(uint32_t c, uint64_t v) {
        switch (c) {
        case 0x300: mstatus_ = v & (3ull << 11); break;  // only MPP is modeled
        case 0x305: mtvec_ = v & ~3ull; break;            // direct mode only
        case 0x340: mscratch_ = v; break;
        case 0x341: mepc_ = v & ~1ull; break;
        case 0x342: mcause_ = v; break;
        case 0x343: mtval_ = v; break;
        default: momegaen_ = v & ((1ull << OMEGA_CSR_S_BIT) | (1ull << OMEGA_CSR_U_BIT)); break;
        }
    }
    uint8_t* at(uint64_t addr, unsigned n) {
        if (addr < base_ || addr + n > base_ + mem_.size()) throw std::runtime_error("refsim: memory access out of range at 0x" + std::to_string(addr) + " (pc " + std::to_string(pc_) + ")");
        return &mem_[addr - base_];
    }
    uint64_t load(uint64_t addr, unsigned n) {
        uint64_t v = 0;
        std::memcpy(&v, at(addr, n), n);
        return v;
    }
    void store(uint64_t addr, uint64_t v, unsigned n) { std::memcpy(at(addr, n), &v, n); }
};

} // namespace rv64_ref
