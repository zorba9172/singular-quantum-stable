// SINGULAR Ω v24 causal_machine — EMULATED Xomegacausal runtime for the ARM64 host path.
// QBE IL from `singular-as --emit-qbe` calls omega_rt_<op>(a, b). On Apple ARM64 there is
// no Ω hardware: each call executes the reference semantics in software (omega_ref.hpp).
// The host main runs sga_main and prints the final integer registers x1..x29 written by
// the program in the same format as `omega-cosim regs`, so results can be compared with Spike.
#include <cstdint>
#include <cstdio>

#include "omega_ref.hpp"

namespace {
omega_ref::Table g_table;
uint64_t call(uint32_t f3, uint64_t a, uint64_t b) { return omega_ref::execute(g_table, f3, a, b); }
}  // namespace

extern "C" {
uint64_t omega_rt_track(uint64_t a, uint64_t b) { return call(OMEGA_F3_TRACK, a, b); }
uint64_t omega_rt_dirty(uint64_t a, uint64_t b) { return call(OMEGA_F3_DIRTY, a, b); }
uint64_t omega_rt_prop(uint64_t a, uint64_t) { return call(OMEGA_F3_PROP, a, 0); }
uint64_t omega_rt_valid(uint64_t a, uint64_t b) { return call(OMEGA_F3_VALID, a, b); }
uint64_t omega_rt_clean(uint64_t a, uint64_t) { return call(OMEGA_F3_CLEAN, a, 0); }
uint64_t sga_main(uint64_t* regs);
}

int main() {
    uint64_t regs[32] = {};
    uint64_t a0 = sga_main(regs);
    for (int r = 1; r < 30; ++r) std::printf("x%d 0x%016llx\n", r, static_cast<unsigned long long>(regs[r]));
    std::fprintf(stderr, "EMULATED_EXIT a0=%llu\n", static_cast<unsigned long long>(a0));
    return static_cast<int>(a0 & 0xFF);
}
