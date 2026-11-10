// ============================================================================
// SINGULAR Ω v35.0.0 — NATIVE ARM64 NEON (.2d) TOPOLOGICAL COBOUNDARY SINGULAR
// ARCS (Δ_edge⁺ / Δ_edge⁻ × TwoSum × 5-Tap SIAC × Hodge 1-Form), STONE-WALES
// NANOTECH BOND SURGERY & RIEMANNIAN COUNTERDIABATIC QUARTIC HUBO AI TRIGGER
// AAPCS64 Compliant: Caller-saved v0..v7, v16..v31 only (Zero v8..v15 usage).
// Enforces Constitutional Rule `SG-ARCH-400` (<= 400 physical lines).
// ============================================================================

    .section __TEXT,__text,regular,pure_instructions
    .p2align 4

// 1. _omega_v35_topological_coboundary_sarc_siac_arm64
//    x0: state, x1: err, x2: cobnd, x3: out_hodge, x4: n_cone, d0: edge_weight, d1: hodge_zeta
    .globl _omega_v35_topological_coboundary_sarc_siac_arm64
_omega_v35_topological_coboundary_sarc_siac_arm64:
    cbz     x4, L_topo_done
    dup     v24.2d, v0.d[0]             // v24 = [edge_weight, edge_weight]
    dup     v25.2d, v1.d[0]             // v25 = [hodge_zeta, hodge_zeta]
    movz    x8, #0xAAAA, lsl #0
    movk    x8, #0xAAAA, lsl #16
    movk    x8, #0xAAAA, lsl #32
    movk    x8, #0x3FDA, lsl #48        // 10.0 / 24.0
    fmov    d26, x8
    dup     v26.2d, v26.d[0]
    fmov    v27.2d, #0.25               // 6.0 / 24.0
    movz    x8, #0x5555, lsl #0
    movk    x8, #0x5555, lsl #16
    movk    x8, #0x5555, lsl #32
    movk    x8, #0x3FA5, lsl #48        // 1.0 / 24.0
    fmov    d28, x8
    dup     v28.2d, v28.d[0]
    fmov    v29.2d, #2.0

    // Prologue: update Halo [0..3] with Rank-2 Coboundary TwoSum
    ldp     q16, q17, [x0]
    ldp     q2,  q3,  [x1]
    ldp     q4,  q5,  [x2]
    fmul    v4.2d, v4.2d, v24.2d
    fmul    v5.2d, v5.2d, v24.2d
    fadd    v4.2d, v4.2d, v2.2d
    fadd    v5.2d, v5.2d, v3.2d
    fadd    v6.2d, v16.2d, v4.2d
    fadd    v7.2d, v17.2d, v5.2d
    fsub    v30.2d, v6.2d, v16.2d
    fsub    v31.2d, v7.2d, v17.2d
    fsub    v2.2d, v6.2d, v30.2d
    fsub    v3.2d, v7.2d, v31.2d
    fsub    v2.2d, v16.2d, v2.2d
    fsub    v3.2d, v17.2d, v3.2d
    fsub    v4.2d, v4.2d, v30.2d
    fsub    v5.2d, v5.2d, v31.2d
    fadd    v2.2d, v2.2d, v4.2d
    fadd    v3.2d, v3.2d, v5.2d
    mov     v16.16b, v6.16b
    mov     v17.16b, v7.16b
    stp     q16, q17, [x0]
    stp     q2,  q3,  [x1]

    add     x5, x0, #32
    add     x6, x1, #32
    add     x7, x2, #32
    mov     x9, x3
    lsr     x10, x4, #3
    cbz     x10, L_topo_ret

    .p2align 4
L_topo_vec8_loop:
    ldp     q18, q19, [x5]
    ldp     q20, q21, [x5, #32]
    ldp     q2,  q3,  [x6]
    ldp     q4,  q5,  [x6, #32]
    ldp     q6,  q7,  [x7]
    ldp     q30, q31, [x7, #32]

    fmul    v6.2d,  v6.2d,  v24.2d
    fmul    v7.2d,  v7.2d,  v24.2d
    fmul    v30.2d, v30.2d, v24.2d
    fmul    v31.2d, v31.2d, v24.2d
    fadd    v2.2d, v2.2d, v6.2d
    fadd    v3.2d, v3.2d, v7.2d
    fadd    v4.2d, v4.2d, v30.2d
    fadd    v5.2d, v5.2d, v31.2d

    fadd    v6.2d,  v18.2d, v2.2d
    fadd    v7.2d,  v19.2d, v3.2d
    fadd    v30.2d, v20.2d, v4.2d
    fadd    v31.2d, v21.2d, v5.2d

    fsub    v0.2d, v6.2d,  v18.2d
    fsub    v1.2d, v7.2d,  v19.2d
    fsub    v22.2d, v6.2d, v0.2d
    fsub    v23.2d, v7.2d, v1.2d
    fsub    v22.2d, v18.2d, v22.2d
    fsub    v23.2d, v19.2d, v23.2d
    fsub    v2.2d, v2.2d, v0.2d
    fsub    v3.2d, v3.2d, v1.2d
    fadd    v22.2d, v22.2d, v2.2d
    fadd    v23.2d, v23.2d, v3.2d

    fsub    v0.2d, v30.2d, v20.2d
    fsub    v1.2d, v31.2d, v21.2d
    fsub    v2.2d, v30.2d, v0.2d
    fsub    v3.2d, v31.2d, v1.2d
    fsub    v2.2d, v20.2d, v2.2d
    fsub    v3.2d, v21.2d, v3.2d
    fsub    v4.2d, v4.2d, v0.2d
    fsub    v5.2d, v5.2d, v1.2d
    fadd    v4.2d, v2.2d, v4.2d
    fadd    v5.2d, v3.2d, v5.2d

    mov     v18.16b, v6.16b
    mov     v19.16b, v7.16b
    mov     v20.16b, v30.16b
    mov     v21.16b, v31.16b

    stp     q18, q19, [x5], #64
    stp     q20, q21, [x5, #-32]
    stp     q22, q23, [x6], #64
    stp     q4,  q5,  [x6, #-32]
    add     x7,  x7,  #64

    ext     v1.16b, v16.16b, v17.16b, #8
    ext     v2.16b, v17.16b, v18.16b, #8
    ext     v3.16b, v18.16b, v19.16b, #8
    ext     v4.16b, v19.16b, v20.16b, #8
    ext     v5.16b, v20.16b, v21.16b, #8

    fadd    v6.2d,  v1.2d,  v2.2d
    fadd    v22.2d, v2.2d,  v3.2d
    fadd    v7.2d,  v16.2d, v18.2d
    fadd    v23.2d, v17.2d, v19.2d
    fmul    v30.2d, v17.2d, v26.2d
    fmul    v31.2d, v18.2d, v26.2d
    fmla    v30.2d, v6.2d,  v27.2d
    fmla    v31.2d, v22.2d, v27.2d
    fmla    v30.2d, v7.2d,  v28.2d
    fmla    v31.2d, v23.2d, v28.2d
    fmul    v0.2d,  v17.2d, v29.2d
    fmul    v7.2d,  v18.2d, v29.2d
    fsub    v0.2d,  v0.2d,  v6.2d
    fsub    v7.2d,  v7.2d,  v22.2d
    fmls    v30.2d, v0.2d,  v25.2d
    fmls    v31.2d, v7.2d,  v25.2d

    stp     q30, q31, [x9], #64

    fadd    v6.2d,  v3.2d,  v4.2d
    fadd    v22.2d, v4.2d,  v5.2d
    fadd    v7.2d,  v18.2d, v20.2d
    fadd    v23.2d, v19.2d, v21.2d
    fmul    v30.2d, v19.2d, v26.2d
    fmul    v31.2d, v20.2d, v26.2d
    fmla    v30.2d, v6.2d,  v27.2d
    fmla    v31.2d, v22.2d, v27.2d
    fmla    v30.2d, v7.2d,  v28.2d
    fmla    v31.2d, v23.2d, v28.2d
    fmul    v0.2d,  v19.2d, v29.2d
    fmul    v7.2d,  v20.2d, v29.2d
    fsub    v0.2d,  v0.2d,  v6.2d
    fsub    v7.2d,  v7.2d,  v22.2d
    fmls    v30.2d, v0.2d,  v25.2d
    fmls    v31.2d, v7.2d,  v25.2d

    stp     q30, q31, [x9, #-32]
    mov     v16.16b, v20.16b
    mov     v17.16b, v21.16b
    subs    x10, x10, #1
    b.ne    L_topo_vec8_loop

L_topo_ret:
    mov     x0, #0x5335
    lsl     x0, x0, #48
    and     x11, x4, #0xFFFF
    lsl     x11, x11, #24
    orr     x0, x0, x11
    orr     x0, x0, #2
    ret
L_topo_done:
    mov     x0, #0x5335
    lsl     x0, x0, #48
    ret

// 2. _omega_v35_topological_edge_retract_arm64
    .globl _omega_v35_topological_edge_retract_arm64
_omega_v35_topological_edge_retract_arm64:
    cbz     x3, L_ret_done
    fneg    d0, d0
    dup     v24.2d, v0.d[0]
    fmov    d25, xzr
    dup     v25.2d, v25.d[0]
    lsr     x4, x3, #2
    cbz     x4, L_ret_done
    .p2align 4
L_ret_loop4:
    ldp     q16, q17, [x0]
    ldp     q2,  q3,  [x1]
    ldp     q4,  q5,  [x2], #32
    fmul    v4.2d, v4.2d, v24.2d
    fmul    v5.2d, v5.2d, v24.2d
    fadd    v6.2d, v16.2d, v4.2d
    fadd    v7.2d, v17.2d, v5.2d
    fsub    v30.2d, v6.2d, v16.2d
    fsub    v31.2d, v7.2d, v17.2d
    fsub    v18.2d, v6.2d, v30.2d
    fsub    v19.2d, v7.2d, v31.2d
    fsub    v18.2d, v16.2d, v18.2d
    fsub    v19.2d, v17.2d, v19.2d
    fsub    v4.2d, v4.2d, v30.2d
    fsub    v5.2d, v5.2d, v31.2d
    fadd    v18.2d, v18.2d, v4.2d
    fadd    v19.2d, v19.2d, v5.2d
    fadd    v18.2d, v18.2d, v2.2d
    fadd    v19.2d, v19.2d, v3.2d
    fadd    v6.2d,  v6.2d,  v18.2d
    fadd    v7.2d,  v7.2d,  v19.2d
    stp     q6,  q7,  [x0], #32
    stp     q25, q25, [x1], #32
    subs    x4, x4, #1
    b.ne    L_ret_loop4
L_ret_done:
    ret

// 3. _omega_v35_nanotech_stone_wales_bond_surgery_arm64
    .globl _omega_v35_nanotech_stone_wales_bond_surgery_arm64
_omega_v35_nanotech_stone_wales_bond_surgery_arm64:
    lsr     x6, x5, #3
    cbz     x6, L_sw_done
    dup     v24.2d, v0.d[0]
    fneg    v24.2d, v24.2d
    movz    x8, #0xc046, lsl #48        // -44.0
    fmov    d25, x8
    dup     v25.2d, v25.d[0]
    fmov    d26, #-1.5
    dup     v26.2d, v26.d[0]
    fmov    d27, #0.5
    dup     v27.2d, v27.d[0]
    .p2align 4
L_sw_vec8:
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
    b.ne    L_sw_vec8
L_sw_done:
    ret

// 4. _omega_v35_riemannian_cd_quartic_hubo_trigger_arm64
    .globl _omega_v35_riemannian_cd_quartic_hubo_trigger_arm64
_omega_v35_riemannian_cd_quartic_hubo_trigger_arm64:
    mov     x8, #0
    lsr     x5, x4, #2
    cbz     x5, L_qhubo_done
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
L_qhubo_loop4:
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
    b.ne    L_qhubo_loop4
L_qhubo_done:
    mov     x0, x8
    ret
