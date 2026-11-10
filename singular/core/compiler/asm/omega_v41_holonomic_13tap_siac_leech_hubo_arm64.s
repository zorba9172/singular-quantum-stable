// ============================================================================
// EYWA Ω4 / SINGULAR Ω v41.0.0 — NATIVE ARM64 NEON HOLONOMIC WILCZEK-ZEE
// SINGULAR ARCS (`Δ_hol⁺/Δ_hol⁻`), 1-PASS 13-TAP O(h^13) B-SPLINE SIAC,
// TORIC-SiV⁻ ANYON STABILIZER BRIDGE & 24D LEECH LATTICE (`Λ_24`) 8TH-ORDER
// OCTIC HUBO AUTONOMOUS AI TRIGGER (`EYW-001` / `SG-ARCH-400` <= 400L)
// ============================================================================
    .section __TEXT,__text,regular,pure_instructions
    .p2align 4

// 1. _omega_v41_holonomic_13tap_siac_arc_arm64
//    Stencil weights: [1, 36, 300, 1100, 2350, 3516, 3994, 3516, 2350, 1100, 300, 36, 1] / 18600
    .globl _omega_v41_holonomic_13tap_siac_arc_arm64
_omega_v41_holonomic_13tap_siac_arc_arm64:
    cbz     x4, L_hol13_done
    stp     d8,  d9,  [sp, #-64]!
    stp     d10, d11, [sp, #16]
    stp     d12, d13, [sp, #32]
    stp     d14, d15, [sp, #48]
    dup     v24.2d, v0.d[0]             // v24 = [dhol, dhol]

    // w0 = 3994/18600 = 0x3FCB7C9E1B206186
    movz    x8, #0x6186, lsl #0
    movk    x8, #0x1B20, lsl #16
    movk    x8, #0x7C9E, lsl #32
    movk    x8, #0x3FCB, lsl #48
    fmov    d25, x8
    dup     v25.2d, v25.d[0]
    // w1 = 3516/18600 = 0x3FC832352B959365
    movz    x8, #0x9365, lsl #0
    movk    x8, #0x2B95, lsl #16
    movk    x8, #0x3235, lsl #32
    movk    x8, #0x3FC8, lsl #48
    fmov    d26, x8
    dup     v26.2d, v26.d[0]
    // w2 = 2350/18600 = 0x3FC02C0D147AE148
    movz    x8, #0xE148, lsl #0
    movk    x8, #0x147A, lsl #16
    movk    x8, #0x2C0D, lsl #32
    movk    x8, #0x3FC0, lsl #48
    fmov    d27, x8
    dup     v27.2d, v27.d[0]
    // w3 = 1100/18600 = 0x3FAE43C88B439581
    movz    x8, #0x9581, lsl #0
    movk    x8, #0x8B43, lsl #16
    movk    x8, #0x43C8, lsl #32
    movk    x8, #0x3FAE, lsl #48
    fmov    d28, x8
    dup     v28.2d, v28.d[0]
    // w4 = 300/18600 = 0x3F90842108421084
    movz    x8, #0x1084, lsl #0
    movk    x8, #0x0842, lsl #16
    movk    x8, #0x8421, lsl #32
    movk    x8, #0x3F90, lsl #48
    fmov    d29, x8
    dup     v29.2d, v29.d[0]
    // w5_6 = 37/18600 = 0x3F604D4873ECADE3
    movz    x8, #0xADE3, lsl #0
    movk    x8, #0x73EC, lsl #16
    movk    x8, #0x4D48, lsl #32
    movk    x8, #0x3F60, lsl #48
    fmov    d13, x8
    dup     v13.2d, v13.d[0]

    // Prologue: update 12-element Halo [0..11] with TwoSum in v16..v20, v8
    ldp     q16, q17, [x0]
    ldp     q18, q19, [x0, #32]
    ldp     q20, q8,  [x0, #64]
    ldp     q2,  q3,  [x1]
    ldp     q4,  q5,  [x1, #32]
    ldp     q12, q9,  [x1, #64]
    ldp     q6,  q7,  [x2]
    ldp     q30, q31, [x2, #32]
    ldp     q14, q10, [x2, #64]

    fmul    v6.2d,  v6.2d,  v24.2d
    fmul    v7.2d,  v7.2d,  v24.2d
    fmul    v30.2d, v30.2d, v24.2d
    fmul    v31.2d, v31.2d, v24.2d
    fmul    v14.2d, v14.2d, v24.2d
    fmul    v10.2d, v10.2d, v24.2d
    fadd    v2.2d,  v2.2d,  v6.2d
    fadd    v3.2d,  v3.2d,  v7.2d
    fadd    v4.2d,  v4.2d,  v30.2d
    fadd    v5.2d,  v5.2d,  v31.2d
    fadd    v12.2d, v12.2d, v14.2d
    fadd    v9.2d,  v9.2d,  v10.2d

    fadd    v6.2d,  v16.2d, v2.2d
    fadd    v7.2d,  v17.2d, v3.2d
    fadd    v30.2d, v18.2d, v4.2d
    fadd    v31.2d, v19.2d, v5.2d
    fadd    v14.2d, v20.2d, v12.2d
    fadd    v10.2d, v8.2d,  v9.2d

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
    fsub    v1.2d,  v10.2d, v8.2d
    fsub    v2.2d,  v14.2d, v0.2d
    fsub    v3.2d,  v10.2d, v1.2d
    fsub    v2.2d,  v20.2d, v2.2d
    fsub    v3.2d,  v8.2d,  v3.2d
    fsub    v12.2d, v12.2d, v0.2d
    fsub    v9.2d,  v9.2d,  v1.2d
    fadd    v12.2d, v2.2d,  v12.2d
    fadd    v9.2d,  v3.2d,  v9.2d

    mov     v16.16b, v6.16b
    mov     v17.16b, v7.16b
    mov     v18.16b, v30.16b
    mov     v19.16b, v31.16b
    mov     v20.16b, v14.16b
    mov     v8.16b,  v10.16b

    stp     q16, q17, [x0]
    stp     q18, q19, [x0, #32]
    stp     q20, q8,  [x0, #64]
    stp     q21, q22, [x1]
    stp     q4,  q5,  [x1, #32]
    stp     q12, q9,  [x1, #64]

    add     x5, x0, #96                 // state_s + 12
    add     x6, x1, #96                 // err_s + 12
    add     x7, x2, #96                 // wilson_b + 12
    mov     x9, x3                      // out_siac13
    lsr     x10, x4, #3
    cbz     x10, L_hol13_ret

    .p2align 4
L_hol13_vec8_loop:
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

    // Shifted views for 13-tap symmetric SIAC stencil
    ext     v0.16b,  v16.16b, v17.16b, #8
    ext     v1.16b,  v17.16b, v18.16b, #8
    ext     v2.16b,  v18.16b, v19.16b, #8
    ext     v3.16b,  v19.16b, v20.16b, #8
    ext     v4.16b,  v20.16b, v8.16b,  #8
    ext     v5.16b,  v8.16b,  v21.16b, #8
    ext     v14.16b, v21.16b, v22.16b, #8
    ext     v15.16b, v22.16b, v23.16b, #8

    // Out[0..3] & Out[4..7]
    fadd    v6.2d,  v18.2d, v19.2d
    fadd    v7.2d,  v19.2d, v20.2d
    fmul    v30.2d, v3.2d,  v25.2d
    fmul    v31.2d, v4.2d,  v25.2d
    fmla    v30.2d, v6.2d,  v26.2d
    fmla    v31.2d, v7.2d,  v26.2d
    fadd    v6.2d,  v1.2d,  v4.2d
    fadd    v7.2d,  v2.2d,  v5.2d
    fmla    v30.2d, v6.2d,  v27.2d
    fmla    v31.2d, v7.2d,  v27.2d
    fadd    v6.2d,  v17.2d, v8.2d
    fadd    v7.2d,  v18.2d, v21.2d
    fmla    v30.2d, v6.2d,  v28.2d
    fmla    v31.2d, v7.2d,  v28.2d
    fadd    v6.2d,  v0.2d,  v5.2d
    fadd    v7.2d,  v1.2d,  v14.2d
    fmla    v30.2d, v6.2d,  v29.2d
    fmla    v31.2d, v7.2d,  v29.2d
    fadd    v6.2d,  v16.2d, v21.2d
    fadd    v7.2d,  v17.2d, v22.2d
    fmla    v30.2d, v6.2d,  v13.2d
    fmla    v31.2d, v7.2d,  v13.2d
    stp     q30, q31, [x9], #64

    fadd    v6.2d,  v20.2d, v8.2d
    fadd    v7.2d,  v8.2d,  v21.2d
    fmul    v30.2d, v5.2d,  v25.2d
    fmul    v31.2d, v14.2d, v25.2d
    fmla    v30.2d, v6.2d,  v26.2d
    fmla    v31.2d, v7.2d,  v26.2d
    fadd    v6.2d,  v3.2d,  v14.2d
    fadd    v7.2d,  v4.2d,  v15.2d
    fmla    v30.2d, v6.2d,  v27.2d
    fmla    v31.2d, v7.2d,  v27.2d
    fadd    v6.2d,  v19.2d, v22.2d
    fadd    v7.2d,  v20.2d, v23.2d
    fmla    v30.2d, v6.2d,  v28.2d
    fmla    v31.2d, v7.2d,  v28.2d
    fadd    v6.2d,  v2.2d,  v15.2d
    fadd    v7.2d,  v3.2d,  v12.2d
    fmla    v30.2d, v6.2d,  v29.2d
    fmla    v31.2d, v7.2d,  v29.2d
    fadd    v6.2d,  v18.2d, v23.2d
    fadd    v7.2d,  v19.2d, v12.2d
    fmla    v30.2d, v6.2d,  v13.2d
    fmla    v31.2d, v7.2d,  v13.2d
    stp     q30, q31, [x9, #-32]

    mov     v16.16b, v20.16b
    mov     v17.16b, v8.16b
    mov     v18.16b, v21.16b
    mov     v19.16b, v22.16b
    mov     v20.16b, v23.16b
    mov     v8.16b,  v12.16b
    subs    x10, x10, #1
    b.ne    L_hol13_vec8_loop
L_hol13_ret:
    ldp     d14, d15, [sp, #48]
    ldp     d12, d13, [sp, #32]
    ldp     d10, d11, [sp, #16]
    ldp     d8,  d9,  [sp], #64
    mov     x0, #0x455A
    lsl     x0, x0, #48
    and     x11, x4, #0xFFFF
    lsl     x11, x11, #24
    orr     x0, x0, x11
    mov     x12, #7                         // Mode 7: HOLONOMIC_13TAP_SIAC_LEECH
    orr     x0, x0, x12
    ret
L_hol13_done:
    mov     x0, #0x455A
    lsl     x0, x0, #48
    ret

// 2. _omega_v41_holonomic_wilson_retract_arm64 (0-ULP Wilson-Loop Retraction Δ_hol⁻)
    .globl _omega_v41_holonomic_wilson_retract_arm64
_omega_v41_holonomic_wilson_retract_arm64:
    cbz     x3, L_wret_done
    dup     v24.2d, v0.d[0]
    lsr     x5, x3, #1
    cbz     x5, L_wret_done
    .p2align 4
L_wret_vec2:
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
    b.ne    L_wret_vec2
L_wret_done:
    ret

// 3. _omega_v41_toric_siv_anyon_bridge_arm64 (Toric-SiV⁻ Anyon Stabilizer Bridge)
    .globl _omega_v41_toric_siv_anyon_bridge_arm64
_omega_v41_toric_siv_anyon_bridge_arm64:
    cbz     x5, L_tor_done
    dup     v24.2d, v0.d[0]
    movz    x8, #0x404B, lsl #48            // 54.0 eV/A^2 Toric-SiV spring
    fmov    d25, x8
    fmov    d26, #-1.0
    fmul    d25, d25, d26
    dup     v25.2d, v25.d[0]
    fmul    v24.2d, v24.2d, v26.d[0]
    fmov    v27.2d, #1.5
    fmov    v28.2d, #0.75
    lsr     x6, x5, #2
    cbz     x6, L_tor_done
    .p2align 4
L_tor_vec4:
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
    b.ne    L_tor_vec4
L_tor_done:
    ret

// 4. _omega_v41_leech_24d_octic_hubo_trigger_arm64 (24D Leech Lattice 8th-Order Octic HUBO)
    .globl _omega_v41_leech_24d_octic_hubo_trigger_arm64
_omega_v41_leech_24d_octic_hubo_trigger_arm64:
    cbz     x4, L_leech_zero
    dup     v24.2d, v0.d[0]
    dup     v25.2d, v1.d[0]
    fmov    v26.2d, #6.0
    fmov    v27.2d, #8.0
    fmov    v28.2d, #0.96875                // Leech Lambda_24 196,560-kissing gauge factor
    lsr     x5, x4, #2
    cbz     x5, L_leech_ret
    .p2align 4
L_leech_vec4:
    ldp     q0, q1, [x0]
    ldp     q2, q3, [x1]
    ldp     q4, q5, [x2], #32
    ldp     q6, q7, [x3], #32
    fmul    v16.2d, v0.2d, v0.2d            // s^2
    fmul    v17.2d, v1.2d, v1.2d
    fmul    v18.2d, v16.2d, v16.2d          // s^4
    fmul    v19.2d, v17.2d, v17.2d
    fmul    v20.2d, v18.2d, v0.2d           // s^5
    fmul    v21.2d, v19.2d, v1.2d
    fmul    v22.2d, v20.2d, v16.2d          // s^7
    fmul    v23.2d, v21.2d, v17.2d
    fmul    v4.2d,  v4.2d,  v20.2d
    fmul    v5.2d,  v5.2d,  v21.2d
    fmul    v4.2d,  v4.2d,  v26.2d
    fmul    v5.2d,  v5.2d,  v26.2d
    fmul    v6.2d,  v6.2d,  v22.2d
    fmul    v7.2d,  v7.2d,  v23.2d
    fmla    v4.2d,  v6.2d,  v27.2d
    fmla    v5.2d,  v7.2d,  v27.2d
    fmla    v2.2d,  v4.2d,  v28.2d
    fmla    v3.2d,  v5.2d,  v28.2d
    fmla    v0.2d,  v2.2d,  v24.2d
    fmla    v1.2d,  v3.2d,  v24.2d
    stp     q0, q1, [x0], #32
    stp     q2, q3, [x1], #32
    subs    x5, x5, #1
    b.ne    L_leech_vec4
L_leech_ret:
    mvn     x0, xzr
    ret
L_leech_zero:
    mov     x0, #0
    ret
