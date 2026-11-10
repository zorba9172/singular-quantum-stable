#pragma once
// ============================================================================
// CYCRAFT Ω — The Computational Reality Language (`Code Reality.`)
// Layer: L2 Hardware & Arena Substrate (`cycraft_arena_and_map_jit.hpp`)
// Enforces Constitutional Rule `SG-ARCH-400` (<= 400 lines per file).
//   1. 64B-Aligned Atomic-Nanometric Causal Arena with Redzone Canaries
//   2. In-Memory ARM64 (`AArch64`) `MAP_JIT` Machine-Code Emitter (W^X)
//   3. Register VM Bytecode Instruction & Execution Trace Types
// ============================================================================

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#ifdef __APPLE__
#include <libkern/OSCacheControl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace cycraft::omega::runtime {

// ============================================================================
// 1. 64B-Aligned Atomic-Nanometric Causal Arena with Redzone Canaries
// ============================================================================
class CyCraftCausalArena {
public:
    static constexpr uint64_t kCanaryMagic = 0x435943524146545FULL; // "CYCRAFT_"

    explicit CyCraftCausalArena(size_t bytes = 4 * 1024 * 1024) : cap_(bytes) {
        base_ = static_cast<uint8_t*>(::aligned_alloc(64, cap_));
        if (base_) std::memset(base_, 0, cap_);
    }

    ~CyCraftCausalArena() {
        if (base_) std::free(base_);
    }

    double* alloc_state_block(size_t count) {
        size_t payload = ((count * sizeof(double)) + 63ULL) & ~63ULL;
        size_t total = 64ULL + payload + 64ULL;
        if (!base_ || offset_ + total > cap_) return nullptr;
        uint8_t* blk = base_ + offset_;
        offset_ += total;
        *reinterpret_cast<uint64_t*>(blk) = kCanaryMagic;
        *reinterpret_cast<uint64_t*>(blk + 64ULL + payload) = kCanaryMagic;
        guards_.push_back({blk, 64ULL + payload});
        return reinterpret_cast<double*>(blk + 64ULL);
    }

    bool verify_canaries() const {
        for (const auto& g : guards_) {
            if (*reinterpret_cast<const uint64_t*>(g.first) != kCanaryMagic) return false;
            if (*reinterpret_cast<const uint64_t*>(g.first + g.second) != kCanaryMagic) return false;
        }
        return true;
    }

private:
    uint8_t* base_ = nullptr;
    size_t cap_ = 0;
    size_t offset_ = 0;
    std::vector<std::pair<uint8_t*, size_t>> guards_;
};

// ============================================================================
// 2. In-Memory ARM64 (`AArch64`) `MAP_JIT` Native Machine-Code Emitter
// ============================================================================
class CyCraftArm64MapJit {
public:
    using ArcFmaKernelFn = void (*)(const float*, float*, uint64_t, float);
    using ArcRetractKernelFn = void (*)(const float*, float*, float*, uint64_t);

    CyCraftArm64MapJit() { init_jit_page(); }

    ~CyCraftArm64MapJit() {
        if (exec_page_) {
            ::munmap(exec_page_, page_bytes_);
            exec_page_ = nullptr;
        }
    }

    bool init_jit_page() {
#if defined(__APPLE__) && defined(__aarch64__)
        page_bytes_ = 16384;
        void* p = ::mmap(nullptr, page_bytes_,
                         PROT_READ | PROT_WRITE | PROT_EXEC,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_JIT,
                         -1, 0);
        if (p == MAP_FAILED || !p) return false;
        exec_page_ = static_cast<uint8_t*>(p);

        ::pthread_jit_write_protect_np(0);

        // Offset 0: AArch64 `jit_cycraft_arc_retract_f32`
        static const uint32_t kRetractCode[] = {
            0xB4000143u, // cbz x3, #40
            0x1E2703E2u, // fmov s2, wzr
            0xBD400020u, // ldr s0, [x1]
            0xBC404401u, // ldr s1, [x0], #4
            0x1E213800u, // fsub s0, s0, s1
            0xBC004420u, // str s0, [x1], #4
            0x1E226800u, // fmaxnm s0, s0, s2
            0xBC004440u, // str s0, [x2], #4
            0xF1000463u, // subs x3, x3, #1
            0x54FFFF21u, // b.ne -28
            0xD65F03C0u  // ret
        };
        std::memcpy(exec_page_, kRetractCode, sizeof(kRetractCode));

        // Offset 256: AArch64 `jit_cycraft_arc_fma_f32`
        static const uint32_t kFmaCode[] = {
            0xB40000E2u, // cbz x2, #28
            0xBC404401u, // ldr s1, [x0], #4
            0xBD400022u, // ldr s2, [x1]
            0x1F000822u, // fmadd s2, s1, s0, s2
            0xBC004422u, // str s2, [x1], #4
            0xF1000442u, // subs x2, x2, #1
            0x54FFFF61u, // b.ne -20
            0xD65F03C0u  // ret
        };
        std::memcpy(exec_page_ + 256, kFmaCode, sizeof(kFmaCode));

        ::pthread_jit_write_protect_np(1);
        ::sys_icache_invalidate(exec_page_, page_bytes_);

        retract_fn_ = reinterpret_cast<ArcRetractKernelFn>(exec_page_);
        fma_fn_ = reinterpret_cast<ArcFmaKernelFn>(exec_page_ + 256);
        wx_active_ = true;
        return true;
#else
        return false;
#endif
    }

    void run_arc_fma(const float* coeff, float* d_stress, uint64_t n, float d_temp) const {
        if (fma_fn_) fma_fn_(coeff, d_stress, n, d_temp);
        else {
            for (uint64_t i = 0; i < n; ++i) d_stress[i] += coeff[i] * d_temp;
        }
    }

    void run_arc_retract(const float* d_stress, float* stress, float* eff_stress, uint64_t n) const {
        if (retract_fn_) retract_fn_(d_stress, stress, eff_stress, n);
        else {
            for (uint64_t i = 0; i < n; ++i) {
                stress[i] -= d_stress[i];
                eff_stress[i] = std::max(0.0f, stress[i]);
            }
        }
    }

    uintptr_t page_address() const { return reinterpret_cast<uintptr_t>(exec_page_); }
    bool wx_active() const { return wx_active_; }

    std::string hex_preview() const {
        if (!exec_page_) return "UNAVAILABLE";
        const uint32_t* w = reinterpret_cast<const uint32_t*>(exec_page_ + 256);
        std::ostringstream oss;
        for (size_t i = 0; i < 8; ++i) {
            if (i > 0) oss << " ";
            oss << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << w[i];
        }
        return oss.str();
    }

private:
    uint8_t* exec_page_ = nullptr;
    size_t page_bytes_ = 0;
    bool wx_active_ = false;
    ArcRetractKernelFn retract_fn_ = nullptr;
    ArcFmaKernelFn fma_fn_ = nullptr;
};

// ============================================================================
// 3. Register Virtual Machine Bytecode & Execution Trace Structures
// ============================================================================
enum class CyVmOpcode : uint8_t {
    OP_LOAD_SYM = 1,
    OP_LOAD_CONST = 2,
    OP_ADD = 3,
    OP_SUB = 4,
    OP_MUL = 5,
    OP_DIV = 6,
    OP_STORE_SYM = 7,
    OP_EVOLVE_ADD = 8,
    OP_VERIFY_LAW_GE = 9,
    OP_VERIFY_LAW_LE = 10
};

struct CyVmInstruction {
    CyVmOpcode op = CyVmOpcode::OP_LOAD_CONST;
    int dst = 0;
    int s1 = 0;
    int s2 = 0;
    double imm = 0.0;
    std::string sym;

    std::string to_string() const {
        std::ostringstream oss;
        switch (op) {
            case CyVmOpcode::OP_LOAD_SYM: oss << "CY_LOAD_SYM    r" << dst << ", @" << sym; break;
            case CyVmOpcode::OP_LOAD_CONST: oss << "CY_LOAD_CONST  r" << dst << ", " << imm; break;
            case CyVmOpcode::OP_ADD: oss << "CY_ADD_F64     r" << dst << ", r" << s1 << ", r" << s2; break;
            case CyVmOpcode::OP_SUB: oss << "CY_SUB_F64     r" << dst << ", r" << s1 << ", r" << s2; break;
            case CyVmOpcode::OP_MUL: oss << "CY_MUL_F64     r" << dst << ", r" << s1 << ", r" << s2; break;
            case CyVmOpcode::OP_DIV: oss << "CY_DIV_F64     r" << dst << ", r" << s1 << ", r" << s2; break;
            case CyVmOpcode::OP_STORE_SYM: oss << "CY_STORE_SYM   @" << sym << ", r" << s1; break;
            case CyVmOpcode::OP_EVOLVE_ADD: oss << "CY_EVOLVE_ADD  @" << sym << ", r" << s1; break;
            case CyVmOpcode::OP_VERIFY_LAW_GE: oss << "CY_LAW_GE      r" << dst << ", @" << sym << " >= " << imm; break;
            case CyVmOpcode::OP_VERIFY_LAW_LE: oss << "CY_LAW_LE      r" << dst << ", @" << sym << " <= " << imm; break;
        }
        return oss.str();
    }
};

struct CyExecutionTrace {
    bool ok = true;
    std::vector<CyVmInstruction> bytecode;
    std::map<std::string, double> state_env;
    uintptr_t map_jit_page = 0;
    std::string map_jit_hex;
    bool canary_integrity_verified = true;

    double prog_a_elastic_stress_mpa = 0.0;
    double prog_a_retracted_stress_mpa = 0.0;
    double prog_a_retract_0log_err = 0.0;
    double prog_a_plastic_fallback_stress_mpa = 0.0;
    bool prog_a_guard_triggered_fallback = false;
    double prog_a_siac_jit_us = 0.0;
    double prog_a_dense_recompute_us = 0.0;

    double prog_b_initial_temp_K = 0.0;
    double prog_b_heating_rate_K_per_s = 0.0;
    double prog_b_final_temp_K = 0.0;
    double prog_b_reference_euler_temp_K = 0.0;
    double prog_b_optical_energy_in = 0.0;
    double prog_b_optical_energy_out = 0.0;
    double prog_b_parseval_energy_drift = 0.0;
    double prog_b_center_phase_rad = 0.0;
    bool prog_b_laws_satisfied = true;

    double prog_c_candidate1_dt = 0.0;
    double prog_c_candidate1_err = 0.0;
    bool prog_c_candidate1_committed = false;
    double prog_c_candidate2_adversarial_dt = 0.0;
    double prog_c_candidate2_err = 0.0;
    bool prog_c_candidate2_rejected_to_fallback = false;
    std::string prog_c_active_solver_after_fallback;
};

} // namespace cycraft::omega::runtime
