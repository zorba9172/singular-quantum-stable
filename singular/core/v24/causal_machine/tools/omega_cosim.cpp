// SINGULAR Ω v24 causal_machine — omega-cosim: reference-vs-Spike differential checker.
//   omega-cosim ref <elf>                         normalized commit trace of the reference machine
//   omega-cosim spike <spike.log>                 normalized commit trace parsed from `spike --log-commits`
//   omega-cosim diff <elf> <spike.log> [rtl.trace] compare both; optionally write the Ω operand trace
//                                                 (insn rs1 rs2 priv momegaen trapped expected) for the RTL bench
// A trace ends at the first executed `jal x0, 0` (the halt spin loop).
#include <fstream>
#include <iostream>
#include <sstream>

#include "elf64.hpp"
#include "rv64_ref.hpp"

namespace {

constexpr uint32_t kSpin = 0x0000006Fu;

std::vector<std::string> ref_trace(const std::string& elf, std::vector<rv64_ref::OmegaEvent>* events, uint64_t* instret) {
    elf64::Program p = elf64::read(elf);
    if (!p.syms.count("tohost")) throw std::runtime_error("ELF has no tohost symbol");
    rv64_ref::Machine m(p.base, p.bytes, p.syms["tohost"]);
    std::vector<std::string> out;
    for (uint64_t steps = 0; steps < 50'000'000; ++steps) {
        auto c = m.step();
        if (!c) continue;  // trapped: Spike does not log a commit for the faulting instruction
        out.push_back(rv64_ref::format(*c));
        if (c->insn == kSpin) {
            if (events) *events = m.omega_events;
            if (instret) *instret = m.instret;
            return out;
        }
    }
    throw std::runtime_error("reference machine did not reach the halt loop");
}

std::vector<std::string> spike_trace(const std::string& log) {
    std::ifstream f(log);
    if (!f) throw std::runtime_error("cannot open " + log);
    std::vector<std::string> out;
    std::string line;
    while (std::getline(f, line)) {
        // core   0: 3 0x0000000080000000 (0x02a00493) x9  0x000000000000002a [mem ...]
        if (line.rfind("core", 0) != 0) continue;
        std::istringstream in(line.substr(line.find(':') + 1));
        unsigned priv;
        std::string pcs, insns, tok;
        if (!(in >> priv >> pcs >> insns)) continue;
        uint64_t pc = std::stoull(pcs, nullptr, 16);
        if (pc < 0x80000000ull) continue;  // boot ROM
        uint32_t insn = static_cast<uint32_t>(std::stoul(insns.substr(1, insns.size() - 2), nullptr, 16));
        rv64_ref::Commit c{static_cast<uint8_t>(priv), pc, insn};
        while (in >> tok) {
            if (tok.size() >= 2 && tok[0] == 'x' && isdigit(static_cast<unsigned char>(tok[1]))) {
                std::string val;
                in >> val;
                int r = std::stoi(tok.substr(1));
                if (r != 0) {
                    c.rd = r;
                    c.value = std::stoull(val, nullptr, 16);
                }
            }
        }
        out.push_back(rv64_ref::format(c));
        if (insn == kSpin) return out;
    }
    throw std::runtime_error("spike log ended before the halt loop");
}

} // namespace

int main(int argc, char** argv) {
    try {
        std::string mode = argc > 1 ? argv[1] : "";
        if (mode == "ref" && argc == 3) {
            for (const auto& l : ref_trace(argv[2], nullptr, nullptr)) std::cout << l << "\n";
            return 0;
        }
        if (mode == "spike" && argc == 3) {
            for (const auto& l : spike_trace(argv[2])) std::cout << l << "\n";
            return 0;
        }
        if (mode == "diff" && (argc == 4 || argc == 5)) {
            std::vector<rv64_ref::OmegaEvent> ev;
            uint64_t instret = 0;
            auto r = ref_trace(argv[2], &ev, &instret);
            auto s = spike_trace(argv[3]);
            size_t n = std::min(r.size(), s.size()), first_bad = n;
            for (size_t i = 0; i < n; ++i)
                if (r[i] != s[i]) {
                    first_bad = i;
                    break;
                }
            bool same = first_bad == n && r.size() == s.size();
            size_t omega_ok = 0, omega_trap = 0;
            for (const auto& e : ev) (e.trapped ? omega_trap : omega_ok)++;
            std::cout << "COSIM ref_commits=" << r.size() << " spike_commits=" << s.size() << " omega_executed=" << omega_ok
                      << " omega_trapped=" << omega_trap << " ref_instret=" << instret << " RESULT=" << (same ? "MATCH" : "MISMATCH") << "\n";
            if (!same) {
                std::cout << "  first divergence at commit " << first_bad << "\n  ref:   " << (first_bad < r.size() ? r[first_bad] : "<end>")
                          << "\n  spike: " << (first_bad < s.size() ? s[first_bad] : "<end>") << "\n";
            }
            if (argc == 5) {
                std::ofstream t(argv[4]);
                for (const auto& e : ev) {
                    char b[160];
                    std::snprintf(b, sizeof b, "%08x %016llx %016llx %u %llu %u %016llx\n", e.insn, (unsigned long long)e.rs1,
                                  (unsigned long long)e.rs2, e.priv, (unsigned long long)e.momegaen, e.trapped ? 1u : 0u,
                                  (unsigned long long)e.result);
                    t << b;
                }
            }
            return same ? 0 : 1;
        }
        if (mode == "count" && argc >= 6 && (argc - 4) % 2 == 0) {  // count <elf> <log> <start> <end> [<start> <end>...]
            elf64::Program p = elf64::read(argv[2]);
            auto s = spike_trace(argv[3]);
            for (int k = 4; k + 1 < argc; k += 2) {
                if (!p.syms.count(argv[k]) || !p.syms.count(argv[k + 1])) throw std::runtime_error("unknown label");
                uint64_t lo = p.syms[argv[k]], hi = p.syms[argv[k + 1]], n = 0;
                for (const auto& l : s) {
                    uint64_t pc = std::stoull(l.substr(l.find("0x") + 2, 16), nullptr, 16);
                    n += (pc >= lo && pc < hi);
                }
                std::cout << "REGION " << argv[k] << ".." << argv[k + 1] << " dynamic_instructions=" << n << "\n";
            }
            return 0;
        }
        std::cerr << "usage: omega-cosim ref <elf> | spike <log> | diff <elf> <log> [rtl.trace] | count <elf> <log> <start> <end>...\n";
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "omega-cosim: " << e.what() << "\n";
        return 1;
    }
}
