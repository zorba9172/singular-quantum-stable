// SINGULAR Ω v24 causal_machine — minimal ELF64 (EM_RISCV) writer and reader.
// Writer: one RWX PT_LOAD segment at 0x80000000 + .text/.symtab/.strtab/.shstrtab
// sections (the symbol table lets Spike's HTIF locate tohost/fromhost).
// Reader: loads PT_LOAD segments and the symbol table (used by objdump and refsim).
#pragma once
#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace elf64 {

#pragma pack(push, 1)
struct Ehdr { uint8_t ident[16]; uint16_t type, machine; uint32_t version; uint64_t entry, phoff, shoff; uint32_t flags; uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx; };
struct Phdr { uint32_t type, flags; uint64_t offset, vaddr, paddr, filesz, memsz, align; };
struct Shdr { uint32_t name, type; uint64_t flags, addr, offset, size; uint32_t link, info; uint64_t addralign, entsize; };
struct Sym { uint32_t name; uint8_t info, other; uint16_t shndx; uint64_t value, size; };
#pragma pack(pop)

constexpr uint16_t kEmRiscv = 243;

struct Program {
    uint64_t entry = 0, base = 0;
    std::vector<uint8_t> bytes;  // contiguous image starting at base
    std::map<std::string, uint64_t> syms;
};

inline void put(std::vector<uint8_t>& out, const void* p, size_t n) {
    const uint8_t* b = static_cast<const uint8_t*>(p);
    out.insert(out.end(), b, b + n);
}

inline void write(const std::string& path, const Program& prog) {
    std::vector<uint8_t> strtab(1, 0), symtab, shstr(1, 0);
    Sym null_sym{};
    put(symtab, &null_sym, sizeof null_sym);
    for (const auto& kv : prog.syms) {
        Sym s{};
        s.name = static_cast<uint32_t>(strtab.size());
        put(strtab, kv.first.c_str(), kv.first.size() + 1);
        s.info = (1u << 4) | 0u;  // STB_GLOBAL, STT_NOTYPE
        s.shndx = 1;
        s.value = kv.second;
        put(symtab, &s, sizeof s);
    }
    auto shname = [&](const char* n) {
        uint32_t off = static_cast<uint32_t>(shstr.size());
        put(shstr, n, std::strlen(n) + 1);
        return off;
    };
    uint32_t n_text = shname(".text"), n_sym = shname(".symtab"), n_str = shname(".strtab"), n_shs = shname(".shstrtab");
    const uint64_t seg_off = 0x1000;
    uint64_t sym_off = seg_off + prog.bytes.size();
    sym_off = (sym_off + 7) & ~7ull;
    uint64_t str_off = sym_off + symtab.size();
    uint64_t shs_off = str_off + strtab.size();
    uint64_t sh_off = (shs_off + shstr.size() + 7) & ~7ull;

    Ehdr eh{};
    const uint8_t ident[16] = {0x7F, 'E', 'L', 'F', 2, 1, 1, 0};
    std::memcpy(eh.ident, ident, 16);
    eh.type = 2;  // ET_EXEC
    eh.machine = kEmRiscv;
    eh.version = 1;
    eh.entry = prog.entry;
    eh.phoff = sizeof(Ehdr);
    eh.shoff = sh_off;
    eh.ehsize = sizeof(Ehdr);
    eh.phentsize = sizeof(Phdr);
    eh.phnum = 1;
    eh.shentsize = sizeof(Shdr);
    eh.shnum = 5;
    eh.shstrndx = 4;
    Phdr ph{1, 7, seg_off, prog.base, prog.base, prog.bytes.size(), prog.bytes.size(), 0x1000};
    Shdr sh[5] = {};
    sh[1] = {n_text, 1, 0x7, prog.base, seg_off, prog.bytes.size(), 0, 0, 4, 0};
    sh[2] = {n_sym, 2, 0, 0, sym_off, symtab.size(), 3, 1, 8, sizeof(Sym)};
    sh[3] = {n_str, 3, 0, 0, str_off, strtab.size(), 0, 0, 1, 0};
    sh[4] = {n_shs, 3, 0, 0, shs_off, shstr.size(), 0, 0, 1, 0};

    std::vector<uint8_t> out;
    put(out, &eh, sizeof eh);
    put(out, &ph, sizeof ph);
    out.resize(seg_off, 0);
    put(out, prog.bytes.data(), prog.bytes.size());
    out.resize(sym_off, 0);
    put(out, symtab.data(), symtab.size());
    put(out, strtab.data(), strtab.size());
    put(out, shstr.data(), shstr.size());
    out.resize(sh_off, 0);
    put(out, sh, sizeof sh);
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(out.size()));
    if (!f) throw std::runtime_error("cannot write " + path);
}

inline Program read(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + path);
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (d.size() < sizeof(Ehdr) || std::memcmp(d.data(), "\x7F" "ELF", 4) != 0 || d[4] != 2) throw std::runtime_error("not an ELF64 file");
    Ehdr eh;
    std::memcpy(&eh, d.data(), sizeof eh);
    if (eh.machine != kEmRiscv) throw std::runtime_error("not a RISC-V ELF");
    auto at = [&](uint64_t off, size_t n) {
        if (off + n > d.size()) throw std::runtime_error("truncated ELF");
        return d.data() + off;
    };
    Program p;
    p.entry = eh.entry;
    uint64_t lo = UINT64_MAX, hi = 0;
    std::vector<Phdr> loads;
    for (unsigned i = 0; i < eh.phnum; ++i) {
        Phdr ph;
        std::memcpy(&ph, at(eh.phoff + i * sizeof(Phdr), sizeof ph), sizeof ph);
        if (ph.type != 1) continue;
        loads.push_back(ph);
        lo = std::min(lo, ph.vaddr);
        hi = std::max(hi, ph.vaddr + ph.memsz);
    }
    if (loads.empty() || hi - lo > (64u << 20)) throw std::runtime_error("no loadable segment or image too large");
    p.base = lo;
    p.bytes.assign(hi - lo, 0);
    for (const auto& ph : loads) std::memcpy(p.bytes.data() + (ph.vaddr - lo), at(ph.offset, ph.filesz), ph.filesz);
    for (unsigned i = 0; i < eh.shnum; ++i) {
        Shdr sh;
        std::memcpy(&sh, at(eh.shoff + i * sizeof(Shdr), sizeof sh), sizeof sh);
        if (sh.type != 2 || sh.entsize != sizeof(Sym)) continue;
        Shdr str;
        std::memcpy(&str, at(eh.shoff + sh.link * sizeof(Shdr), sizeof str), sizeof str);
        for (uint64_t k = 1; k < sh.size / sizeof(Sym); ++k) {
            Sym s;
            std::memcpy(&s, at(sh.offset + k * sizeof(Sym), sizeof s), sizeof s);
            if (s.name >= str.size) continue;
            const char* nm = reinterpret_cast<const char*>(at(str.offset + s.name, 1));
            p.syms[std::string(nm, strnlen(nm, str.size - s.name))] = s.value;
        }
    }
    return p;
}

// Bytes of the section named `name` (works for relocatable objects, e.g. TinyCC -c output).
inline std::vector<uint8_t> read_section(const std::string& path, const std::string& name) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + path);
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (d.size() < sizeof(Ehdr) || std::memcmp(d.data(), "\x7F" "ELF", 4) != 0 || d[4] != 2) throw std::runtime_error("not an ELF64 file");
    Ehdr eh;
    std::memcpy(&eh, d.data(), sizeof eh);
    auto sh_at = [&](unsigned i) {
        Shdr s;
        if (eh.shoff + (i + 1) * sizeof(Shdr) > d.size()) throw std::runtime_error("truncated ELF");
        std::memcpy(&s, d.data() + eh.shoff + i * sizeof(Shdr), sizeof s);
        return s;
    };
    Shdr strs = sh_at(eh.shstrndx);
    for (unsigned i = 0; i < eh.shnum; ++i) {
        Shdr s = sh_at(i);
        if (strs.offset + s.name >= d.size()) continue;
        if (name == reinterpret_cast<const char*>(d.data() + strs.offset + s.name)) {
            if (s.offset + s.size > d.size()) throw std::runtime_error("truncated section");
            return std::vector<uint8_t>(d.begin() + static_cast<long>(s.offset), d.begin() + static_cast<long>(s.offset + s.size));
        }
    }
    throw std::runtime_error("section " + name + " not found in " + path);
}

} // namespace elf64
