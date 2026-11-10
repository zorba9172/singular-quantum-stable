// ============================================================================
// SINGULAR Ω v36.0.0 — NATIVE ARM64 NEON (.2d) SYMPLECTIC-CYCLOTOMIC SINGULAR
// ARCS (Δ⁺/Δ⁻ × TwoSum × 7-Tap O(h^7) SIAC), ANISOTROPIC GRAPHENE-SiV⁻
// PHONON-SPIN BRIDGE & NON-ABELIAN SU(2) COUNTERDIABATIC HUBO AI TRIGGER
// AAPCS64 Compliant: Caller-saved v0..v7, v16..v31 only (Zero v8..v15 usage).
// Enforces Constitutional Rule `SG-ARCH-400` (<= 400 physical lines).
// ============================================================================

    .section __TEXT,__text,regular,pure_instructions
    .p2align 4

// 1. _omega_v36_symplectic_7tap_siac_arc_arm64
//    Fused 1-Pass Symplectic Leapfrog + Knuth TwoSum (Δ⁺) + 7-Tap Symmetric
//    SIAC Superconvergent Filter ([1, 12, 39, 40, 39, 12, 1] / 144.0).
//    Args:
//      x0: double* state_q      (length n_cone + 6, 64B aligned)
//      x1: double* err_q        (length n_cone + 6, 64B aligned)
//      x2: const double* mom_p  (length n_cone + 6, symplectic momentum p)
//      x3: double* out_siac7    (length n_cone, 64B aligned)
//      x4: uint64_t n_cone      (multiple of 8, >= 8)
//      d0: double dt_step       (symplectic step size dt)
//    Returns:
//      x0: uint64_t (6-mode hardware control word 0x5336...)
    .globl _omega_v36_symplectic_7tap_siac_arc_arm64
_omega_v36_symplectic_7tap_siac_arc_arm64:
    cbz     x4, L_symp7_done
    dup     v24.2d, v0.d[0]             // v24 = [dt, dt]
    movz    x8, #0x1C72, lsl #0
    movk    x8, #0x71C7, lsl #16
    movk    x8, #0x1C71, lsl #32
    movk    x8, #0x3FD1, lsl #48        // 40.0 / 144.0
    fmov    d25, x8
    dup     v25.2d, v25.d[0]
    movz    x8, #0x5555, lsl #0
    movk    x8, #0x5555, lsl #16
    movk    x8, #0x5555, lsl #32
    movk    x8, #0x3FD1, lsl #48        // 39.0 / 144.0
    fmov    d26, x8
    dup     v26.2d, v26.d[0]
    movk    x8, #0x3FB5, lsl #48        // 12.0 / 144.0 (shares low 48 bits 0x555555555555!)
    fmov    d27, x8
    dup     v27.2d, v27.d[0]
    movz    x8, #0xC71C, lsl #0
    movk    x8, #0x71C7, lsl #16
    movk    x8, #0x1C71, lsl #32
    movk    x8, #0x3F7C, lsl #48        // 1.0 / 144.0
    fmov    d28, x8
    dup     v28.2d, v28.d[0]

    // Prologue: update 6-element Halo [0..5] with Symplectic TwoSum (dq = p * dt)
    ldp     q16, q17, [x0]
    ldr     q18, [x0, #32]
    ldp     q2,  q3,  [x1]
    ldr     q4,  [x1, #32]
    ldp     q5,  q6,  [x2]
    ldr     q7,  [x2, #32]

    fmul    v5.2d, v5.2d, v24.2d
    fmul    v6.2d, v6.2d, v24.2d
    fmul    v7.2d, v7.2d, v24.2d
    fadd    v5.2d, v5.2d, v2.2d
    fadd    v6.2d, v6.2d, v3.2d
    fadd    v7.2d, v7.2d, v4.2d

    fadd    v19.2d, v16.2d, v5.2d
    fadd    v20.2d, v17.2d, v6.2d
    fadd    v21.2d, v18.2d, v7.2d
    fsub    v29.2d, v19.2d, v16.2d
    fsub    v30.2d, v20.2d, v17.2d
    fsub    v31.2d, v21.2d, v18.2d
    fsub    v2.2d,  v19.2d, v29.2d
    fsub    v3.2d,  v20.2d, v30.2d
    fsub    v4.2d,  v21.2d, v31.2d
    fsub    v2.2d,  v16.2d, v2.2d
    fsub    v3.2d,  v17.2d, v3.2d
    fsub    v4.2d,  v18.2d, v4.2d
    fsub    v5.2d,  v5.2d,  v29.2d
    fsub    v6.2d,  v6.2d,  v30.2d
    fsub    v7.2d,  v7.2d,  v31.2d
    fadd    v2.2d,  v2.2d,  v5.2d
    fadd    v3.2d,  v3.2d,  v6.2d
    fadd    v4.2d,  v4.2d,  v7.2d

    stp     q19, q20, [x0]
    str     q21, [x0, #32]
    stp     q2,  q3,  [x1]
    str     q4,  [x1, #32]
    mov     v16.16b, v19.16b
    mov     v17.16b, v20.16b
    mov     v18.16b, v21.16b

    add     x5, x0, #48                 // state_q + 6
    add     x6, x1, #48                 // err_q + 6
    add     x7, x2, #48                 // mom_p + 6
    mov     x9, x3                      // out_siac7
    lsr     x10, x4, #3
    cbz     x10, L_symp7_ret

    .p2align 4
L_symp7_vec8_loop:
    // Halo [s0..s5] is already held in v16..v18; load new 8 elements [s6..s13]
    ldp     q19, q20, [x5]              // s6..s9
    ldp     q21, q22, [x5, #32]         // s10..s13
    ldp     q2,  q3,  [x6]
    ldp     q4,  q5,  [x6, #32]
    ldp     q6,  q7,  [x7]
    ldp     q30, q31, [x7, #32]

    fmul    v6.2d,  v6.2d,  v24.2d
    fmul    v7.2d,  v7.2d,  v24.2d
    fmul    v30.2d, v30.2d, v24.2d
    fmul    v31.2d, v31.2d, v24.2d
    fadd    v2.2d,  v2.2d,  v6.2d
    fadd    v3.2d,  v3.2d,  v7.2d
    fadd    v4.2d,  v4.2d,  v30.2d
    fadd    v5.2d,  v5.2d,  v31.2d

    fadd    v6.2d,  v19.2d, v2.2d
    fadd    v7.2d,  v20.2d, v3.2d
    fadd    v30.2d, v21.2d, v4.2d
    fadd    v31.2d, v22.2d, v5.2d

    fsub    v0.2d,  v6.2d,  v19.2d
    fsub    v1.2d,  v7.2d,  v20.2d
    fsub    v23.2d, v6.2d,  v0.2d
    fsub    v29.2d, v7.2d,  v1.2d
    fsub    v23.2d, v19.2d, v23.2d
    fsub    v29.2d, v20.2d, v29.2d
    fsub    v2.2d,  v2.2d,  v0.2d
    fsub    v3.2d,  v3.2d,  v1.2d
    fadd    v23.2d, v23.2d, v2.2d
    fadd    v29.2d, v29.2d, v3.2d

    fsub    v0.2d,  v30.2d, v21.2d
    fsub    v1.2d,  v31.2d, v22.2d
    fsub    v2.2d,  v30.2d, v0.2d
    fsub    v3.2d,  v31.2d, v1.2d
    fsub    v2.2d,  v21.2d, v2.2d
    fsub    v3.2d,  v22.2d, v3.2d
    fsub    v4.2d,  v4.2d,  v0.2d
    fsub    v5.2d,  v5.2d,  v1.2d
    fadd    v4.2d,  v2.2d,  v4.2d
    fadd    v5.2d,  v3.2d,  v5.2d

    mov     v19.16b, v6.16b
    mov     v20.16b, v7.16b
    mov     v21.16b, v30.16b
    mov     v22.16b, v31.16b

    stp     q19, q20, [x5], #64
    stp     q21, q22, [x5, #-32]
    stp     q23, q29, [x6], #64
    stp     q4,  q5,  [x6, #-32]
    add     x7,  x7,  #64

    // Shifted views via `ext` for 7-tap stencil [1, 12, 39, 40, 39, 12, 1] / 144
    ext     v0.16b, v16.16b, v17.16b, #8    // [s1, s2]
    ext     v1.16b, v17.16b, v18.16b, #8    // [s3, s4]
    ext     v2.16b, v18.16b, v19.16b, #8    // [s5, s6]
    ext     v3.16b, v19.16b, v20.16b, #8    // [s7, s8]
    ext     v4.16b, v20.16b, v21.16b, #8    // [s9, s10]
    ext     v5.16b, v21.16b, v22.16b, #8    // [s11, s12]

    // Out[0..1] (v30) & Out[2..3] (v31) interleaved with pre-scaled 1/144 weights
    fadd    v6.2d,  v17.2d, v18.2d          // s2+s4, s3+s5
    fadd    v23.2d, v18.2d, v19.2d          // s4+s6, s5+s7
    fadd    v7.2d,  v0.2d,  v2.2d           // s1+s5, s2+s6
    fadd    v29.2d, v1.2d,  v3.2d           // s3+s7, s4+s8
    fadd    v30.2d, v16.2d, v19.2d          // s0+s6, s1+s7
    fadd    v31.2d, v17.2d, v20.2d          // s2+s8, s3+s9
    fmul    v30.2d, v30.2d, v28.2d
    fmul    v31.2d, v31.2d, v28.2d
    fmla    v30.2d, v1.2d,  v25.2d
    fmla    v31.2d, v2.2d,  v25.2d
    fmla    v30.2d, v6.2d,  v26.2d
    fmla    v31.2d, v23.2d, v26.2d
    fmla    v30.2d, v7.2d,  v27.2d
    fmla    v31.2d, v29.2d, v27.2d

    stp     q30, q31, [x9], #64

    // Out[4..5] (v30) & Out[6..7] (v31) interleaved with pre-scaled 1/144 weights
    fadd    v6.2d,  v19.2d, v20.2d          // s6+s8, s7+s9
    fadd    v23.2d, v20.2d, v21.2d          // s8+s10, s9+s11
    fadd    v7.2d,  v2.2d,  v4.2d           // s5+s9, s6+s10
    fadd    v29.2d, v3.2d,  v5.2d           // s7+s11, s8+s12
    fadd    v30.2d, v18.2d, v21.2d          // s4+s10, s5+s11
    fadd    v31.2d, v19.2d, v22.2d          // s6+s12, s7+s13
    fmul    v30.2d, v30.2d, v28.2d
    fmul    v31.2d, v31.2d, v28.2d
    fmla    v30.2d, v3.2d,  v25.2d
    fmla    v31.2d, v4.2d,  v25.2d
    fmla    v30.2d, v6.2d,  v26.2d
    fmla    v31.2d, v23.2d, v26.2d
    fmla    v30.2d, v7.2d,  v27.2d
    fmla    v31.2d, v29.2d, v27.2d

    stp     q30, q31, [x9, #-32]
    mov     v16.16b, v20.16b
    mov     v17.16b, v21.16b
    mov     v18.16b, v22.16b
    subs    x10, x10, #1
    b.ne    L_symp7_vec8_loop

L_symp7_ret:
    mov     x0, #0x5336
    lsl     x0, x0, #48
    and     x11, x4, #0xFFFF
    lsl     x11, x11, #24
    orr     x0, x0, x11
    orr     x0, x0, #3                      // Mode 3: SYMPLECTIC_7TAP_SIAC_CYCLOTOMIC
    ret
L_symp7_done:
    mov     x0, #0x5336
    lsl     x0, x0, #48
    ret

// 2. _omega_v36_symplectic_time_reverse_retract_arm64
    .globl _omega_v36_symplectic_time_reverse_retract_arm64
_omega_v36_symplectic_time_reverse_retract_arm64:
    cbz     x3, L_sret_done
    fneg    d0, d0
    dup     v24.2d, v0.d[0]
    fmov    d25, xzr
    dup     v25.2d, v25.d[0]
    lsr     x4, x3, #1
    cbz     x4, L_sret_done
    .p2align 4
L_sret_loop2:
    ldr     q16, [x0]
    ldr     q2,  [x1]
    ldr     q4,  [x2], #16
    fmul    v4.2d,  v4.2d,  v24.2d
    fadd    v6.2d,  v16.2d, v4.2d
    fsub    v30.2d, v6.2d,  v16.2d
    fsub    v18.2d, v6.2d,  v30.2d
    fsub    v18.2d, v16.2d, v18.2d
    fsub    v4.2d,  v4.2d,  v30.2d
    fadd    v18.2d, v18.2d, v4.2d
    fadd    v18.2d, v18.2d, v2.2d
    fadd    v6.2d,  v6.2d,  v18.2d
    str     q6,  [x0], #16
    str     q25, [x1], #16
    subs    x4, x4, #1
    b.ne    L_sret_loop2
L_sret_done:
    ret

// 3. _omega_v36_nanotech_anisotropic_phonon_spin_arc_arm64
    .globl _omega_v36_nanotech_anisotropic_phonon_spin_arc_arm64
_omega_v36_nanotech_anisotropic_phonon_spin_arc_arm64:
    lsr     x6, x5, #3
    cbz     x6, L_ph_done
    dup     v24.2d, v0.d[0]
    fneg    v24.2d, v24.2d
    movz    x8, #0xc047, lsl #48        // -46.0
    fmov    d25, x8
    dup     v25.2d, v25.d[0]
    fmov    d26, #-1.5
    dup     v26.2d, v26.d[0]
    fmov    d27, #0.5
    dup     v27.2d, v27.d[0]
    .p2align 4
L_ph_vec8:
    ldp     q0,  q1,  [x0], #64
    ldp     q2,  q3,  [x0, #-32]
    ldp     q4,  q5,  [x1], #64
    ldp     q6,  q7,  [x1, #-32]
    ldp     q16, q17, [x2], #64
    ldp     q18, q19, [x2, #-32]
    fsub    v0.2d, v0.2d, v4.2d
    fsub    v1.2d, v1.2d, v5.2d
    fsub    v2.2d, v2.2d, v6.2d
    fsub    v3.2d, v3.2d, v7.2d
    fmul    v20.2d, v0.2d, v0.2d
    fmul    v21.2d, v1.2d, v1.2d
    fmul    v22.2d, v2.2d, v2.2d
    fmul    v23.2d, v3.2d, v3.2d
    fmla    v0.2d, v20.2d, v26.2d
    fmla    v1.2d, v21.2d, v26.2d
    fmla    v2.2d, v22.2d, v26.2d
    fmla    v3.2d, v23.2d, v26.2d
    fmul    v16.2d, v16.2d, v27.2d
    fmul    v17.2d, v17.2d, v27.2d
    fmul    v18.2d, v18.2d, v27.2d
    fmul    v19.2d, v19.2d, v27.2d
    fmla    v0.2d, v20.2d, v16.2d
    fmla    v1.2d, v21.2d, v17.2d
    fmla    v2.2d, v22.2d, v18.2d
    fmla    v3.2d, v23.2d, v19.2d
    fmul    v0.2d, v0.2d, v25.2d
    fmul    v1.2d, v1.2d, v25.2d
    fmul    v2.2d, v2.2d, v25.2d
    fmul    v3.2d, v3.2d, v25.2d
    stp     q0, q1, [x3], #64
    stp     q2, q3, [x3, #-32]
    fmul    v20.2d, v20.2d, v24.2d
    fmul    v21.2d, v21.2d, v24.2d
    fmul    v22.2d, v22.2d, v24.2d
    fmul    v23.2d, v23.2d, v24.2d
    stp     q20, q21, [x4], #64
    stp     q22, q23, [x4, #-32]
    subs    x6, x6, #1
    b.ne    L_ph_vec8
L_ph_done:
    ret

// 4. _omega_v36_nonabelian_su2_cd_hubo_trigger_arm64
    .globl _omega_v36_nonabelian_su2_cd_hubo_trigger_arm64
_omega_v36_nonabelian_su2_cd_hubo_trigger_arm64:
    mov     x8, #0
    lsr     x5, x4, #2
    cbz     x5, L_su2_done
    dup     v24.2d, v0.d[0]
    dup     v25.2d, v1.d[0]
    fmov    d26, #1.0
    dup     v26.2d, v26.d[0]
    fmov    d27, #-1.0
    dup     v27.2d, v27.d[0]
    fmov    d28, #0.75
    dup     v28.2d, v28.d[0]
    mov     x10, #0
    .p2align 4
L_su2_loop4:
    ldp     q0, q1, [x0]
    ldp     q2, q3, [x1]
    ldp     q4, q5, [x2], #32
    ldp     q6, q7, [x3], #32
    fmul    v16.2d, v0.2d, v0.2d
    fmul    v17.2d, v1.2d, v1.2d
    fmul    v18.2d, v16.2d, v0.2d
    fmul    v19.2d, v17.2d, v1.2d
    fsub    v20.2d, v0.2d, v18.2d
    fsub    v21.2d, v1.2d, v19.2d
    fadd    v20.2d, v20.2d, v4.2d
    fadd    v21.2d, v21.2d, v5.2d
    fmul    v20.2d, v20.2d, v6.2d
    fmul    v21.2d, v21.2d, v7.2d
    fmla    v20.2d, v2.2d,  v25.2d
    fmla    v21.2d, v3.2d,  v25.2d
    fmla    v2.2d, v20.2d, v24.2d
    fmla    v3.2d, v21.2d, v24.2d
    fmla    v0.2d, v2.2d,  v24.2d
    fmla    v1.2d, v3.2d,  v24.2d
    fminnm  v0.2d, v0.2d, v26.2d
    fminnm  v1.2d, v1.2d, v26.2d
    fmaxnm  v0.2d, v0.2d, v27.2d
    fmaxnm  v1.2d, v1.2d, v27.2d
    stp     q0, q1, [x0], #32
    stp     q2, q3, [x1], #32
    facgt   v22.2d, v0.2d, v28.2d
    facgt   v23.2d, v1.2d, v28.2d
    umov    x11, v22.d[0]
    and     x11, x11, #1
    umov    x12, v22.d[1]
    and     x12, x12, #2
    orr     x11, x11, x12
    umov    x12, v23.d[0]
    and     x12, x12, #4
    orr     x11, x11, x12
    umov    x12, v23.d[1]
    and     x12, x12, #8
    orr     x11, x11, x12
    lsl     x11, x11, x10
    orr     x8,  x8,  x11
    add     x10, x10, #4
    subs    x5, x5, #1
    b.ne    L_su2_loop4
L_su2_done:
    mov     x0, x8
    ret
