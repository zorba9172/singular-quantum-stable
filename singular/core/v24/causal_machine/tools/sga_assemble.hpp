// SINGULAR Ω v24 causal_machine — .sga assembler back end (pseudo expansion, layout,
// label resolution, RV64 + Xomegacausal encoding).
#pragma once
#include <cstring>
#include <map>

#include "sga_parse.hpp"

namespace sga {

constexpr uint64_t kBase = 0x80000000ull;

struct MInsn {  // one machine instruction before label resolution
    std::string op;
    std::vector<std::string> args;
    int line;
    size_t auipc_index = 0;  // for "%lo:" relocations
    std::string pseudo;      // set on the first instruction of a "halt" expansion
};

struct Image {
    std::vector<uint32_t> text;
    std::vector<uint64_t> data;            // 8-byte words following text
    std::map<std::string, uint64_t> syms;  // label -> address (includes tohost/fromhost/_start)
    std::vector<int> text_lines;           // source line per text word
    std::vector<MInsn> minsns;             // resolved-order machine instructions (for QBE lowering)
    uint64_t data_addr = 0, tohost_addr = 0, end_addr = 0;
};

inline bool fits_signed(long long v, int bits) { return v >= -(1ll << (bits - 1)) && v < (1ll << (bits - 1)); }

inline void expand(const Stmt& s, std::vector<MInsn>& out, std::vector<std::pair<std::string, size_t>>& text_labels) {
    auto need = [&](size_t n) {
        if (s.args.size() != n) throw Error(s.line, s.op + " expects " + std::to_string(n) + " operands");
    };
    if (s.op == "li") {
        need(2);
        long long v = to_int(s.args[1], s.line);
        if (!fits_signed(v, 32)) throw Error(s.line, "li supports signed 32-bit immediates; build wider constants with shifts");
        if (fits_signed(v, 12)) {
            out.push_back({"addi", {s.args[0], "x0", std::to_string(v)}, s.line});
        } else {
            long long lo = ((v & 0xFFF) ^ 0x800) - 0x800;
            long long hi = ((v - lo) >> 12) & 0xFFFFF;
            out.push_back({"lui", {s.args[0], std::to_string(hi)}, s.line});
            out.push_back({"addiw", {s.args[0], s.args[0], std::to_string(lo)}, s.line});
        }
    } else if (s.op == "mv") {
        need(2);
        out.push_back({"addi", {s.args[0], s.args[1], "0"}, s.line});
    } else if (s.op == "nop") {
        out.push_back({"addi", {"x0", "x0", "0"}, s.line});
    } else if (s.op == "j") {
        need(1);
        out.push_back({"jal", {"x0", s.args[0]}, s.line});
    } else if (s.op == "la") {
        need(2);
        size_t idx = out.size();
        out.push_back({"auipc", {s.args[0], "%hi:" + s.args[1]}, s.line});
        out.push_back({"addi", {s.args[0], s.args[0], "%lo:" + s.args[1]}, s.line, idx});
    } else if (s.op == "csrr") {
        need(2);
        out.push_back({"csrrs", {s.args[0], s.args[1], "x0"}, s.line});
    } else if (s.op == "csrw") {
        need(2);
        out.push_back({"csrrw", {"x0", s.args[0], s.args[1]}, s.line});
    } else if (s.op == "halt") {  // tohost <- (a0 << 1) | 1 ; spin. Clobbers t5, t6.
        std::string spin = "__halt_spin_" + std::to_string(out.size());
        out.push_back({"slli", {"t6", "a0", "1"}, s.line, 0, "halt"});
        out.push_back({"ori", {"t6", "t6", "1"}, s.line});
        size_t idx = out.size();
        out.push_back({"auipc", {"t5", "%hi:tohost"}, s.line});
        out.push_back({"addi", {"t5", "t5", "%lo:tohost"}, s.line, idx});
        out.push_back({"sd", {"t6", "0(t5)"}, s.line});
        text_labels.push_back({spin, out.size()});
        out.push_back({"jal", {"x0", spin}, s.line});
    } else {
        out.push_back({s.op, s.args, s.line});
    }
}

inline uint32_t reg_or_throw(const std::string& r, int line) {
    int v = rv64::parse_reg(r);
    if (v < 0) throw Error(line, "bad register '" + r + "'");
    return static_cast<uint32_t>(v);
}

inline void mem_arg(const std::string& a, int line, int32_t& imm, uint32_t& base) {
    size_t lp = a.find('('), rp = a.find(')');
    if (lp == std::string::npos || rp != a.size() - 1) throw Error(line, "expected imm(reg), got '" + a + "'");
    std::string is = lp == 0 ? "0" : a.substr(0, lp);
    long long v = to_int(is, line);
    if (!fits_signed(v, 12)) throw Error(line, "memory offset out of range");
    imm = static_cast<int32_t>(v);
    base = reg_or_throw(a.substr(lp + 1, rp - lp - 1), line);
}

inline uint32_t csr_addr(const std::string& s, int line) {
    for (const auto& c : rv64::csr_names())
        if (s == c.name) return c.addr;
    long long v = to_int(s, line);
    if (v < 0 || v > 0xFFF) throw Error(line, "csr out of range");
    return static_cast<uint32_t>(v);
}

inline uint32_t encode_one(const MInsn& m, uint64_t pc, const std::map<std::string, uint64_t>& syms) {
    const auto& a = m.args;
    auto need = [&](size_t n) {
        if (a.size() != n) throw Error(m.line, m.op + " expects " + std::to_string(n) + " operands");
    };
    auto sym = [&](const std::string& n) {
        auto it = syms.find(n);
        if (it == syms.end()) throw Error(m.line, "undefined label '" + n + "'");
        return it->second;
    };
    auto R = [&](size_t i) { return reg_or_throw(a[i], m.line); };
    if (const omega_insn_desc_t* d = rv64::find_omega(m.op)) {
        need(d->rs2_zero ? 2 : 3);
        return d->match | (R(0) << 7) | (R(1) << 15) | ((d->rs2_zero ? 0 : R(2)) << 20);
    }
    if (m.op == ".word") {
        long long v = to_int(a.at(0), m.line);
        if (v < 0 || v > 0xFFFFFFFFll) throw Error(m.line, ".word out of range");
        return static_cast<uint32_t>(v);
    }
    if (m.op == "mret") return 0x30200073u;
    if (m.op == "ecall") return 0x00000073u;
    const rv64::Op* o = rv64::find_op(m.op);
    if (!o) throw Error(m.line, "unknown instruction '" + m.op + "'");
    switch (o->fmt) {
    case rv64::Fmt::R:
        need(3);
        return rv64::enc_r(o->opcode, o->funct3, o->funct7, R(0), R(1), R(2));
    case rv64::Fmt::I: {
        need(3);
        long long imm;
        if (a[2].rfind("%lo:", 0) == 0) {
            long long off = static_cast<long long>(sym(a[2].substr(4)) - (kBase + 4 * m.auipc_index));
            imm = ((off & 0xFFF) ^ 0x800) - 0x800;
        } else {
            imm = to_int(a[2], m.line);
        }
        if (!fits_signed(imm, 12)) throw Error(m.line, "immediate out of range");
        return rv64::enc_i(o->opcode, o->funct3, R(0), R(1), static_cast<int32_t>(imm));
    }
    case rv64::Fmt::IShift: {
        need(3);
        long long sh = to_int(a[2], m.line);
        if (sh < 0 || sh > 63) throw Error(m.line, "shift amount out of range");
        return (o->funct7 << 26) | (static_cast<uint32_t>(sh) << 20) | (R(1) << 15) | (o->funct3 << 12) | (R(0) << 7) | o->opcode;
    }
    case rv64::Fmt::Load: case rv64::Fmt::Jalr: {
        need(2);
        int32_t imm; uint32_t base;
        mem_arg(a[1], m.line, imm, base);
        return rv64::enc_i(o->opcode, o->funct3, R(0), base, imm);
    }
    case rv64::Fmt::S: {
        need(2);
        int32_t imm; uint32_t base;
        mem_arg(a[1], m.line, imm, base);
        return rv64::enc_s(o->opcode, o->funct3, base, R(0), imm);
    }
    case rv64::Fmt::B: {
        need(3);
        long long off = static_cast<long long>(sym(a[2]) - pc);
        if (!fits_signed(off, 13)) throw Error(m.line, "branch target out of range");
        return rv64::enc_b(o->opcode, o->funct3, R(0), R(1), static_cast<int32_t>(off));
    }
    case rv64::Fmt::U: {
        need(2);
        long long v;
        if (a[1].rfind("%hi:", 0) == 0) {
            long long off = static_cast<long long>(sym(a[1].substr(4)) - pc);
            v = ((off + 0x800) >> 12) & 0xFFFFF;
        } else {
            v = to_int(a[1], m.line);
            if (v < 0 || v > 0xFFFFF) throw Error(m.line, "U-type immediate out of range");
        }
        return rv64::enc_u(o->opcode, R(0), static_cast<int32_t>(v));
    }
    case rv64::Fmt::J: {
        need(2);
        long long off = static_cast<long long>(sym(a[1]) - pc);
        if (!fits_signed(off, 21)) throw Error(m.line, "jump target out of range");
        return rv64::enc_j(o->opcode, R(0), static_cast<int32_t>(off));
    }
    case rv64::Fmt::Csr:
        need(3);
        return (csr_addr(a[1], m.line) << 20) | (R(2) << 15) | (o->funct3 << 12) | (R(0) << 7) | o->opcode;
    default:
        throw Error(m.line, "unsupported format for " + m.op);
    }
}

inline Image assemble(const std::vector<Stmt>& prog) {
    std::vector<MInsn> mi;
    std::vector<std::pair<std::string, size_t>> text_labels, data_labels;
    std::vector<uint64_t> data;
    bool in_data = false;
    for (const auto& s : prog) {
        if (s.op == ".data") { in_data = true; continue; }
        if (s.op == ".text") { in_data = false; continue; }
        if (s.op[0] == ':') {
            (in_data ? data_labels : text_labels).push_back({s.op.substr(1), in_data ? data.size() : mi.size()});
            continue;
        }
        if (in_data) {
            if (s.op == ".dword") {
                for (const auto& a : s.args) data.push_back(static_cast<uint64_t>(to_int(a, s.line)));
            } else if (s.op == ".zero") {
                long long n = s.args.size() == 1 ? to_int(s.args[0], s.line) : -1;
                if (n < 0 || n % 8) throw Error(s.line, ".zero expects a non-negative multiple of 8");
                data.insert(data.end(), static_cast<size_t>(n / 8), 0);
            } else {
                throw Error(s.line, "unknown data directive " + s.op);
            }
            continue;
        }
        if (s.op == ".word") {
            for (const auto& a : s.args) mi.push_back({".word", {a}, s.line});
            continue;
        }
        if (s.op[0] == '.') throw Error(s.line, "unknown text directive " + s.op);
        expand(s, mi, text_labels);
    }
    Image img;
    img.data_addr = kBase + ((mi.size() * 4 + 7) & ~7ull);
    img.tohost_addr = (img.data_addr + data.size() * 8 + 63) & ~63ull;
    img.end_addr = img.tohost_addr + 16;
    img.data = data;
    img.syms["tohost"] = img.tohost_addr;
    img.syms["fromhost"] = img.tohost_addr + 8;
    img.syms["__text_end"] = kBase + mi.size() * 4;
    for (auto& l : text_labels)
        if (!img.syms.emplace(l.first, kBase + 4 * l.second).second) throw Error(0, "duplicate or reserved label " + l.first);
    for (auto& l : data_labels)
        if (!img.syms.emplace(l.first, img.data_addr + 8 * l.second).second) throw Error(0, "duplicate or reserved label " + l.first);
    if (!img.syms.emplace("_start", kBase).second && img.syms["_start"] != kBase) throw Error(0, "_start must be the first instruction");
    for (size_t i = 0; i < mi.size(); ++i) {
        img.text.push_back(encode_one(mi[i], kBase + 4 * i, img.syms));
        img.text_lines.push_back(mi[i].line);
    }
    img.minsns = mi;
    return img;
}

} // namespace sga
