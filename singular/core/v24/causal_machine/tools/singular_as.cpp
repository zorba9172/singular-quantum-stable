// SINGULAR Ω v24 causal_machine — singular-as / singular-objdump.
//   singular-as <in.sga> -o <out.elf> [--listing]   assemble to a bare-metal RV64 ELF (Spike-loadable)
//   singular-as --emit-qbe <in.sga> -o <out.ssa>     lower to QBE IL for the EMULATED path (ARM64 host;
//                                                    Ω ops become calls to omega_rt_* software runtime)
//   singular-as --emit-gnu <in.sga> -o <out.s>       GNU-syntax listing (Ω ops as .int) for cross-assembly
//   singular-as --selftest-encoding                  exhaustive Ω encoding/decoding checks
//   singular-objdump <file.elf>                      disassemble the .text of an ELF
#include <cstdio>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

#include "elf64.hpp"
#include "sga_assemble.hpp"

namespace {

std::string slurp(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + p);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

elf64::Program to_program(const sga::Image& img) {
    elf64::Program p;
    p.entry = p.base = sga::kBase;
    p.bytes.assign(img.end_addr - sga::kBase, 0);
    for (size_t i = 0; i < img.text.size(); ++i) std::memcpy(&p.bytes[4 * i], &img.text[i], 4);
    for (size_t i = 0; i < img.data.size(); ++i) std::memcpy(&p.bytes[img.data_addr - sga::kBase + 8 * i], &img.data[i], 8);
    p.syms = img.syms;
    return p;
}

int objdump(const std::string& path) {
    elf64::Program p = elf64::read(path);
    std::map<uint64_t, std::string> by_addr;
    for (const auto& kv : p.syms) by_addr[kv.second] += (by_addr[kv.second].empty() ? "" : ", ") + kv.first;
    uint64_t end = p.syms.count("__text_end") ? p.syms["__text_end"] : p.base + p.bytes.size();
    std::printf("%s:\tfile format elf64-littleriscv (Xomegacausal %s)\n\n", path.c_str(), OMEGA_ISA_VERSION);
    for (uint64_t a = p.base; a + 4 <= end; a += 4) {
        auto it = by_addr.find(a);
        if (it != by_addr.end()) std::printf("\n%016llx <%s>:\n", (unsigned long long)a, it->second.c_str());
        uint32_t w;
        std::memcpy(&w, &p.bytes[a - p.base], 4);
        std::printf("%8llx:\t%08x\t%s\n", (unsigned long long)a, w, rv64::disasm(w, a).c_str());
    }
    return 0;
}

// Independent legality formula (not the generated MATCH/MASK table) for the exhaustive check.
bool legal_by_formula(uint32_t w) {
    uint32_t f3 = (w >> 12) & 7, f7 = w >> 25, rs2 = (w >> 20) & 31;
    if ((w & 0x7F) != OMEGA_OPCODE || f7 != OMEGA_FUNCT7) return false;
    if (f3 == OMEGA_F3_TRACK || f3 == OMEGA_F3_DIRTY || f3 == OMEGA_F3_VALID) return true;
    if (f3 == OMEGA_F3_PROP || f3 == OMEGA_F3_CLEAN) return rs2 == 0;
    return false;
}

int selftest_encoding() {
    // 1) every 32-bit word whose opcode is custom-3: table decoder vs formula (2^25 words)
    uint64_t checked = 0, legal = 0, mismatches = 0;
    for (uint32_t hi = 0; hi < (1u << 25); ++hi) {
        uint32_t w = (hi << 7) | OMEGA_OPCODE;
        bool t = false;
        for (const auto& d : OMEGA_INSNS) t = t || ((w & d.mask) == d.match);
        bool f = legal_by_formula(w);
        mismatches += (t != f);
        legal += f;
        ++checked;
    }
    // 2) assemble -> encode -> disassemble -> reassemble round trip for every legal operand combination
    uint64_t roundtrips = 0, rt_fail = 0;
    for (const auto& d : OMEGA_INSNS) {
        for (uint32_t rd = 0; rd < 32; ++rd)
            for (uint32_t rs1 = 0; rs1 < 32; ++rs1)
                for (uint32_t rs2 = 0; rs2 < (d.rs2_zero ? 1u : 32u); ++rs2) {
                    std::string src = std::string(d.mnemonic) + " x" + std::to_string(rd) + ", x" + std::to_string(rs1);
                    if (!d.rs2_zero) src += ", x" + std::to_string(rs2);
                    uint32_t w = sga::assemble(sga::parse(src + "\n")).text.at(0);
                    std::string dis = rv64::disasm(w, sga::kBase);
                    uint32_t w2 = sga::assemble(sga::parse(dis + "\n")).text.at(0);
                    bool fields_ok = ((w >> 7) & 31) == rd && ((w >> 15) & 31) == rs1 && ((w >> 20) & 31) == rs2 &&
                                     ((w >> 12) & 7) == d.funct3 && (w >> 25) == OMEGA_FUNCT7 && (w & 0x7F) == OMEGA_OPCODE;
                    rt_fail += !(fields_ok && w2 == w && dis == src);
                    ++roundtrips;
                }
    }
    // 3) malformed source must be rejected
    const char* bad[] = {"omega.dirty x32, x1, x2", "omega.prop x1, x2, x3", "omega.track x1, x2", "omega.bogus x1, x2, x3",
                         "omega.clean x1", "omega.valid q1, x2, x3"};
    unsigned rejected = 0;
    for (const char* b : bad) {
        try {
            sga::assemble(sga::parse(std::string(b) + "\n"));
        } catch (const std::exception&) {
            ++rejected;
        }
    }
    unsigned n_bad = sizeof(bad) / sizeof(bad[0]);
    bool ok = mismatches == 0 && rt_fail == 0 && rejected == n_bad;
    std::printf("ENCODING_SELFTEST custom3_words=%llu legal=%llu decoder_formula_mismatches=%llu roundtrips=%llu roundtrip_failures=%llu "
                "malformed_rejected=%u/%u RESULT=%s\n",
                (unsigned long long)checked, (unsigned long long)legal, (unsigned long long)mismatches, (unsigned long long)roundtrips,
                (unsigned long long)rt_fail, rejected, n_bad, ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

std::string emit_gnu(const sga::Image& img) {
    std::ostringstream o;
    o << "# GNU-syntax listing generated by singular-as (Xomegacausal words as .int)\n.text\n";
    for (size_t i = 0; i < img.text.size(); ++i) {
        uint32_t w = img.text[i];
        bool omega = (w & 0x7F) == OMEGA_OPCODE;
        std::string d = rv64::disasm(w, sga::kBase + 4 * i);
        bool pcrel = d.rfind("jal ", 0) == 0 || d.rfind("b", 0) == 0 || d.rfind("auipc", 0) == 0;
        bool no_tcc = d == "mret" || d == "ecall";  // TinyCC's RISC-V assembler has no mret/ecall mnemonics
        if (omega || pcrel || no_tcc || d.rfind(".word", 0) == 0) o << "  .int 0x" << std::hex << w << std::dec << "  # " << d << "\n";
        else o << "  " << d << "\n";
    }
    return o.str();
}

std::string qreg(const std::string& r) {
    int n = rv64::parse_reg(r);
    return n == 0 ? std::string("0") : "%x" + std::to_string(n);
}

std::string emit_qbe(const sga::Image& img) {
    std::ostringstream o;
    auto index_of = [&](const std::string& label, int line) {
        auto it = img.syms.find(label);
        if (it == img.syms.end() || it->second >= img.data_addr) throw sga::Error(line, "branch to non-text label " + label);
        return (it->second - sga::kBase) / 4;
    };
    o << "# QBE IL generated by singular-as --emit-qbe (EMULATED Xomegacausal: calls omega_rt_*)\n";
    if (!img.data.empty()) {
        o << "data $sga_data = align 8 {";
        for (size_t i = 0; i < img.data.size(); ++i) o << (i ? ", " : " ") << "l " << static_cast<long long>(img.data[i]);
        o << " }\n";
    }
    o << "export function l $sga_main(l %regs) {\n@start\n";
    for (int r = 1; r < 32; ++r) o << "  %x" << r << " =l copy 0\n";
    const auto& mi = img.minsns;
    for (size_t i = 0; i < mi.size(); ++i) {
        const sga::MInsn& m = mi[i];
        const auto& a = m.args;
        o << "@i" << i << "\n";
        auto dst = [&](const std::string& r) { return rv64::parse_reg(r) == 0 ? std::string("%discard") : qreg(r); };
        if (m.pseudo == "halt") {
            for (int r = 1; r < 32; ++r) o << "  %p" << r << " =l add %regs, " << 8 * r << "\n  storel %x" << r << ", %p" << r << "\n";
            o << "  ret %x10\n";
            i += 5;
            continue;
        }
        static const std::map<std::string, std::string> rr = {{"add", "add"}, {"sub", "sub"}, {"and", "and"}, {"or", "or"},
            {"xor", "xor"}, {"sll", "shl"}, {"srl", "shr"}, {"sra", "sar"}, {"mul", "mul"}, {"slt", "csltl"}, {"sltu", "cultl"},
            {"addi", "add"}, {"andi", "and"}, {"ori", "or"}, {"xori", "xor"}, {"slli", "shl"}, {"srli", "shr"}, {"srai", "sar"},
            {"slti", "csltl"}, {"sltiu", "cultl"}};
        static const std::map<std::string, std::string> br = {{"beq", "ceql"}, {"bne", "cnel"}, {"blt", "csltl"},
            {"bge", "csgel"}, {"bltu", "cultl"}, {"bgeu", "cugel"}};
        if (const omega_insn_desc_t* d = rv64::find_omega(m.op)) {
            std::string fn = std::string("omega_rt_") + (d->mnemonic + 6);
            o << "  " << dst(a[0]) << " =l call $" << fn << "(l " << qreg(a[1]) << ", l " << (d->rs2_zero ? "0" : qreg(a[2])) << ")\n";
        } else if (m.op == "auipc" && a[1].rfind("%hi:", 0) == 0) {
            uint64_t addr = img.syms.at(a[1].substr(4));
            if (addr < img.data_addr || addr >= img.data_addr + 8 * img.data.size())
                throw sga::Error(m.line, "emulated path: la supports data labels only");
            o << "  " << dst(a[0]) << " =l add $sga_data, " << (addr - img.data_addr) << "\n";
        } else if (m.op == "addi" && a[2].rfind("%lo:", 0) == 0) {
            o << "  # low part folded into the preceding address\n";
        } else if (m.op == "lui") {
            long long v = static_cast<int32_t>(static_cast<uint32_t>(sga::to_int(a[1], m.line)) << 12);
            o << "  " << dst(a[0]) << " =l copy " << v << "\n";
        } else if (m.op == "addiw") {
            o << "  %w" << i << " =w add " << qreg(a[1]) << ", " << a[2] << "\n  " << dst(a[0]) << " =l extsw %w" << i << "\n";
        } else if (rr.count(m.op)) {
            bool imm = m.op.back() == 'i' || m.op == "sltiu";
            std::string rhs = imm ? a[2] : qreg(a[2]);
            o << "  " << dst(a[0]) << " =l " << rr.at(m.op) << " " << qreg(a[1]) << ", " << rhs << "\n";
        } else if (br.count(m.op)) {
            o << "  %c" << i << " =w " << br.at(m.op) << " " << qreg(a[0]) << ", " << qreg(a[1]) << "\n  jnz %c" << i << ", @i"
              << index_of(a[2], m.line) << ", @i" << i + 1 << "\n";
        } else if (m.op == "jal" && rv64::parse_reg(a[0]) == 0) {
            o << "  jmp @i" << index_of(a[1], m.line) << "\n";
        } else if (m.op == "ld" || m.op == "sd") {
            int32_t off; uint32_t base;
            sga::mem_arg(a[1], m.line, off, base);
            o << "  %a" << i << " =l add " << qreg("x" + std::to_string(base)) << ", " << off << "\n";
            if (m.op == "ld") o << "  " << dst(a[0]) << " =l loadl %a" << i << "\n";
            else o << "  storel " << qreg(a[0]) << ", %a" << i << "\n";
        } else {
            throw sga::Error(m.line, "emulated path does not support '" + m.op + "' (privileged or indirect control flow)");
        }
    }
    o << "@i" << mi.size() << "\n  ret %x10\n}\n";
    return o.str();
}

} // namespace

int main(int argc, char** argv) {
    try {
        std::string self = argv[0];
        self = self.substr(self.find_last_of('/') == std::string::npos ? 0 : self.find_last_of('/') + 1);
        if (self == "singular-objdump" || (argc >= 2 && std::string(argv[1]) == "--objdump")) {
            if (argc < 2) return 2;
            return objdump(argv[argc - 1]);
        }
        if (argc >= 2 && std::string(argv[1]) == "--selftest-encoding") return selftest_encoding();
        if (argc == 3 && std::string(argv[1]) == "--text-words") {  // words of .text of any ELF (e.g. TinyCC object)
            auto t = elf64::read_section(argv[2], ".text");
            for (size_t i = 0; i + 4 <= t.size(); i += 4) {
                uint32_t w;
                std::memcpy(&w, &t[i], 4);
                std::printf("%08x\n", w);
            }
            return 0;
        }
        std::string mode = "elf", in, out;
        bool listing = false;
        for (int i = 1; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--emit-qbe") mode = "qbe";
            else if (a == "--emit-gnu") mode = "gnu";
            else if (a == "--listing") listing = true;
            else if (a == "-o" && i + 1 < argc) out = argv[++i];
            else in = a;
        }
        if (in.empty() || out.empty()) {
            std::cerr << "usage: singular-as [--emit-qbe|--emit-gnu] <in.sga> -o <out> [--listing]\n";
            return 2;
        }
        sga::Image img = sga::assemble(sga::parse(slurp(in)));
        if (mode == "elf") {
            elf64::write(out, to_program(img));
        } else {
            std::ofstream f(out);
            f << (mode == "qbe" ? emit_qbe(img) : emit_gnu(img));
            if (!f) throw std::runtime_error("cannot write " + out);
        }
        if (listing)
            for (size_t i = 0; i < img.text.size(); ++i)
                std::printf("%08llx: %08x  %-34s ; line %d\n", (unsigned long long)(sga::kBase + 4 * i), img.text[i],
                            rv64::disasm(img.text[i], sga::kBase + 4 * i).c_str(), img.text_lines[i]);
        std::printf("singular-as: %s -> %s (%zu insns, %zu data words, tohost=0x%llx)\n", in.c_str(), out.c_str(), img.text.size(),
                    img.data.size(), (unsigned long long)img.tohost_addr);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "singular-as: error: " << e.what() << "\n";
        return 1;
    }
}
