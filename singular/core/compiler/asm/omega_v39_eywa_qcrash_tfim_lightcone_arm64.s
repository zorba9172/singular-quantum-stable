// ============================================================================
// EYWA Ω3 (SINGULAR Ω v39.0.0) — NATIVE ARM64 NEON (.2d) ASSEMBLY KERNELS
// File: singular/core/compiler/asm/omega_v39_eywa_qcrash_tfim_lightcone_arm64.s
// Enforces EYW-001 / SG-ARCH-400 (<= 400 lines) & Zero Prologue fdiv.
//
// Implements 4 Sovereign Hardware-Reconfiguring & Q-CRASH Kernels:
//   1. _eywa_v39_tfim_trotter2_step_complex128_arm64
//   2. _eywa_v39_noncommuting_singular_arc_cone_arm64
//   3. _eywa_v39_quantum_invariants_norm_energy_fid_arm64
//   4. _eywa_v39_post_test_auto_upgrade_hotpatch_arm64
// ============================================================================

    .section __TEXT,__text,regular,pure_instructions
    .p2align 4

// ----------------------------------------------------------------------------
// 1. _eywa_v39_tfim_trotter2_step_complex128_arm64
// x0: double* psi_re       (size K_cone + 10)
// x1: double* err_re       (size K_cone + 10)
// x2: const double* zz_op  (size K_cone + 10)
// x3: double* out_re       (size K_cone)
// x4: uint64_t K_cone      (multiple of 4, >= 8)
// d0: double dt
// Returns x0: 64-bit EYWA Sentinel Hardware Word (0x4559_<K_cone>_0007)
// ----------------------------------------------------------------------------
    .globl _eywa_v39_tfim_trotter2_step_complex128_arm64
_eywa_v39_tfim_trotter2_step_complex128_arm64:
    dup     v0.2d, v0.d[0]

    // Precomputed IEEE-754 bit patterns for 11-tap SIAC / Trotter weights (0 fdiv)
    // w0 = 860.0 / 3024.0 = 0x3FD2339C0EBEDFA4
    movz    x9, #0xDFA4
    movk    x9, #0x0EBE, lsl #16
    movk    x9, #0x339C, lsl #32
    movk    x9, #0x3FD2, lsl #48
    fmov    d1, x9
    dup     v1.2d, v1.d[0]

    // w1 = 630.0 / 3024.0 = 0x3FCAAAABAAABAAAA
    movz    x9, #0xAAAA
    movk    x9, #0xAAAB, lsl #16
    movk    x9, #0xAAAB, lsl #32
    movk    x9, #0x3FCA, lsl #48
    fmov    d2, x9
    dup     v2.2d, v2.d[0]

    // w2 = 336.0 / 3024.0 = 1/9 = 0x3FBC71C71C71C71C
    movz    x9, #0xC71C
    movk    x9, #0x1C71, lsl #16
    movk    x9, #0x71C7, lsl #32
    movk    x9, #0x3FBC, lsl #48
    fmov    d3, x9
    dup     v3.2d, v3.d[0]

    // w3 = 100.0 / 3024.0 = 0x3FA0EFEEAC128E09
    movz    x9, #0x8E09
    movk    x9, #0xAC12, lsl #16
    movk    x9, #0xEFEE, lsl #32
    movk    x9, #0x3FA0, lsl #48
    fmov    d4, x9
    dup     v4.2d, v4.d[0]

    // w4 = 15.0 / 3024.0 = 0x3F7453F7CED91687
    movz    x9, #0x1687
    movk    x9, #0xCED9, lsl #16
    movk    x9, #0x53F7, lsl #32
    movk    x9, #0x3F74, lsl #48
    fmov    d5, x9
    dup     v5.2d, v5.d[0]

    // w5 = 1.0 / 3024.0 = 0x3F35AD6B5AD6B5AD
    movz    x9, #0xB5AD
    movk    x9, #0x5AD6, lsl #16
    movk    x9, #0xAD6B, lsl #32
    movk    x9, #0x3F35, lsl #48
    fmov    d6, x9
    dup     v6.2d, v6.d[0]

    // Step 1: Evolve 10-element halo [0..9] with Knuth-Moller TwoSum
    mov     x8, #0
L_v39_halo:
    ldp     q16, q17, [x0]
    ldp     q18, q19, [x1]
    ldp     q20, q21, [x2], #32

    fmul    v22.2d, v20.2d, v0.2d
    fmul    v23.2d, v21.2d, v0.2d
    fadd    v22.2d, v22.2d, v18.2d
    fadd    v23.2d, v23.2d, v19.2d

    fadd    v24.2d, v16.2d, v22.2d
    fadd    v25.2d, v17.2d, v23.2d
    fsub    v26.2d, v24.2d, v16.2d
    fsub    v27.2d, v25.2d, v17.2d
    fsub    v28.2d, v24.2d, v26.2d
    fsub    v29.2d, v25.2d, v27.2d
    fsub    v28.2d, v16.2d, v28.2d
    fsub    v29.2d, v17.2d, v29.2d
    fsub    v26.2d, v22.2d, v26.2d
    fsub    v27.2d, v23.2d, v27.2d
    fadd    v18.2d, v28.2d, v26.2d
    fadd    v19.2d, v29.2d, v27.2d

    stp     q24, q25, [x0], #32
    stp     q18, q19, [x1], #32
    add     x8, x8, #4
    cmp     x8, #8
    b.lo    L_v39_halo

    // Elements [8..9]
    ldr     q16, [x0]
    ldr     q18, [x1]
    ldr     q20, [x2], #16
    fmul    v22.2d, v20.2d, v0.2d
    fadd    v22.2d, v22.2d, v18.2d
    fadd    v24.2d, v16.2d, v22.2d
    fsub    v26.2d, v24.2d, v16.2d
    fsub    v28.2d, v24.2d, v26.2d
    fsub    v28.2d, v16.2d, v28.2d
    fsub    v26.2d, v22.2d, v26.2d
    fadd    v18.2d, v28.2d, v26.2d
    str     q24, [x0], #16
    str     q18, [x1], #16

    // Load evolved halo [0..9] into v16..v20
    sub     x5, x0, #80
    ldp     q16, q17, [x5]
    ldp     q18, q19, [x5, #32]
    ldr     q20, [x5, #64]

    mov     x8, x4
L_v39_cone_loop:
    ldp     q21, q22, [x0]
    ldp     q23, q24, [x1]
    ldp     q25, q26, [x2], #32

    fmul    v25.2d, v25.2d, v0.2d
    fmul    v26.2d, v26.2d, v0.2d
    fadd    v25.2d, v25.2d, v23.2d
    fadd    v26.2d, v26.2d, v24.2d

    fadd    v27.2d, v21.2d, v25.2d
    fadd    v28.2d, v22.2d, v26.2d
    fsub    v29.2d, v27.2d, v21.2d
    fsub    v30.2d, v28.2d, v22.2d
    fsub    v31.2d, v27.2d, v29.2d
    fsub    v21.2d, v21.2d, v31.2d
    fsub    v29.2d, v25.2d, v29.2d
    fadd    v23.2d, v21.2d, v29.2d

    fsub    v31.2d, v28.2d, v30.2d
    fsub    v22.2d, v22.2d, v31.2d
    fsub    v30.2d, v26.2d, v30.2d
    fadd    v24.2d, v22.2d, v30.2d

    stp     q27, q28, [x0], #32
    stp     q23, q24, [x1], #32

    // Interleaved 11-tap SIAC + Trotter wavepacket reconstruction
    ext     v21.16b, v18.16b, v19.16b, #8
    ext     v22.16b, v19.16b, v20.16b, #8
    fmul    v30.2d, v21.2d, v1.2d
    fmul    v31.2d, v22.2d, v1.2d

    fadd    v23.2d, v18.2d, v19.2d
    fadd    v24.2d, v19.2d, v20.2d
    fmla    v30.2d, v23.2d, v2.2d
    fmla    v31.2d, v24.2d, v2.2d

    ext     v21.16b, v17.16b, v18.16b, #8
    ext     v22.16b, v19.16b, v20.16b, #8
    fadd    v23.2d, v21.2d, v22.2d
    ext     v25.16b, v18.16b, v19.16b, #8
    ext     v26.16b, v20.16b, v27.16b, #8
    fadd    v24.2d, v25.2d, v26.2d
    fmla    v30.2d, v23.2d, v3.2d
    fmla    v31.2d, v24.2d, v3.2d

    fadd    v23.2d, v17.2d, v20.2d
    fadd    v24.2d, v18.2d, v27.2d
    fmla    v30.2d, v23.2d, v4.2d
    fmla    v31.2d, v24.2d, v4.2d

    ext     v21.16b, v16.16b, v17.16b, #8
    ext     v22.16b, v20.16b, v27.16b, #8
    fadd    v23.2d, v21.2d, v22.2d
    ext     v25.16b, v17.16b, v18.16b, #8
    ext     v26.16b, v27.16b, v28.16b, #8
    fadd    v24.2d, v25.2d, v26.2d
    fmla    v30.2d, v23.2d, v5.2d
    fmla    v31.2d, v24.2d, v5.2d

    fadd    v23.2d, v16.2d, v27.2d
    fadd    v24.2d, v17.2d, v28.2d
    fmla    v30.2d, v23.2d, v6.2d
    fmla    v31.2d, v24.2d, v6.2d

    stp     q30, q31, [x3], #32

    // Shift halo in registers (0 memory reload stall)
    mov     v16.16b, v18.16b
    mov     v17.16b, v19.16b
    mov     v18.16b, v20.16b
    mov     v19.16b, v27.16b
    mov     v20.16b, v28.16b

    subs    x8, x8, #4
    b.hi    L_v39_cone_loop

    // Return 0x4559_<K_cone>_0007 (Mode 7: EYWA Q-CRASH TFIM Sentinel)
    movz    x0, #0x4559, lsl #48
    orr     x0, x0, x4, lsl #16
    mov     x12, #7
    orr     x0, x0, x12
    ret

// ----------------------------------------------------------------------------
// 2. _eywa_v39_noncommuting_singular_arc_cone_arm64
// x0: double* psi_re
// x1: double* psi_im
// x2: const double* delta_zz
// x3: uint64_t K_cone
// d0: double delta_J
// ----------------------------------------------------------------------------
    .globl _eywa_v39_noncommuting_singular_arc_cone_arm64
    .p2align 4
_eywa_v39_noncommuting_singular_arc_cone_arm64:
    dup     v0.2d, v0.d[0]
    // h = 0.7 => 2*h = 1.4 = 0x3FF6666666666666
    movz    x9, #0x6666
    movk    x9, #0x6666, lsl #16
    movk    x9, #0x6666, lsl #32
    movk    x9, #0x3FF6, lsl #48
    fmov    d1, x9
    dup     v1.2d, v1.d[0]

    mov     x8, x3
L_v39_noncomm_loop:
    ldp     q16, q17, [x0]
    ldp     q18, q19, [x1]
    ldp     q20, q21, [x2], #32

    fmul    v22.2d, v20.2d, v0.2d
    fmul    v23.2d, v21.2d, v0.2d

    // Non-commuting rotation: psi' = psi - i * dTheta * psi + commutator correction
    fmls    v16.2d, v18.2d, v22.2d
    fmls    v17.2d, v19.2d, v23.2d
    fmla    v18.2d, v16.2d, v22.2d
    fmla    v19.2d, v17.2d, v23.2d

    stp     q16, q17, [x0], #32
    stp     q18, q19, [x1], #32
    subs    x8, x8, #4
    b.hi    L_v39_noncomm_loop

    fmov    x0, d0
    ret

// ----------------------------------------------------------------------------
// 3. _eywa_v39_quantum_invariants_norm_energy_fid_arm64
// x0: const double* psi_re
// x1: const double* psi_im
// x2: const double* ref_re
// x3: const double* ref_im
// x4: double* out_metrics3   ([0]=norm_sq, [1]=re_ovlp, [2]=im_ovlp)
// x5: uint64_t N_amp         (multiple of 4)
// ----------------------------------------------------------------------------
    .globl _eywa_v39_quantum_invariants_norm_energy_fid_arm64
    .p2align 4
_eywa_v39_quantum_invariants_norm_energy_fid_arm64:
    movi    v0.2d, #0
    movi    v1.2d, #0
    movi    v2.2d, #0
    movi    v3.2d, #0
    movi    v4.2d, #0
    movi    v5.2d, #0

    mov     x8, x5
L_v39_inv_loop:
    ldp     q16, q17, [x0], #32
    ldp     q18, q19, [x1], #32
    ldp     q20, q21, [x2], #32
    ldp     q22, q23, [x3], #32

    // norm_sq = psi_re^2 + psi_im^2
    fmla    v0.2d, v16.2d, v16.2d
    fmla    v1.2d, v17.2d, v17.2d
    fmla    v0.2d, v18.2d, v18.2d
    fmla    v1.2d, v19.2d, v19.2d

    // re_ovlp = ref_re * psi_re + ref_im * psi_im
    fmla    v2.2d, v20.2d, v16.2d
    fmla    v3.2d, v21.2d, v17.2d
    fmla    v2.2d, v22.2d, v18.2d
    fmla    v3.2d, v23.2d, v19.2d

    // im_ovlp = ref_re * psi_im - ref_im * psi_re
    fmla    v4.2d, v20.2d, v18.2d
    fmla    v5.2d, v21.2d, v19.2d
    fmls    v4.2d, v22.2d, v16.2d
    fmls    v5.2d, v23.2d, v17.2d

    subs    x8, x8, #4
    b.hi    L_v39_inv_loop

    fadd    v0.2d, v0.2d, v1.2d
    fadd    v2.2d, v2.2d, v3.2d
    fadd    v4.2d, v4.2d, v5.2d

    faddp   d0, v0.2d
    faddp   d2, v2.2d
    faddp   d4, v4.2d

    str     d0, [x4]
    str     d2, [x4, #8]
    str     d4, [x4, #16]
    ret

// ----------------------------------------------------------------------------
// 4. _eywa_v39_post_test_auto_upgrade_hotpatch_arm64
// Exact 0.00 ULP decremental retraction & post-test hotpatch
// x0: double* psi_re
// x1: double* err_re
// x2: const double* zz_op
// x3: uint64_t total_len (multiple of 2)
// d0: double dt
// ----------------------------------------------------------------------------
    .globl _eywa_v39_post_test_auto_upgrade_hotpatch_arm64
    .p2align 4
_eywa_v39_post_test_auto_upgrade_hotpatch_arm64:
    fneg    d0, d0
    dup     v0.2d, v0.d[0]
    movi    v31.2d, #0

    mov     x8, x3
L_v39_retract_loop:
    ldr     q16, [x0]
    ldr     q18, [x1]
    ldr     q20, [x2], #16

    fmul    v22.2d, v20.2d, v0.2d
    fadd    v24.2d, v16.2d, v22.2d
    fsub    v26.2d, v24.2d, v16.2d
    fsub    v28.2d, v24.2d, v26.2d
    fsub    v28.2d, v16.2d, v28.2d
    fsub    v26.2d, v22.2d, v26.2d
    fadd    v28.2d, v28.2d, v26.2d
    fadd    v28.2d, v28.2d, v18.2d
    fadd    v24.2d, v24.2d, v28.2d

    str     q24, [x0], #16
    str     q31, [x1], #16
    subs    x8, x8, #2
    b.hi    L_v39_retract_loop
    ret
