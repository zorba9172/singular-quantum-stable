// SINGULAR Ω v24 — Verilator testbench for omega_causal_unit (N = OMEGA_ENTRIES = 64).
//   tb_omega trace <file>       replay an Ω operand trace produced by `omega-cosim diff` (Spike-checked)
//   tb_omega random <seed> <n>  random instructions vs the C++ reference (omega_ref.hpp), full table
//                               comparison after every instruction, random result back-pressure and resets
// Prints a one-line summary; exit code 0 only when every check passed.
#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <sstream>
#include <string>

#include "Vomega_causal_unit.h"
#include "verilated.h"
#include "../tools/omega_ref.hpp"

namespace {

struct Bench {
    Vomega_causal_unit top;
    uint64_t cycles = 0;
    void tick() {
        top.clk = 0; top.eval();
        top.clk = 1; top.eval();
        ++cycles;
    }
    void reset() {
        top.rst_n = 0; top.issue_valid = 0; top.result_ready = 0;
        tick();  // synchronous reset: one clock edge with rst_n low
        top.rst_n = 1; top.eval();
    }
    bool table_equals(const omega_ref::Table& t) const {
        for (unsigned i = 0; i < OMEGA_ENTRIES; ++i) {
            if (top.version_q[i] != t.version[i]) return false;
            if (((top.dirty_q >> i) & 1) != t.dirty[i]) return false;
            uint64_t d = (static_cast<uint64_t>(top.deps_q[2 * i + 1]) << 32) | top.deps_q[2 * i];
            if (d != t.deps[i]) return false;
        }
        return true;
    }
    struct Outcome { bool accepted, illegal; uint64_t data; unsigned rd; unsigned latency; };
    // Issues one instruction; waits for the result with `stall` cycles of result_ready back-pressure.
    Outcome issue(uint32_t insn, uint64_t a, uint64_t b, unsigned priv, unsigned en, unsigned stall) {
        Outcome o{};
        top.issue_valid = 1; top.issue_insn = insn; top.issue_rs1 = a; top.issue_rs2 = b;
        top.issue_priv = priv; top.momegaen = en; top.result_ready = 0;
        top.eval();
        o.accepted = top.issue_accept; o.illegal = top.issue_illegal;
        if (!top.issue_ready) { std::fprintf(stderr, "unit not ready at issue\n"); std::exit(3); }
        tick();
        top.issue_valid = 0;
        if (!o.accepted) return o;
        unsigned waited = 0;
        while (!top.result_valid) {
            tick();
            if (++waited > 4 * OMEGA_ENTRIES) { std::fprintf(stderr, "result timeout\n"); std::exit(3); }
        }
        o.latency = waited + 1;
        for (unsigned s = 0; s < stall; ++s) {
            tick();
            if (!top.result_valid) { std::fprintf(stderr, "result dropped under back-pressure\n"); std::exit(3); }
        }
        o.data = top.result_data; o.rd = top.result_rd;
        top.result_ready = 1; tick(); top.result_ready = 0; top.eval();
        return o;
    }
};

int run_trace(const char* path) {
    std::ifstream f(path);
    if (!f) { std::fprintf(stderr, "cannot open %s\n", path); return 2; }
    Bench b; b.reset();
    std::string line;
    unsigned long n = 0, bad = 0, traps = 0, max_lat = 0, prop_lat = 0, props = 0;
    while (std::getline(f, line)) {
        std::istringstream in(line);
        std::string insn_s, a_s, b_s, res_s;
        unsigned priv, trapped; unsigned long long en;
        if (!(in >> insn_s >> a_s >> b_s >> priv >> en >> trapped >> res_s)) continue;
        uint32_t insn = static_cast<uint32_t>(std::stoul(insn_s, nullptr, 16));
        auto o = b.issue(insn, std::stoull(a_s, nullptr, 16), std::stoull(b_s, nullptr, 16), priv, static_cast<unsigned>(en), 0);
        uint64_t exp = std::stoull(res_s, nullptr, 16);
        bool ok = trapped ? (o.illegal && !o.accepted) : (o.accepted && !o.illegal && o.data == exp && o.rd == ((insn >> 7) & 31));
        traps += trapped;
        if (o.accepted) { max_lat = std::max<unsigned long>(max_lat, o.latency); if (((insn >> 12) & 7) == OMEGA_F3_PROP) { prop_lat += o.latency; ++props; } }
        if (!ok && bad++ < 5) std::fprintf(stderr, "MISMATCH insn=%08x exp=%016llx got=%016llx acc=%d ill=%d\n", insn, (unsigned long long)exp, (unsigned long long)o.data, o.accepted, o.illegal);
        ++n;
    }
    std::printf("RTL_TRACE file=%s checked=%lu traps=%lu mismatches=%lu cycles=%llu max_latency=%lu prop_latency_sum=%lu props=%lu RESULT=%s\n", path, n, traps, bad,
                (unsigned long long)b.cycles, max_lat, prop_lat, props, bad == 0 && n > 0 ? "PASS" : "FAIL");
    return bad == 0 && n > 0 ? 0 : 1;
}

int run_random(unsigned long long seed, unsigned long count) {
    std::mt19937_64 rng(seed);
    auto pick = [&](uint64_t lo, uint64_t hi) { return std::uniform_int_distribution<uint64_t>(lo, hi)(rng); };
    Bench b; b.reset();
    omega_ref::Table t;
    unsigned long bad = 0, accepted = 0, illegal = 0, resets = 0, props = 0, max_lat = 0, lat_sum = 0;
    for (unsigned long i = 0; i < count; ++i) {
        if (pick(0, 999) == 0) { b.reset(); t.reset(); ++resets; if (!b.table_equals(t)) ++bad; }
        uint32_t insn;
        if (pick(0, 19) == 0) insn = static_cast<uint32_t>(pick(0, 0xFFFFFFFF)) | 0x7B;  // arbitrary custom-3 word
        else {
            const auto& d = OMEGA_INSNS[pick(0, OMEGA_INSN_COUNT - 1)];
            insn = d.match | (static_cast<uint32_t>(pick(0, 31)) << 7) | (static_cast<uint32_t>(pick(0, 31)) << 15) |
                   (d.rs2_zero && pick(0, 9) ? 0u : static_cast<uint32_t>(pick(0, 31)) << 20);
        }
        unsigned span = pick(0, 1) ? 8 : OMEGA_ENTRIES;
        auto operand = [&]() -> uint64_t {
            unsigned k = static_cast<unsigned>(pick(0, 9));
            if (k < 7) return pick(0, span - 1);
            if (k < 9) return pick(0, 3);
            return pick(0, 2) ? pick(OMEGA_ENTRIES, 1000) : pick(0, UINT64_MAX);
        };
        uint64_t a = operand(), bb = operand();
        unsigned priv = static_cast<unsigned>(pick(0, 3)), en = static_cast<unsigned>(pick(0, 3));
        if (pick(0, 1)) priv = 3;
        omega_ref::Decoded d = omega_ref::decode(insn);
        bool ok_ref = d.legal && priv != 2 && omega_ref::permitted(static_cast<omega_ref::Priv>(priv), en);
        auto o = b.issue(insn, a, bb, priv, en, static_cast<unsigned>(pick(0, 3)));
        bool ok = (o.accepted == ok_ref) && (o.illegal == (d.ours && !ok_ref));
        if (ok_ref) {
            uint64_t exp = omega_ref::execute(t, d.funct3, a, bb);
            ok = ok && o.data == exp && o.rd == d.rd;
            ++accepted;
            if (d.funct3 == OMEGA_F3_PROP) { ++props; lat_sum += o.latency; if (o.latency > max_lat) max_lat = o.latency; }
        } else {
            illegal += o.illegal;
        }
        ok = ok && b.table_equals(t);
        if (!ok && bad++ < 5) std::fprintf(stderr, "MISMATCH #%lu insn=%08x a=%llx b=%llx priv=%u en=%u data=%016llx\n", i, insn, (unsigned long long)a, (unsigned long long)bb, priv, en, (unsigned long long)o.data);
    }
    std::printf("RTL_RANDOM seed=%llu ops=%lu accepted=%lu illegal_trapped=%lu resets=%lu props=%lu prop_latency_avg=%.2f prop_latency_max=%lu "
                "mismatches=%lu cycles=%llu RESULT=%s\n", seed, count, accepted, illegal, resets, props, props ? double(lat_sum) / props : 0.0,
                max_lat, bad, (unsigned long long)b.cycles, bad == 0 ? "PASS" : "FAIL");
    return bad == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    if (argc == 3 && std::string(argv[1]) == "trace") return run_trace(argv[2]);
    if (argc == 4 && std::string(argv[1]) == "random") return run_random(std::strtoull(argv[2], nullptr, 0), std::strtoul(argv[3], nullptr, 0));
    std::fprintf(stderr, "usage: tb_omega trace <file> | random <seed> <n>\n");
    return 2;
}
