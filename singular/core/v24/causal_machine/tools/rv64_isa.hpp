// SINGULAR Ω v24 causal_machine — RV64 encoding tables for singular-as / singular-objdump.
// Base RV64I subset + Zicsr + mret, plus Xomegacausal from the generated ISA header.
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "../isa/gen/omega_isa_gen.h"

namespace rv64 {

enum class Fmt { R, I, IShift, Load, S, B, U, J, Jalr, Csr, Sys, Omega };

struct Op {
    const char* name;
    Fmt fmt;
    uint32_t opcode, funct3, funct7;
};

// funct7 for IShift holds the upper 6 bits (imm[11:6]) for RV64 shifts.
inline const std::vector<Op>& ops() {
    static const std::vector<Op> t = {
        {"lui", Fmt::U, 0x37, 0, 0},       {"auipc", Fmt::U, 0x17, 0, 0},
        {"jal", Fmt::J, 0x6F, 0, 0},       {"jalr", Fmt::Jalr, 0x67, 0, 0},
        {"beq", Fmt::B, 0x63, 0, 0},       {"bne", Fmt::B, 0x63, 1, 0},
        {"blt", Fmt::B, 0x63, 4, 0},       {"bge", Fmt::B, 0x63, 5, 0},
        {"bltu", Fmt::B, 0x63, 6, 0},      {"bgeu", Fmt::B, 0x63, 7, 0},
        {"lb", Fmt::Load, 0x03, 0, 0},     {"lh", Fmt::Load, 0x03, 1, 0},
        {"lw", Fmt::Load, 0x03, 2, 0},     {"ld", Fmt::Load, 0x03, 3, 0},
        {"lbu", Fmt::Load, 0x03, 4, 0},    {"lhu", Fmt::Load, 0x03, 5, 0},
        {"lwu", Fmt::Load, 0x03, 6, 0},    {"sb", Fmt::S, 0x23, 0, 0},
        {"sh", Fmt::S, 0x23, 1, 0},        {"sw", Fmt::S, 0x23, 2, 0},
        {"sd", Fmt::S, 0x23, 3, 0},        {"addi", Fmt::I, 0x13, 0, 0},
        {"slti", Fmt::I, 0x13, 2, 0},      {"sltiu", Fmt::I, 0x13, 3, 0},
        {"xori", Fmt::I, 0x13, 4, 0},      {"ori", Fmt::I, 0x13, 6, 0},
        {"andi", Fmt::I, 0x13, 7, 0},      {"slli", Fmt::IShift, 0x13, 1, 0x00},
        {"srli", Fmt::IShift, 0x13, 5, 0x00}, {"srai", Fmt::IShift, 0x13, 5, 0x10},
        {"addiw", Fmt::I, 0x1B, 0, 0},     {"add", Fmt::R, 0x33, 0, 0x00},
        {"sub", Fmt::R, 0x33, 0, 0x20},    {"sll", Fmt::R, 0x33, 1, 0x00},
        {"slt", Fmt::R, 0x33, 2, 0x00},    {"sltu", Fmt::R, 0x33, 3, 0x00},
        {"xor", Fmt::R, 0x33, 4, 0x00},    {"srl", Fmt::R, 0x33, 5, 0x00},
        {"sra", Fmt::R, 0x33, 5, 0x20},    {"or", Fmt::R, 0x33, 6, 0x00},
        {"and", Fmt::R, 0x33, 7, 0x00},    {"mul", Fmt::R, 0x33, 0, 0x01},
        {"csrrw", Fmt::Csr, 0x73, 1, 0},   {"csrrs", Fmt::Csr, 0x73, 2, 0},
        {"csrrc", Fmt::Csr, 0x73, 3, 0},   {"mret", Fmt::Sys, 0x73, 0, 0},
        {"ecall", Fmt::Sys, 0x73, 0, 0},
    };
    return t;
}

inline const Op* find_op(const std::string& n) {
    for (const auto& o : ops())
        if (n == o.name) return &o;
    return nullptr;
}

inline const omega_insn_desc_t* find_omega(const std::string& n) {
    for (const auto& d : OMEGA_INSNS)
        if (n == d.mnemonic || n == d.symbol) return &d;
    return nullptr;
}

inline uint32_t enc_r(uint32_t op, uint32_t f3, uint32_t f7, uint32_t rd, uint32_t rs1, uint32_t rs2) {
    return (f7 << 25) | (rs2 << 20) | (rs1 << 15) | (f3 << 12) | (rd << 7) | op;
}
inline uint32_t enc_i(uint32_t op, uint32_t f3, uint32_t rd, uint32_t rs1, int32_t imm) {
    return (static_cast<uint32_t>(imm & 0xFFF) << 20) | (rs1 << 15) | (f3 << 12) | (rd << 7) | op;
}
inline uint32_t enc_s(uint32_t op, uint32_t f3, uint32_t rs1, uint32_t rs2, int32_t imm) {
    uint32_t u = static_cast<uint32_t>(imm);
    return (((u >> 5) & 0x7F) << 25) | (rs2 << 20) | (rs1 << 15) | (f3 << 12) | ((u & 0x1F) << 7) | op;
}
inline uint32_t enc_b(uint32_t op, uint32_t f3, uint32_t rs1, uint32_t rs2, int32_t off) {
    uint32_t u = static_cast<uint32_t>(off);
    return (((u >> 12) & 1) << 31) | (((u >> 5) & 0x3F) << 25) | (rs2 << 20) | (rs1 << 15) | (f3 << 12) |
           (((u >> 1) & 0xF) << 8) | (((u >> 11) & 1) << 7) | op;
}
inline uint32_t enc_u(uint32_t op, uint32_t rd, int32_t imm20) {
    return (static_cast<uint32_t>(imm20 & 0xFFFFF) << 12) | (rd << 7) | op;
}
inline uint32_t enc_j(uint32_t op, uint32_t rd, int32_t off) {
    uint32_t u = static_cast<uint32_t>(off);
    return (((u >> 20) & 1) << 31) | (((u >> 1) & 0x3FF) << 21) | (((u >> 11) & 1) << 20) | (((u >> 12) & 0xFF) << 12) |
           (rd << 7) | op;
}

inline const char* const* abi_names() {
    static const char* const n[32] = {"zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2", "s0", "s1", "a0",
                                      "a1",   "a2", "a3", "a4", "a5", "a6", "a7", "s2", "s3", "s4", "s5",
                                      "s6",   "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"};
    return n;
}

inline int parse_reg(const std::string& s) {
    if (s.size() >= 2 && s[0] == 'x') {
        int v = 0;
        for (size_t i = 1; i < s.size(); ++i) {
            if (s[i] < '0' || s[i] > '9') return -1;
            v = v * 10 + (s[i] - '0');
            if (v > 31) return -1;
        }
        return v;
    }
    if (s == "fp") return 8;
    for (int i = 0; i < 32; ++i)
        if (s == abi_names()[i]) return i;
    return -1;
}

struct CsrName {
    const char* name;
    uint32_t addr;
};
inline const std::vector<CsrName>& csr_names() {
    static const std::vector<CsrName> t = {
        {"mstatus", 0x300}, {"misa", 0x301},   {"mtvec", 0x305},    {"mscratch", 0x340},
        {"mepc", 0x341},    {"mcause", 0x342}, {"mtval", 0x343},    {"mcycle", 0xB00},
        {"minstret", 0xB02}, {"momegaen", OMEGA_CSR_MOMEGAEN},
    };
    return t;
}

inline std::string reg_name(uint32_t r) { return std::string("x") + std::to_string(r); }

inline int32_t sext(uint32_t v, int bits) {
    uint32_t m = 1u << (bits - 1);
    return static_cast<int32_t>((v ^ m) - m);
}

// Disassembles one 32-bit word (used by singular-objdump). pc is used for branch targets.
inline std::string disasm(uint32_t w, uint64_t pc) {
    char buf[128];
    for (const auto& d : OMEGA_INSNS) {
        if ((w & d.mask) == d.match) {
            uint32_t rd = (w >> 7) & 31, rs1 = (w >> 15) & 31, rs2 = (w >> 20) & 31;
            if (d.rs2_zero) std::snprintf(buf, sizeof buf, "%s x%u, x%u", d.mnemonic, rd, rs1);
            else std::snprintf(buf, sizeof buf, "%s x%u, x%u, x%u", d.mnemonic, rd, rs1, rs2);
            return buf;
        }
    }
    uint32_t op = w & 0x7F, rd = (w >> 7) & 31, f3 = (w >> 12) & 7, rs1 = (w >> 15) & 31, rs2 = (w >> 20) & 31,
             f7 = w >> 25;
    if (w == 0x30200073u) return "mret";
    if (w == 0x00000073u) return "ecall";
    for (const auto& o : ops()) {
        if (o.opcode != op) continue;
        switch (o.fmt) {
        case Fmt::R:
            if (o.funct3 == f3 && o.funct7 == f7) {
                std::snprintf(buf, sizeof buf, "%s x%u, x%u, x%u", o.name, rd, rs1, rs2);
                return buf;
            }
            break;
        case Fmt::I: case Fmt::Load: case Fmt::Jalr:
            if (o.funct3 == f3 && (o.fmt != Fmt::Jalr || f3 == 0)) {
                if (o.fmt == Fmt::I) std::snprintf(buf, sizeof buf, "%s x%u, x%u, %d", o.name, rd, rs1, sext(w >> 20, 12));
                else std::snprintf(buf, sizeof buf, "%s x%u, %d(x%u)", o.name, rd, sext(w >> 20, 12), rs1);
                return buf;
            }
            break;
        case Fmt::IShift:
            if (o.funct3 == f3 && (w >> 26) == o.funct7) {
                std::snprintf(buf, sizeof buf, "%s x%u, x%u, %u", o.name, rd, rs1, (w >> 20) & 0x3F);
                return buf;
            }
            break;
        case Fmt::S:
            if (o.funct3 == f3) {
                std::snprintf(buf, sizeof buf, "%s x%u, %d(x%u)", o.name, rs2, sext(((w >> 25) << 5) | ((w >> 7) & 31), 12), rs1);
                return buf;
            }
            break;
        case Fmt::B:
            if (o.funct3 == f3) {
                int32_t off = sext((((w >> 31) & 1) << 12) | (((w >> 7) & 1) << 11) | (((w >> 25) & 0x3F) << 5) | (((w >> 8) & 0xF) << 1), 13);
                std::snprintf(buf, sizeof buf, "%s x%u, x%u, 0x%llx", o.name, rs1, rs2, (unsigned long long)(pc + off));
                return buf;
            }
            break;
        case Fmt::U:
            std::snprintf(buf, sizeof buf, "%s x%u, 0x%x", o.name, rd, w >> 12);
            return buf;
        case Fmt::J: {
            int32_t off = sext((((w >> 31) & 1) << 20) | (((w >> 12) & 0xFF) << 12) | (((w >> 20) & 1) << 11) | (((w >> 21) & 0x3FF) << 1), 21);
            std::snprintf(buf, sizeof buf, "jal x%u, 0x%llx", rd, (unsigned long long)(pc + off));
            return buf;
        }
        case Fmt::Csr:
            if (o.funct3 == f3) {
                std::snprintf(buf, sizeof buf, "%s x%u, 0x%03x, x%u", o.name, rd, w >> 20, rs1);
                return buf;
            }
            break;
        default:
            break;
        }
    }
    std::snprintf(buf, sizeof buf, ".word 0x%08x", w);
    return buf;
}

} // namespace rv64
