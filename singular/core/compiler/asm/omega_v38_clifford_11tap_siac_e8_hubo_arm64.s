// ============================================================================
// SINGULAR Ω v38.0.0 — NATIVE ARM64 NEON CLIFFORD SPINOR Cl_{3,1}(R) SINGULAR
// ARCS, 1-PASS 11-TAP O(h^11) B-SPLINE SIAC, KAGOME-SiV⁻ MAJORANA BRAID LINK
// & E8 GOSSET 6TH-ORDER SEXTIC HUBO AUTONOMOUS AI TRIGGER (<= 400L)
// ============================================================================
    .section __TEXT,__text,regular,pure_instructions
    .p2align 4

// 1. _omega_v38_clifford_11tap_siac_arc_arm64
//    Stencil weights: [1, 15, 100, 336, 630, 860, 630, 336, 100, 15, 1] / 3024
    .globl _omega_v38_clifford_11tap_siac_arc_arm64
_omega_v38_clifford_11tap_siac_arc_arm64:
    cbz     x4, L_clif11_done
    stp     d12, d13, [sp, #-32]!
    stp     d14, d15, [sp, #16]
    dup     v24.2d, v0.d[0]             // v24 = [dtheta, dtheta]
    movz    x8, #0xDFA4, lsl #0
    movk    x8, #0x0EBE, lsl #16
    movk    x8, #0x339C, lsl #32
    movk    x8, #0x3FD2, lsl #48
    fmov    d25, x8
    dup     v25.2d, v25.d[0]
    // w1 = 630/3024 = 5/24 = 0x3FCAAAAAAAAAAAAA
    movz    x8, #0xAAAA, lsl #0
    movk    x8, #0xAAAA, lsl #16
    movk    x8, #0xAAAA, lsl #32
    movk    x8, #0x3FCA, lsl #48
    fmov    d26, x8
    dup     v26.2d, v26.d[0]
    // w2 = 336/3024 = 1/9 = 0x3FBBC71C71C71C72
    movz    x8, #0x1C72, lsl #0
    movk    x8, #0x71C7, lsl #16
    movk    x8, #0xBC71, lsl #32
    movk    x8, #0x3FBB, lsl #48
    fmov    d27, x8
    dup     v27.2d, v27.d[0]
    // w3 = 100/3024 = 0x3FA0F205C5B853B2
    movz    x8, #0x53B2, lsl #0
    movk    x8, #0xC5B8, lsl #16
    movk    x8, #0xF205, lsl #32
    movk    x8, #0x3FA0, lsl #48
    fmov    d28, x8
    dup     v28.2d, v28.d[0]
    // w4 = 15/3024 = 0x3F74484484484484
    movz    x8, #0x4484, lsl #0
    movk    x8, #0x8448, lsl #16
    movk    x8, #0x4844, lsl #32
    movk    x8, #0x3F74, lsl #48
    fmov    d29, x8
    dup     v29.2d, v29.d[0]
    // w5 = 1/3024 = 0x3F35AB9F51853B29
    movz    x8, #0x3B29, lsl #0
    movk    x8, #0x5185, lsl #16
    movk    x8, #0xAB9F, lsl #32
    movk    x8, #0x3F35, lsl #48
    fmov    d13, x8
    dup     v13.2d, v13.d[0]

    // Prologue: update 10-element Halo [0..9] with TwoSum in v16..v20
    ldp     q16, q17, [x0]
    ldp     q18, q19, [x0, #32]
    ldr     q20, [x0, #64]
    ldp     q2,  q3,  [x1]
    ldp     q4,  q5,  [x1, #32]
    ldr     q12, [x1, #64]
    ldp     q6,  q7,  [x2]
    ldp     q30, q31, [x2, #32]
    ldr     q14, [x2, #64]

    fmul    v6.2d,  v6.2d,  v24.2d
    fmul    v7.2d,  v7.2d,  v24.2d
    fmul    v30.2d, v30.2d, v24.2d
    fmul    v31.2d, v31.2d, v24.2d
    fmul    v14.2d, v14.2d, v24.2d
    fadd    v2.2d,  v2.2d,  v6.2d
    fadd    v3.2d,  v3.2d,  v7.2d
    fadd    v4.2d,  v4.2d,  v30.2d
    fadd    v5.2d,  v5.2d,  v31.2d
    fadd    v12.2d, v12.2d, v14.2d

    fadd    v6.2d,  v16.2d, v2.2d
    fadd    v7.2d,  v17.2d, v3.2d
    fadd    v30.2d, v18.2d, v4.2d
    fadd    v31.2d, v19.2d, v5.2d
    fadd    v14.2d, v20.2d, v12.2d

    fsub    v0.2d,  v6.2d,  v16.2d
    fsub    v1.2d,  v7.2d,  v17.2d
    fsub    v21.2d, v6.2d,  v0.2d
    fsub    v22.2d, v7.2d,  v1.2d
    fsub    v21.2d, v16.2d, v21.2d
    fsub    v22.2d, v17.2d, v22.2d
    fsub    v2.2d,  v2.2d,  v0.2d
    fsub    v3.2d,  v3.2d,  v1.2d
    fadd    v21.2d, v21.2d, v2.2d
    fadd    v22.2d, v22.2d, v3.2d

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

    fsub    v0.2d,  v14.2d, v20.2d
    fsub    v2.2d,  v14.2d, v0.2d
    fsub    v2.2d,  v20.2d, v2.2d
    fsub    v12.2d, v12.2d, v0.2d
    fadd    v12.2d, v2.2d,  v12.2d

    mov     v16.16b, v6.16b
    mov     v17.16b, v7.16b
    mov     v18.16b, v30.16b
    mov     v19.16b, v31.16b
    mov     v20.16b, v14.16b

    stp     q16, q17, [x0]
    stp     q18, q19, [x0, #32]
    str     q20, [x0, #64]
    stp     q21, q22, [x1]
    stp     q4,  q5,  [x1, #32]
    str     q12, [x1, #64]

    add     x5, x0, #80                 // state_s + 10
    add     x6, x1, #80                 // err_s + 10
    add     x7, x2, #80                 // bivector_b + 10
    mov     x9, x3                      // out_siac11
    lsr     x10, x4, #3
    cbz     x10, L_clif11_ret

    .p2align 4
L_clif11_vec8_loop:
    // Halo [s0..s9] in v16..v20; load new 8 elements [s10..s17] in v21..v23, v12
    ldp     q21, q22, [x5]
    ldp     q23, q12, [x5, #32]
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

    fadd    v6.2d,  v21.2d, v2.2d
    fadd    v7.2d,  v22.2d, v3.2d
    fadd    v30.2d, v23.2d, v4.2d
    fadd    v31.2d, v12.2d, v5.2d

    fsub    v0.2d,  v6.2d,  v21.2d
    fsub    v1.2d,  v7.2d,  v22.2d
    fsub    v14.2d, v6.2d,  v0.2d
    fsub    v15.2d, v7.2d,  v1.2d
    fsub    v14.2d, v21.2d, v14.2d
    fsub    v15.2d, v22.2d, v15.2d
    fsub    v2.2d,  v2.2d,  v0.2d
    fsub    v3.2d,  v3.2d,  v1.2d
    fadd    v14.2d, v14.2d, v2.2d
    fadd    v15.2d, v15.2d, v3.2d

    fsub    v0.2d,  v30.2d, v23.2d
    fsub    v1.2d,  v31.2d, v12.2d
    fsub    v2.2d,  v30.2d, v0.2d
    fsub    v3.2d,  v31.2d, v1.2d
    fsub    v2.2d,  v23.2d, v2.2d
    fsub    v3.2d,  v12.2d, v3.2d
    fsub    v4.2d,  v4.2d,  v0.2d
    fsub    v5.2d,  v5.2d,  v1.2d
    fadd    v4.2d,  v2.2d,  v4.2d
    fadd    v5.2d,  v3.2d,  v5.2d

    mov     v21.16b, v6.16b
    mov     v22.16b, v7.16b
    mov     v23.16b, v30.16b
    mov     v12.16b, v31.16b

    stp     q21, q22, [x5], #64
    stp     q23, q12, [x5, #-32]
    stp     q14, q15, [x6], #64
    stp     q4,  q5,  [x6, #-32]
    add     x7,  x7,  #64

    // Shifted views via `ext` for 11-tap stencil
    ext     v0.16b,  v16.16b, v17.16b, #8   // [s1, s2]
    ext     v1.16b,  v17.16b, v18.16b, #8   // [s3, s4]
    ext     v2.16b,  v18.16b, v19.16b, #8   // [s5, s6]
    ext     v3.16b,  v19.16b, v20.16b, #8   // [s7, s8]
    ext     v4.16b,  v20.16b, v21.16b, #8   // [s9, s10]
    ext     v5.16b,  v21.16b, v22.16b, #8   // [s11, s12]
    ext     v14.16b, v22.16b, v23.16b, #8   // [s13, s14]
    ext     v15.16b, v23.16b, v12.16b, #8   // [s15, s16]

    // Out[0..1] (centered at v2=[s5,s6]) & Out[2..3] (centered at v3=[s7,s8]) interleaved
    fadd    v6.2d,  v18.2d, v19.2d          // s4+s6, s5+s7
    fadd    v7.2d,  v19.2d, v20.2d          // s6+s8, s7+s9
    fmul    v30.2d, v2.2d,  v25.2d          // w0 * [s5, s6]
    fmul    v31.2d, v3.2d,  v25.2d          // w0 * [s7, s8]
    fmla    v30.2d, v6.2d,  v26.2d          // + w1
    fmla    v31.2d, v7.2d,  v26.2d
    fadd    v6.2d,  v1.2d,  v3.2d           // s3+s7, s4+s8
    fadd    v7.2d,  v2.2d,  v4.2d           // s5+s9, s6+s10
    fmla    v30.2d, v6.2d,  v27.2d          // + w2
    fmla    v31.2d, v7.2d,  v27.2d
    fadd    v6.2d,  v17.2d, v20.2d          // s2+s8, s3+s9
    fadd    v7.2d,  v18.2d, v21.2d          // s4+s10, s5+s11
    fmla    v30.2d, v6.2d,  v28.2d          // + w3
    fmla    v31.2d, v7.2d,  v28.2d
    fadd    v6.2d,  v0.2d,  v4.2d           // s1+s9, s2+s10
    fadd    v7.2d,  v1.2d,  v5.2d           // s3+s11, s4+s12
    fmla    v30.2d, v6.2d,  v29.2d          // + w4
    fmla    v31.2d, v7.2d,  v29.2d
    fadd    v6.2d,  v16.2d, v21.2d          // s0+s10, s1+s11
    fadd    v7.2d,  v17.2d, v22.2d          // s2+s12, s3+s13
    fmla    v30.2d, v6.2d,  v13.2d          // + w5
    fmla    v31.2d, v7.2d,  v13.2d
    stp     q30, q31, [x9], #64

    // Out[4..5] (centered at v4=[s9,s10]) & Out[6..7] (centered at v5=[s11,s12]) interleaved
    fadd    v6.2d,  v20.2d, v21.2d          // s8+s10, s9+s11
    fadd    v7.2d,  v21.2d, v22.2d          // s10+s12, s11+s13
    fmul    v30.2d, v4.2d,  v25.2d
    fmul    v31.2d, v5.2d,  v25.2d
    fmla    v30.2d, v6.2d,  v26.2d
    fmla    v31.2d, v7.2d,  v26.2d
    fadd    v6.2d,  v3.2d,  v5.2d           // s7+s11, s8+s12
    fadd    v7.2d,  v4.2d,  v14.2d          // s9+s13, s10+s14
    fmla    v30.2d, v6.2d,  v27.2d
    fmla    v31.2d, v7.2d,  v27.2d
    fadd    v6.2d,  v19.2d, v22.2d          // s6+s12, s7+s13
    fadd    v7.2d,  v20.2d, v23.2d          // s8+s14, s9+s15
    fmla    v30.2d, v6.2d,  v28.2d
    fmla    v31.2d, v7.2d,  v28.2d
    fadd    v6.2d,  v2.2d,  v14.2d          // s5+s13, s6+s14
    fadd    v7.2d,  v3.2d,  v15.2d          // s7+s15, s8+s16
    fmla    v30.2d, v6.2d,  v29.2d
    fmla    v31.2d, v7.2d,  v29.2d
    fadd    v6.2d,  v18.2d, v23.2d          // s4+s14, s5+s15
    fadd    v7.2d,  v19.2d, v12.2d          // s6+s16, s7+s17
    fmla    v30.2d, v6.2d,  v13.2d
    fmla    v31.2d, v7.2d,  v13.2d
    stp     q30, q31, [x9, #-32]

    mov     v16.16b, v20.16b
    mov     v17.16b, v21.16b
    mov     v18.16b, v22.16b
    mov     v19.16b, v23.16b
    mov     v20.16b, v12.16b
    subs    x10, x10, #1
    b.ne    L_clif11_vec8_loop
L_clif11_ret:
    ldp     d14, d15, [sp, #16]
    ldp     d12, d13, [sp], #32
    mov     x0, #0x5338
    lsl     x0, x0, #48
    and     x11, x4, #0xFFFF
    lsl     x11, x11, #24
    orr     x0, x0, x11
    mov     x12, #6                         // Mode 6: CLIFFORD_11TAP_SIAC_E8
    orr     x0, x0, x12
    ret
L_clif11_done:
    mov     x0, #0x5338
    lsl     x0, x0, #48
    ret
// 2. _omega_v38_clifford_spinor_retract_arm64 (0-ULP Spinor Reversion Δ_spin⁻)
    .globl _omega_v38_clifford_spinor_retract_arm64
_omega_v38_clifford_spinor_retract_arm64:
    cbz     x3, L_sret_done
    dup     v24.2d, v0.d[0]
    lsr     x5, x3, #1
    cbz     x5, L_sret_done
    .p2align 4
L_sret_vec2:
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
    b.ne    L_sret_vec2
L_sret_done:
    ret
// 3. _omega_v38_nanotech_kagome_majorana_siv_arc_arm64 (Kagome-SiV⁻ Majorana Braid)
    .globl _omega_v38_nanotech_kagome_majorana_siv_arc_arm64
_omega_v38_nanotech_kagome_majorana_siv_arc_arm64:
    cbz     x5, L_kag_done
    dup     v24.2d, v0.d[0]
    movz    x8, #0x4048, lsl #48            // 48.0 eV/A^2 Kagome-SiV spring
    fmov    d25, x8
    fmov    d26, #-1.0
    fmul    d25, d25, d26
    dup     v25.2d, v25.d[0]
    fmul    v24.2d, v24.2d, v26.d[0]
    fmov    v27.2d, #1.5
    fmov    v28.2d, #0.75
    lsr     x6, x5, #2
    cbz     x6, L_kag_done
    .p2align 4
L_kag_vec4:
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
    b.ne    L_kag_vec4
L_kag_done:
    ret

// 4. _omega_v38_e8_gosset_sextic_hubo_trigger_arm64 (E8 6th-Order Sextic HUBO)
    .globl _omega_v38_e8_gosset_sextic_hubo_trigger_arm64
_omega_v38_e8_gosset_sextic_hubo_trigger_arm64:
    cbz     x4, L_e8_zero
    dup     v24.2d, v0.d[0]
    dup     v25.2d, v1.d[0]
    fmov    v26.2d, #4.0
    fmov    v27.2d, #6.0
    fmov    v28.2d, #0.9375                 // E8 240-root Gosset Cartan gauge factor
    mov     x8, #0
    mov     x9, #0
    lsr     x5, x4, #2
    cbz     x5, L_e8_ret
    .p2align 4
L_e8_vec4:
    ldp     q0, q1, [x0]
    ldp     q2, q3, [x1]
    ldp     q4, q5, [x2], #32
    ldp     q6, q7, [x3], #32
    fmul    v16.2d, v0.2d, v0.2d            // s^2
    fmul    v17.2d, v1.2d, v1.2d
    fmul    v18.2d, v16.2d, v0.2d           // s^3
    fmul    v19.2d, v17.2d, v1.2d
    fmul    v20.2d, v18.2d, v16.2d          // s^5
    fmul    v21.2d, v19.2d, v17.2d
    fmul    v4.2d,  v4.2d,  v18.2d
    fmul    v5.2d,  v5.2d,  v19.2d
    fmul    v4.2d,  v4.2d,  v26.2d
    fmul    v5.2d,  v5.2d,  v26.2d
    fmul    v6.2d,  v6.2d,  v20.2d
    fmul    v7.2d,  v7.2d,  v21.2d
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
    b.ne    L_e8_vec4
L_e8_ret:
    mvn     x0, xzr
    ret
L_e8_zero:
    mov     x0, #0
    ret
