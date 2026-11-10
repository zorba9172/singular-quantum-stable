// SINGULAR Ω v24 causal_machine — ISA generator.
// Reads isa/omega_isa.yaml (single source of truth) and emits:
//   <out>/omega_isa_gen.h     C/C++ constants + instruction table (tools, Spike)
//   <out>/omega_isa_pkg.sv    SystemVerilog package (RTL)
//   --markdown                encoding table for the spec document (stdout)
// --check compares freshly generated text with the files on disk and fails on drift.
#include "mini_yaml.hpp"

#include <cstdio>
#include <iostream>

namespace {

struct Insn {
    std::string mnemonic, symbol, upper, semantics, operands;
    uint32_t funct3 = 0, match = 0, mask = 0;
    bool rs2_zero = false;
};

struct Isa {
    std::string name, version, base;
    uint32_t opcode = 0, funct7 = 0, entries = 0, version_bits = 0, csr_addr = 0, s_bit = 0, u_bit = 0, err_bit = 0;
    std::vector<std::pair<std::string, uint32_t>> errors;
    std::vector<Insn> insns;
};

std::string hex32(uint32_t v) {
    char b[16];
    std::snprintf(b, sizeof b, "0x%08X", v);
    return b;
}

Isa load(const std::string& path) {
    omega_yaml::Node root = omega_yaml::parse_file(path);
    Isa isa;
    const auto& h = root.at("isa");
    isa.name = h.str("name");
    isa.version = h.str("version");
    isa.base = h.str("base");
    isa.opcode = static_cast<uint32_t>(h.u64("opcode"));
    isa.funct7 = static_cast<uint32_t>(h.u64("funct7"));
    if (h.u64("xlen") != 64 || h.str("format") != "R") throw std::runtime_error("only RV64 R-type is supported");
    if ((isa.opcode & 0x7F) != isa.opcode || (isa.opcode & 3) != 3) throw std::runtime_error("opcode must be a 7-bit 32-bit-length opcode");
    if (isa.opcode != 0x0B && isa.opcode != 0x2B && isa.opcode != 0x5B && isa.opcode != 0x7B)
        throw std::runtime_error("opcode must be one of the RISC-V custom-0..3 opcodes");
    if (isa.funct7 > 0x7F) throw std::runtime_error("funct7 out of range");
    const auto& st = root.at("state");
    isa.entries = static_cast<uint32_t>(st.u64("entries"));
    isa.version_bits = static_cast<uint32_t>(st.u64("version_bits"));
    if (isa.entries == 0 || isa.entries > st.u64("deps_bits") || st.u64("deps_bits") != 64)
        throw std::runtime_error("entries must be in [1, 64] with 64-bit dependency masks");
    if (isa.version_bits != 32) throw std::runtime_error("version_bits must be 32");
    const auto& csr = root.at("csr");
    isa.csr_addr = static_cast<uint32_t>(csr.u64("addr"));
    if (isa.csr_addr < 0x7C0 || isa.csr_addr > 0x7FF) throw std::runtime_error("csr must be in the custom M-mode R/W range 0x7C0..0x7FF");
    isa.s_bit = static_cast<uint32_t>(csr.u64("s_enable_bit"));
    isa.u_bit = static_cast<uint32_t>(csr.u64("u_enable_bit"));
    isa.err_bit = static_cast<uint32_t>(root.at("result").u64("error_flag_bit"));
    for (const auto& e : root.at("errors").list) isa.errors.emplace_back(e.str("name"), static_cast<uint32_t>(e.u64("code")));
    uint32_t used_f3 = 0;
    for (const auto& n : root.at("instructions").list) {
        Insn in;
        in.mnemonic = n.str("mnemonic");
        in.symbol = n.str("symbol");
        in.funct3 = static_cast<uint32_t>(n.u64("funct3"));
        in.rs2_zero = n.u64("rs2_must_be_zero") != 0;
        in.semantics = n.str("semantics");
        in.operands = n.str("operands");
        if (in.funct3 > 7) throw std::runtime_error(in.mnemonic + ": funct3 out of range");
        if (used_f3 & (1u << in.funct3)) throw std::runtime_error(in.mnemonic + ": funct3 collision");
        used_f3 |= 1u << in.funct3;
        if (in.mnemonic.rfind("omega.", 0) != 0) throw std::runtime_error(in.mnemonic + ": mnemonic must start with omega.");
        for (char c : in.mnemonic.substr(6)) in.upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        in.match = isa.opcode | (in.funct3 << 12) | (isa.funct7 << 25);
        in.mask = 0x7Fu | (0x7u << 12) | (0x7Fu << 25) | (in.rs2_zero ? (0x1Fu << 20) : 0u);
        isa.insns.push_back(in);
    }
    return isa;
}

std::string gen_header(const Isa& isa) {
    std::ostringstream o;
    o << "/* GENERATED from omega_isa.yaml by omega_isagen. Do not edit. */\n"
      << "#ifndef OMEGA_ISA_GEN_H\n#define OMEGA_ISA_GEN_H\n#include <stdint.h>\n"
      << "#define OMEGA_ISA_NAME \"" << isa.name << "\"\n"
      << "#define OMEGA_ISA_VERSION \"" << isa.version << "\"\n"
      << "#define OMEGA_OPCODE " << hex32(isa.opcode) << "u\n"
      << "#define OMEGA_FUNCT7 " << hex32(isa.funct7) << "u\n"
      << "#define OMEGA_ENTRIES " << isa.entries << "u\n"
      << "#define OMEGA_VERSION_BITS " << isa.version_bits << "u\n"
      << "#define OMEGA_CSR_MOMEGAEN " << hex32(isa.csr_addr) << "u\n"
      << "#define OMEGA_CSR_S_BIT " << isa.s_bit << "u\n"
      << "#define OMEGA_CSR_U_BIT " << isa.u_bit << "u\n"
      << "#define OMEGA_ERR_FLAG (1ull << " << isa.err_bit << ")\n";
    for (const auto& e : isa.errors) o << "#define OMEGA_" << e.first << " " << e.second << "u\n";
    for (const auto& in : isa.insns)
        o << "#define OMEGA_F3_" << in.upper << " " << in.funct3 << "u\n"
          << "#define MATCH_OMEGA_" << in.upper << " " << hex32(in.match) << "u\n"
          << "#define MASK_OMEGA_" << in.upper << " " << hex32(in.mask) << "u\n";
    o << "#define OMEGA_INSN_COUNT " << isa.insns.size() << "\n"
      << "typedef struct { const char* mnemonic; const char* symbol; uint32_t funct3; uint32_t match; uint32_t mask; uint32_t rs2_zero; } omega_insn_desc_t;\n"
      << "static const omega_insn_desc_t OMEGA_INSNS[OMEGA_INSN_COUNT] = {\n";
    for (const auto& in : isa.insns)
        o << "  {\"" << in.mnemonic << "\", \"" << in.symbol << "\", " << in.funct3 << "u, " << hex32(in.match) << "u, "
          << hex32(in.mask) << "u, " << (in.rs2_zero ? 1 : 0) << "u},\n";
    o << "};\n#endif\n";
    return o.str();
}

std::string gen_sv(const Isa& isa) {
    std::ostringstream o;
    o << "// GENERATED from omega_isa.yaml by omega_isagen. Do not edit.\n"
      << "package omega_isa_pkg;\n"
      << "  localparam logic [6:0] OMEGA_OPCODE = 7'h" << std::hex << isa.opcode << ";\n"
      << "  localparam logic [6:0] OMEGA_FUNCT7 = 7'h" << isa.funct7 << ";\n" << std::dec
      << "  localparam int unsigned OMEGA_ENTRIES = " << isa.entries << ";\n"
      << "  localparam int unsigned OMEGA_CSR_S_BIT = " << isa.s_bit << ";\n"
      << "  localparam int unsigned OMEGA_CSR_U_BIT = " << isa.u_bit << ";\n";
    for (const auto& e : isa.errors) o << "  localparam logic [62:0] OMEGA_" << e.first << " = 63'd" << e.second << ";\n";
    for (const auto& in : isa.insns)
        o << "  localparam logic [2:0] OMEGA_F3_" << in.upper << " = 3'd" << in.funct3 << ";\n"
          << "  localparam logic OMEGA_RS2Z_" << in.upper << " = 1'b" << (in.rs2_zero ? 1 : 0) << ";\n";
    o << "endpackage\n";
    return o.str();
}

std::string gen_markdown(const Isa& isa) {
    std::ostringstream o;
    o << "| Mnemonic | Symbol | funct7 | funct3 | rs2 field | MATCH | MASK | Operands |\n|---|---|---|---|---|---|---|---|\n";
    for (const auto& in : isa.insns)
        o << "| `" << in.mnemonic << "` | " << in.symbol << " | `" << hex32(isa.funct7) << "` | " << in.funct3 << " | "
          << (in.rs2_zero ? "must be 0" : "register") << " | `" << hex32(in.match) << "` | `" << hex32(in.mask) << "` | " << in.operands << " |\n";
    return o.str();
}

std::string read_all(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool emit(const std::string& path, const std::string& text, bool check) {
    if (check) {
        bool same = read_all(path) == text;
        std::cout << (same ? "IN_SYNC " : "DRIFT   ") << path << "\n";
        return same;
    }
    std::ofstream f(path, std::ios::binary);
    f << text;
    if (!f) {
        std::cerr << "cannot write " << path << "\n";
        return false;
    }
    std::cout << "WROTE   " << path << "\n";
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: omega_isagen <omega_isa.yaml> (--markdown | [--check] <out_dir> [extra_header_path...])\n";
        return 2;
    }
    try {
        Isa isa = load(argv[1]);
        if (std::string(argv[2]) == "--markdown") {
            std::cout << gen_markdown(isa);
            return 0;
        }
        int a = 2;
        bool check = false;
        if (std::string(argv[a]) == "--check") {
            check = true;
            ++a;
        }
        if (a >= argc) return 2;
        std::string dir = argv[a++];
        std::string hdr = gen_header(isa);
        bool ok = emit(dir + "/omega_isa_gen.h", hdr, check);
        ok = emit(dir + "/omega_isa_pkg.sv", gen_sv(isa), check) && ok;
        for (; a < argc; ++a) ok = emit(argv[a], hdr, check) && ok;
        std::cout << "ISA " << isa.name << " " << isa.version << " insns=" << isa.insns.size() << " entries=" << isa.entries
                  << (check ? (ok ? " CHECK=PASS" : " CHECK=FAIL") : "") << "\n";
        return ok ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "omega_isagen: " << e.what() << "\n";
        return 1;
    }
}
