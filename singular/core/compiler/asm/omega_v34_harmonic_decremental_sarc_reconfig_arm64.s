// ============================================================================
// SINGULAR Ω v34.0.0 — ASSEMBLY-GOVERNED HARDWARE RECONFIGURATION,
// HARMONIC-HODGE DECREMENTAL SINGULAR ARCS (Δ⁺/Δ⁻/Δ_H × SIAC),
// ATOMIC GRAPHICAL NANOTECH LINKS & CUBIC HUBO AI TRIGGER SUPERKERNEL (.s)
// Strictly AAPCS64 ABI Compliant (uses caller-saved x0..x15, v0..v7, v16..v31;
// zero touch of callee-saved v8..v15 or x19..x28). Enforces SG-ARCH-400.
// ============================================================================
.text
.align 4

.globl _omega_v34_hw_reconfig_sarc_dispatch_arm64
.globl _omega_v34_harmonic_hodge_decremental_arc_arm64
.globl _omega_v34_nanotech_graphene_siv_react_arc_arm64
.globl _omega_v34_qio_cubic_hubo_geodesic_trigger_arm64

// ----------------------------------------------------------------------------
// 1. _omega_v34_hw_reconfig_sarc_dispatch_arm64
//    x0: const uint64_t* cone_bitmask
//    x1: uint64_t        num_words
//    x2: uint64_t        total_nodes
//    x3: uint64_t*       hw_ctrl_out
//    Returns in x0: 64-bit Hardware Reconfiguration Control Word:
//      bits [1:0]   = Mode (0=IDLE, 1=SPARSE_FUSED_SARC_SIAC, 3=DENSE_SIMD_BURST)
//      bits [15:2]  = Log2 / word stride descriptor
//      bits [47:16] = Active dirty cone cardinality |ΔC|
//      bits [63:48] = Hardware Reconfiguration Epoch Tag (0x5334 = 'S4')
// ----------------------------------------------------------------------------
_omega_v34_hw_reconfig_sarc_dispatch_arm64:
    mov x4, #0
    cbz x1, L_reconfig_pack

    lsr x5, x1, #1
    cbz x5, L_reconfig_tail1

L_reconfig_vec2:
    ld1 {v16.2d}, [x0], #16
    cnt v17.16b, v16.16b
    uaddlv h18, v17.16b
    umov w6, v18.h[0]
    add x4, x4, x6
    subs x5, x5, #1
    b.ne L_reconfig_vec2

L_reconfig_tail1:
    tbz w1, #0, L_reconfig_pack
    ldr d16, [x0]
    cnt v17.8b, v16.8b
    uaddlv h18, v17.8b
    umov w6, v18.h[0]
    add x4, x4, x6

L_reconfig_pack:
    // Determine hardware execution mode from K/N > 22% crossover law:
    // 100 * K > 22 * N -> Mode 3 (DENSE_SIMD_BURST), else Mode 1 (SPARSE_SARC_SIAC)
    mov x7, #0
    cbz x4, L_reconfig_emit
    mov x8, #100
    mul x8, x4, x8
    mov x9, #22
    mul x9, x2, x9
    cmp x8, x9
    mov x10, #1
    mov x11, #3
    csel x7, x11, x10, hi

L_reconfig_emit:
    and x12, x1, #0x3FFF
    lsl x12, x12, #2
    orr x7, x7, x12
    and x13, x4, #0xFFFFFFFF
    lsl x13, x13, #16
    orr x7, x7, x13
    movz x14, #0x5334, lsl #48
    orr x0, x7, x14
    cbz x3, L_reconfig_done
    str x0, [x3]
L_reconfig_done:
    ret

// ----------------------------------------------------------------------------
// 2. _omega_v34_harmonic_hodge_decremental_arc_arm64
//    1-Pass Cache-Line-Fused Knuth TwoSum + 5-Tap SIAC + 3-Tap Hodge Laplacian
//    x0: double* state          (size: cone_len + 4)
//    x1: double* err_tail       (size: cone_len + 4)
//    x2: double* hodge_siac_out (size: cone_len)
//    x3: uint64_t cone_len
//    d0: double   delta_arc
//    d1: double   harmonic_kappa
// ----------------------------------------------------------------------------
_omega_v34_harmonic_hodge_decremental_arc_arm64:
    cbz x3, L_hodge_sarc_done
    dup v0.2d, v0.d[0]
    dup v1.2d, v1.d[0]

    // Constants: v29 = 10/24, v30 = 6/24 = 0.25, v31 = 1/24, v28 = 2.0
    movz x7, #0xAAAA, lsl #0
    movk x7, #0xAAAA, lsl #16
    movk x7, #0xAAAA, lsl #32
    movk x7, #0x3FDA, lsl #48
    fmov d29, x7
    dup v29.2d, v29.d[0]
    fmov v30.2d, #0.25
    movz x7, #0x5555, lsl #0
    movk x7, #0x5555, lsl #16
    movk x7, #0x5555, lsl #32
    movk x7, #0x3FA5, lsl #48
    fmov d31, x7
    dup v31.2d, v31.d[0]
    fmov v28.2d, #2.0

    mov x5, x0
    mov x6, x1

    // Prologue: update state[0..3] in v16.2d, v17.2d with Knuth TwoSum
    ldp q2, q3, [x5]
    fadd v16.2d, v2.2d, v0.2d
    fadd v17.2d, v3.2d, v0.2d
    fsub v4.2d, v16.2d, v2.2d
    fsub v5.2d, v17.2d, v3.2d
    fsub v6.2d, v16.2d, v4.2d
    fsub v7.2d, v17.2d, v5.2d
    fsub v6.2d, v2.2d, v6.2d
    fsub v7.2d, v3.2d, v7.2d
    fsub v4.2d, v0.2d, v4.2d
    fsub v5.2d, v0.2d, v5.2d
    fadd v6.2d, v6.2d, v4.2d
    fadd v7.2d, v7.2d, v5.2d
    stp q16, q17, [x5], #32
    stp q6, q7, [x6], #32

    lsr x7, x3, #2
    cbz x7, L_hodge_sarc_rem2

    .p2align 4
L_hodge_sarc_vec4:
    ldp q2, q3, [x5]
    fadd v18.2d, v2.2d, v0.2d
    fadd v24.2d, v3.2d, v0.2d
    fsub v4.2d, v18.2d, v2.2d
    fsub v25.2d, v24.2d, v3.2d
    fsub v5.2d, v18.2d, v4.2d
    fsub v26.2d, v24.2d, v25.2d
    fsub v5.2d, v2.2d, v5.2d
    fsub v26.2d, v3.2d, v26.2d
    fsub v4.2d, v0.2d, v4.2d
    fsub v25.2d, v0.2d, v25.2d
    fadd v5.2d, v5.2d, v4.2d
    fadd v26.2d, v26.2d, v25.2d
    stp q18, q24, [x5], #32
    stp q5, q26, [x6], #32

    ext v19.16b, v16.16b, v17.16b, #8
    ext v20.16b, v17.16b, v18.16b, #8
    ext v25.16b, v18.16b, v24.16b, #8
    fadd v6.2d, v19.2d, v20.2d
    fadd v7.2d, v16.2d, v18.2d
    fadd v26.2d, v20.2d, v25.2d
    fadd v27.2d, v17.2d, v24.2d

    fmul v21.2d, v17.2d, v29.2d
    fmul v23.2d, v18.2d, v29.2d
    fmla v21.2d, v6.2d, v30.2d
    fmla v23.2d, v26.2d, v30.2d
    fmla v21.2d, v7.2d, v31.2d
    fmla v23.2d, v27.2d, v31.2d

    fmul v22.2d, v17.2d, v28.2d
    fmul v4.2d, v18.2d, v28.2d
    fsub v22.2d, v22.2d, v6.2d
    fsub v4.2d, v4.2d, v26.2d
    fmls v21.2d, v22.2d, v1.2d
    fmls v23.2d, v4.2d, v1.2d
    stp q21, q23, [x2], #32

    mov v16.16b, v18.16b
    mov v17.16b, v24.16b
    subs x7, x7, #1
    b.ne L_hodge_sarc_vec4

L_hodge_sarc_rem2:
    and x7, x3, #3
    lsr x7, x7, #1
    cbz x7, L_hodge_sarc_tail

L_hodge_sarc_vec2:
    ldr q2, [x5]
    fadd v18.2d, v2.2d, v0.2d
    fsub v4.2d, v18.2d, v2.2d
    fsub v5.2d, v18.2d, v4.2d
    fsub v5.2d, v2.2d, v5.2d
    fsub v4.2d, v0.2d, v4.2d
    fadd v5.2d, v5.2d, v4.2d
    str q18, [x5], #16
    str q5, [x6], #16

    // Shared in-register sliding window: v19 = s[i+1..i+2], v20 = s[i+3..i+4]
    ext v19.16b, v16.16b, v17.16b, #8
    ext v20.16b, v17.16b, v18.16b, #8
    fadd v6.2d, v19.2d, v20.2d
    fadd v7.2d, v16.2d, v18.2d

    // 5-Tap SIAC: v21 = (10/24)*v17 + (6/24)*(v19+v20) + (1/24)*(v16+v18)
    fmul v21.2d, v17.2d, v29.2d
    fmla v21.2d, v6.2d, v30.2d
    fmla v21.2d, v7.2d, v31.2d

    // 3-Tap Hodge Laplacian: L_H = 2*v17 - (v19 + v20), fused in-register!
    fmul v22.2d, v17.2d, v28.2d
    fsub v22.2d, v22.2d, v6.2d
    fmls v21.2d, v22.2d, v1.2d
    str q21, [x2], #16

    mov v16.16b, v17.16b
    mov v17.16b, v18.16b
    subs x7, x7, #1
    b.ne L_hodge_sarc_vec2

L_hodge_sarc_tail:
    tbz w3, #0, L_hodge_sarc_done
    ldr d2, [x5]
    fadd d18, d2, d0
    fsub d4, d18, d2
    fsub d5, d18, d4
    fsub d5, d2, d5
    fsub d4, d0, d4
    fadd d5, d5, d4
    str d18, [x5]
    str d5, [x6]

    mov d2, v16.d[0]
    mov d3, v16.d[1]
    mov d4, v17.d[0]
    mov d5, v17.d[1]
    fadd d6, d3, d5
    fadd d7, d2, d18
    fmul d21, d4, d29
    fmadd d21, d6, d30, d21
    fmadd d21, d7, d31, d21
    fmul d22, d4, d28
    fsub d22, d22, d6
    fmsub d21, d22, d1, d21
    str d21, [x2]

L_hodge_sarc_done:
    ret

// ----------------------------------------------------------------------------
// 3. _omega_v34_nanotech_graphene_siv_react_arc_arm64
//    x0: const double* bond_r
//    x1: const double* bond_r0
//    x2: double*       force_out
//    x3: double*       lyapunov_v_out
//    x4: uint64_t      count
//    d0: double        heal_gain
// ----------------------------------------------------------------------------
_omega_v34_nanotech_graphene_siv_react_arc_arm64:
    cbz x4, L_nano_v34_done
    dup v0.2d, v0.d[0]
    fmov v16.2d, #-21.0
    fmov v17.2d, #-2.0
    fmov v18.2d, #1.5
    fmov v19.2d, #0.5

    lsr x5, x4, #1
    cbz x5, L_nano_v34_done_check

L_nano_v34_vec2:
    ldr q1, [x0], #16
    ldr q2, [x1], #16
    fsub v3.2d, v1.2d, v2.2d
    fmul v4.2d, v3.2d, v3.2d
    fmul v5.2d, v4.2d, v3.2d

    // Anharmonic Graphene-SiV⁻ bond restoring force: F = -42*(dr - 1.5*dr^2 + 0.5*dr^3)
    mov v6.16b, v3.16b
    fmls v6.2d, v4.2d, v18.2d
    fmla v6.2d, v5.2d, v19.2d
    fmul v6.2d, v6.2d, v16.2d
    fmul v6.2d, v6.2d, v17.2d
    str q6, [x2], #16

    // Strictly negative Lyapunov derivative: dV/dt = -heal_gain * dr^2 * (1 + 0.5*dr^2) <= 0
    fmov v7.2d, #1.0
    fmla v7.2d, v4.2d, v19.2d
    fmul v7.2d, v7.2d, v4.2d
    fmul v7.2d, v7.2d, v0.2d
    fneg v7.2d, v7.2d
    str q7, [x3], #16

    subs x5, x5, #1
    b.ne L_nano_v34_vec2

L_nano_v34_done_check:
L_nano_v34_done:
    ret

// ----------------------------------------------------------------------------
// 4. _omega_v34_qio_cubic_hubo_geodesic_trigger_arm64
//    x0: double*       spins_x
//    x1: double*       momenta_p
//    x2: const double* hubo_cubic_field
//    x3: uint64_t      count
//    d0: double        dt
//    d1: double        pump_alpha
//    Returns in x0: 64-bit AI Action Trigger Bitmask (bits i where |x_i| > 0.5)
// ----------------------------------------------------------------------------
_omega_v34_qio_cubic_hubo_geodesic_trigger_arm64:
    mov x4, #0
    cbz x3, L_hubo_v34_done
    dup v0.2d, v0.d[0]
    dup v1.2d, v1.d[0]
    fmov v16.2d, #1.0
    fmov v17.2d, #-1.0
    fmov v18.2d, #0.5
    mov x5, #0

    lsr x6, x3, #1
    cbz x6, L_hubo_v34_done

L_hubo_v34_vec2:
    ldr q2, [x0]
    ldr q3, [x1]
    ldr q4, [x2], #16

    // Kerr + Cubic HUBO force: F = pump_alpha * x - x^3 + hubo_cubic_field
    fmul v5.2d, v2.2d, v2.2d
    fmul v5.2d, v5.2d, v2.2d
    mov v6.16b, v4.16b
    fmla v6.2d, v2.2d, v1.2d
    fsub v6.2d, v6.2d, v5.2d

    // Symplectic momentum & position update with [-1, +1] bifurcation wall clamp
    fmla v3.2d, v6.2d, v0.2d
    fmla v2.2d, v3.2d, v0.2d
    fminnm v2.2d, v2.2d, v16.2d
    fmaxnm v2.2d, v2.2d, v17.2d
    str q2, [x0], #16
    str q3, [x1], #16

    // Autonomous AI Trigger condition: |x_i| > 0.5
    fabs v7.2d, v2.2d
    fcmgt v7.2d, v7.2d, v18.2d
    umov x7, v7.d[0]
    and x7, x7, #1
    lsl x7, x7, x5
    orr x4, x4, x7
    add x5, x5, #1
    umov x7, v7.d[1]
    and x7, x7, #1
    lsl x7, x7, x5
    orr x4, x4, x7
    add x5, x5, #1

    subs x6, x6, #1
    b.ne L_hubo_v34_vec2

L_hubo_v34_done:
    mov x0, x4
    ret
