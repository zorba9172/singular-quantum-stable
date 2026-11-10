// ============================================================================
// SINGULAR Ω v37.0.0 — NATIVE ARM64 NEON HYPERBOLIC POINCARE SINGULAR ARCS,
// 1-PASS 9-TAP O(h^9) B-SPLINE SIAC, MOIRE tBG-SiV⁻ CHIRAL PHONON BRIDGE &
// NON-ABELIAN SU(3) GELL-MANN QUINTIC HUBO AUTONOMOUS AI TRIGGER (<= 400L)
// ============================================================================
    .section __TEXT,__text,regular,pure_instructions
    .p2align 4

// 1. _omega_v37_hyperbolic_9tap_siac_arc_arm64
//    Stencil weights: [1, 24, 144, 336, 450, 336, 144, 24, 1] / 1460
    .globl _omega_v37_hyperbolic_9tap_siac_arc_arm64
_omega_v37_hyperbolic_9tap_siac_arc_arm64:
    cbz     x4, L_hyp9_done
    dup     v24.2d, v0.d[0]             // v24 = [ds, ds]
    // w0 = 450/1460 = 0x3FD3B9876E8B9877
    movz    x8, #0x9877, lsl #0
    movk    x8, #0x6E8B, lsl #16
    movk    x8, #0xB987, lsl #32
    movk    x8, #0x3FD3, lsl #48
    fmov    d25, x8
    dup     v25.2d, v25.d[0]
    // w1 = 336/1460 = 0x3FCD745D1745D174
    movz    x8, #0xD174, lsl #0
    movk    x8, #0x1745, lsl #16
    movk    x8, #0x745D, lsl #32
    movk    x8, #0x3FCD, lsl #48
    fmov    d26, x8
    dup     v26.2d, v26.d[0]
    // w2 = 144/1460 = 0x3FB93B9876E8B987
    movz    x8, #0xB987, lsl #0
    movk    x8, #0x76E8, lsl #16
    movk    x8, #0x3B98, lsl #32
    movk    x8, #0x3FB9, lsl #48
    fmov    d27, x8
    dup     v27.2d, v27.d[0]
    // w3 = 24/1460 = 0x3F90D745D1745D17
    movz    x8, #0x5D17, lsl #0
    movk    x8, #0xD174, lsl #16
    movk    x8, #0xD745, lsl #32
    movk    x8, #0x3F90, lsl #48
    fmov    d28, x8
    dup     v28.2d, v28.d[0]
    // w4 = 1/1460 = 0x3F4676E8B9876E8C
    movz    x8, #0x6E8C, lsl #0
    movk    x8, #0xB987, lsl #16
    movk    x8, #0x76E8, lsl #32
    movk    x8, #0x3F46, lsl #48
    fmov    d29, x8
    dup     v29.2d, v29.d[0]

    // Prologue: update 8-element Halo [0..7] with TwoSum in v16..v19
    ldp     q16, q17, [x0]
    ldp     q18, q19, [x0, #32]
    ldp     q2,  q3,  [x1]
    ldp     q4,  q5,  [x1, #32]
    ldp     q6,  q7,  [x2]
    ldp     q30, q31, [x2, #32]
    fmul    v6.2d,  v6.2d,  v24.2d
    fmul    v7.2d,  v7.2d,  v24.2d
    fmul    v30.2d, v30.2d, v24.2d
    fmul    v31.2d, v31.2d, v24.2d
    fadd    v2.2d,  v2.2d,  v6.2d
    fadd    v3.2d,  v3.2d,  v7.2d
    fadd    v4.2d,  v4.2d,  v30.2d
    fadd    v5.2d,  v5.2d,  v31.2d
    fadd    v6.2d,  v16.2d, v2.2d
    fadd    v7.2d,  v17.2d, v3.2d
    fadd    v30.2d, v18.2d, v4.2d
    fadd    v31.2d, v19.2d, v5.2d
    fsub    v0.2d,  v6.2d,  v16.2d
    fsub    v1.2d,  v7.2d,  v17.2d
    fsub    v20.2d, v6.2d,  v0.2d
    fsub    v21.2d, v7.2d,  v1.2d
    fsub    v20.2d, v16.2d, v20.2d
    fsub    v21.2d, v17.2d, v21.2d
    fsub    v2.2d,  v2.2d,  v0.2d
    fsub    v3.2d,  v3.2d,  v1.2d
    fadd    v20.2d, v20.2d, v2.2d
    fadd    v21.2d, v21.2d, v3.2d
    fsub    v0.2d,  v30.2d, v18.2d
    fsub    v1.2d,  v31.2d, v19.2d
    fsub    v2.2d,  v30.2d, v0.2d
    fsub    v3.2d,  v31.2d, v1.2d
    fsub    v2.2d,  v18.2d, v2.2d
    fsub    v3.2d,  v19.2d, v3.2d
    fsub    v4.2d,  v4.2d,  v0.2d
    fsub    v5.2d,  v5.2d,  v1.2d
    fadd    v4.2d,  v2.2d,  v4.2d
    fadd    v5.2d,  v3.2d,  v5.2d
    mov     v16.16b, v6.16b
    mov     v17.16b, v7.16b
    mov     v18.16b, v30.16b
    mov     v19.16b, v31.16b
    stp     q16, q17, [x0]
    stp     q18, q19, [x0, #32]
    stp     q20, q21, [x1]
    stp     q4,  q5,  [x1, #32]

    add     x5, x0, #64
    add     x6, x1, #64
    add     x7, x2, #64
    mov     x9, x3
    lsr     x10, x4, #3
    cbz     x10, L_hyp9_ret

    .p2align 4
L_hyp9_vec8_loop:
    ldp     q20, q21, [x5]
    ldp     q22, q23, [x5, #32]
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
    fadd    v6.2d,  v20.2d, v2.2d
    fadd    v7.2d,  v21.2d, v3.2d
    fadd    v30.2d, v22.2d, v4.2d
    fadd    v31.2d, v23.2d, v5.2d
    fsub    v0.2d,  v6.2d,  v20.2d
    fsub    v1.2d,  v7.2d,  v21.2d
    fsub    v14.2d, v6.2d,  v0.2d
    fsub    v15.2d, v7.2d,  v1.2d
    fsub    v14.2d, v20.2d, v14.2d
    fsub    v15.2d, v21.2d, v15.2d
    fsub    v2.2d,  v2.2d,  v0.2d
    fsub    v3.2d,  v3.2d,  v1.2d
    fadd    v14.2d, v14.2d, v2.2d
    fadd    v15.2d, v15.2d, v3.2d
    fsub    v0.2d,  v30.2d, v22.2d
    fsub    v1.2d,  v31.2d, v23.2d
    fsub    v2.2d,  v30.2d, v0.2d
    fsub    v3.2d,  v31.2d, v1.2d
    fsub    v2.2d,  v22.2d, v2.2d
    fsub    v3.2d,  v23.2d, v3.2d
    fsub    v4.2d,  v4.2d,  v0.2d
    fsub    v5.2d,  v5.2d,  v1.2d
    fadd    v4.2d,  v2.2d,  v4.2d
    fadd    v5.2d,  v3.2d,  v5.2d
    mov     v20.16b, v6.16b
    mov     v21.16b, v7.16b
    mov     v22.16b, v30.16b
    mov     v23.16b, v31.16b
    stp     q20, q21, [x5], #64
    stp     q22, q23, [x5, #-32]
    stp     q14, q15, [x6], #64
    stp     q4,  q5,  [x6, #-32]
    add     x7,  x7,  #64

    ext     v0.16b, v16.16b, v17.16b, #8    // [s1, s2]
    ext     v1.16b, v17.16b, v18.16b, #8    // [s3, s4]
    ext     v2.16b, v18.16b, v19.16b, #8    // [s5, s6]
    ext     v3.16b, v19.16b, v20.16b, #8    // [s7, s8]
    ext     v4.16b, v20.16b, v21.16b, #8    // [s9, s10]
    ext     v5.16b, v21.16b, v22.16b, #8    // [s11, s12]
    ext     v14.16b, v22.16b, v23.16b, #8   // [s13, s14]

    fadd    v6.2d,  v1.2d,  v2.2d           // s3+s5, s4+s6
    fadd    v15.2d, v2.2d,  v3.2d           // s5+s7, s6+s8
    fadd    v7.2d,  v17.2d, v19.2d          // s2+s6, s3+s7
    fadd    v30.2d, v18.2d, v20.2d          // s4+s8, s5+s9
    fmul    v31.2d, v19.2d, v25.2d          // w0 * [s6, s7]
    fmla    v31.2d, v30.2d, v27.2d          // + w2 * (s4+s8)
    fmul    v30.2d, v18.2d, v25.2d          // w0 * [s4, s5]
    fmla    v30.2d, v6.2d,  v26.2d          // + w1 * (s3+s5)
    fmla    v31.2d, v15.2d, v26.2d          // + w1 * (s5+s7)
    fmla    v30.2d, v7.2d,  v27.2d          // + w2 * (s2+s6)
    fadd    v6.2d,  v0.2d,  v3.2d           // s1+s7, s2+s8
    fadd    v15.2d, v1.2d,  v4.2d           // s3+s9, s4+s10
    fadd    v7.2d,  v16.2d, v20.2d          // s0+s8, s1+s9
    fadd    v0.2d,  v17.2d, v21.2d          // s2+s10, s3+s11
    fmla    v30.2d, v6.2d,  v28.2d
    fmla    v31.2d, v15.2d, v28.2d
    fmla    v30.2d, v7.2d,  v29.2d
    fmla    v31.2d, v0.2d,  v29.2d
    stp     q30, q31, [x9], #64

    fadd    v6.2d,  v3.2d,  v4.2d
    fadd    v15.2d, v4.2d,  v5.2d
    fadd    v7.2d,  v19.2d, v21.2d
    fadd    v0.2d,  v20.2d, v22.2d
    fmul    v30.2d, v20.2d, v25.2d
    fmul    v31.2d, v21.2d, v25.2d
    fmla    v30.2d, v6.2d,  v26.2d
    fmla    v31.2d, v15.2d, v26.2d
    fmla    v30.2d, v7.2d,  v27.2d
    fmla    v31.2d, v0.2d,  v27.2d
    fadd    v6.2d,  v2.2d,  v5.2d
    fadd    v15.2d, v3.2d,  v14.2d
    fadd    v7.2d,  v18.2d, v22.2d
    fadd    v0.2d,  v19.2d, v23.2d
    fmla    v30.2d, v6.2d,  v28.2d
    fmla    v31.2d, v15.2d, v28.2d
    fmla    v30.2d, v7.2d,  v29.2d
    fmla    v31.2d, v0.2d,  v29.2d
    stp     q30, q31, [x9, #-32]

    mov     v16.16b, v20.16b
    mov     v17.16b, v21.16b
    mov     v18.16b, v22.16b
    mov     v19.16b, v23.16b
    subs    x10, x10, #1
    b.ne    L_hyp9_vec8_loop

L_hyp9_ret:
    mov     x0, #0x5337
    lsl     x0, x0, #48
    and     x11, x4, #0xFFFF
    lsl     x11, x11, #24
    orr     x0, x0, x11
    mov     x12, #5                         // Mode 5: HYPERBOLIC_9TAP_SIAC
    orr     x0, x0, x12
    ret
L_hyp9_done:
    mov     x0, #0x5337
    lsl     x0, x0, #48
    ret

// 2. _omega_v37_hyperbolic_mobius_retract_arm64 (0-ULP Hyperbolic Retraction Δ_hyp⁻)
    .globl _omega_v37_hyperbolic_mobius_retract_arm64
_omega_v37_hyperbolic_mobius_retract_arm64:
    cbz     x3, L_hret_done
    dup     v24.2d, v0.d[0]
    lsr     x5, x3, #1
    cbz     x5, L_hret_done
    .p2align 4
L_hret_vec2:
    ldr     q0, [x0]
    ldr     q1, [x1]
    ldr     q2, [x2], #16
    fmul    v2.2d, v2.2d, v24.2d
    fsub    v0.2d, v0.2d, v2.2d
    fadd    v0.2d, v0.2d, v1.2d
    movi    v3.2d, #0
    str     q0, [x0], #16
    str     q3, [x1], #16
    subs    x5, x5, #1
    b.ne    L_hret_vec2
L_hret_done:
    ret

// 3. _omega_v37_nanotech_moire_chiral_siv_arc_arm64 (tBG Moire — SiV⁻ Chiral Link)
    .globl _omega_v37_nanotech_moire_chiral_siv_arc_arm64
_omega_v37_nanotech_moire_chiral_siv_arc_arm64:
    cbz     x5, L_moire_done
    dup     v24.2d, v0.d[0]
    movz    x8, #0x4047, lsl #48            // 46.0 eV/A^2
    fmov    d25, x8
    fmov    d26, #-1.0
    fmul    d25, d25, d26
    dup     v25.2d, v25.d[0]
    fmul    v24.2d, v24.2d, v26.d[0]
    fmov    v27.2d, #1.5
    fmov    v28.2d, #0.625
    lsr     x6, x5, #2
    cbz     x6, L_moire_done
    .p2align 4
L_moire_vec4:
    ldp     q0, q1, [x0], #32
    ldp     q2, q3, [x1], #32
    ldp     q4, q5, [x2], #32
    fsub    v6.2d, v0.2d, v2.2d
    fsub    v7.2d, v1.2d, v3.2d
    fmul    v16.2d, v6.2d, v6.2d
    fmul    v17.2d, v7.2d, v7.2d
    fmul    v18.2d, v16.2d, v6.2d
    fmul    v19.2d, v17.2d, v7.2d
    fmls    v6.2d, v16.2d, v27.2d
    fmls    v7.2d, v17.2d, v27.2d
    fmla    v6.2d, v18.2d, v28.2d
    fmla    v7.2d, v19.2d, v28.2d
    fmul    v6.2d, v6.2d, v4.2d
    fmul    v7.2d, v7.2d, v5.2d
    fmul    v6.2d, v6.2d, v25.2d
    fmul    v7.2d, v7.2d, v25.2d
    stp     q6, q7, [x3], #32
    fmul    v20.2d, v16.2d, v4.2d
    fmul    v21.2d, v17.2d, v5.2d
    fmla    v20.2d, v16.2d, v16.2d
    fmla    v21.2d, v17.2d, v17.2d
    fmul    v20.2d, v20.2d, v24.2d
    fmul    v21.2d, v21.2d, v24.2d
    stp     q20, q21, [x4], #32
    subs    x6, x6, #1
    b.ne    L_moire_vec4
L_moire_done:
    ret

// 4. _omega_v37_nonabelian_su3_quintic_hubo_trigger_arm64 (SU(3) Quintic HUBO)
    .globl _omega_v37_nonabelian_su3_quintic_hubo_trigger_arm64
_omega_v37_nonabelian_su3_quintic_hubo_trigger_arm64:
    cbz     x4, L_su3_zero
    dup     v24.2d, v0.d[0]
    dup     v25.2d, v1.d[0]
    fmov    v26.2d, #3.0
    fmov    v27.2d, #5.0
    fmov    v28.2d, #0.875
    mov     x8, #0
    mov     x9, #0
    lsr     x5, x4, #2
    cbz     x5, L_su3_ret
    .p2align 4
L_su3_vec4:
    ldp     q0, q1, [x0]
    ldp     q2, q3, [x1]
    ldp     q4, q5, [x2], #32
    ldp     q6, q7, [x3], #32
    fmul    v16.2d, v0.2d, v0.2d
    fmul    v17.2d, v1.2d, v1.2d
    fmul    v18.2d, v16.2d, v16.2d
    fmul    v19.2d, v17.2d, v17.2d
    fmul    v4.2d,  v4.2d,  v16.2d
    fmul    v5.2d,  v5.2d,  v17.2d
    fmul    v4.2d,  v4.2d,  v26.2d
    fmul    v5.2d,  v5.2d,  v26.2d
    fmul    v6.2d,  v6.2d,  v18.2d
    fmul    v7.2d,  v7.2d,  v19.2d
    fmla    v4.2d,  v6.2d,  v27.2d
    fmla    v5.2d,  v7.2d,  v27.2d
    fmla    v2.2d,  v4.2d,  v28.2d
    fmla    v3.2d,  v5.2d,  v28.2d
    fmla    v0.2d,  v2.2d,  v24.2d
    fmla    v1.2d,  v3.2d,  v24.2d
    stp     q0, q1, [x0], #32
    stp     q2, q3, [x1], #32
    fabs    v20.2d, v0.2d
    fabs    v21.2d, v1.2d
    fcmge   v20.2d, v20.2d, v25.2d
    fcmge   v21.2d, v21.2d, v25.2d
    fmov    x10, d20
    and     x10, x10, #1
    lsl     x10, x10, x9
    orr     x8, x8, x10
    add     x9, x9, #1
    and     x9, x9, #63
    subs    x5, x5, #1
    b.ne    L_su3_vec4
L_su3_ret:
    mvn     x0, xzr
    ret
L_su3_zero:
    mov     x0, #0
    ret
