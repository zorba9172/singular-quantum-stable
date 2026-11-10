// ============================================================================
// SINGULAR Ω v25.2.0 — TOTAL NATIVE ARM64 NEON (.2d) ASSEMBLY SUPERKERNEL
// Strictly AAPCS64 ABI Compliant (uses caller-saved v0..v7, v16..v25 only;
// preserves callee-saved v8..v15). Single-pass sliding-window register fusion
// using `ext v19.16b, v16.16b, v17.16b, #8` (2 loads + 3 stores per 2 elems).
// ============================================================================
.text
.align 4

.globl _omega_v25_sarc_siac_harmonic_cone_arm64
.globl _omega_v25_sarc_decremental_retract_arm64
.globl _omega_v25_nanotech_atomic_bond_arc_arm64
.globl _omega_v25_qio_bifurcation_geodesic_arm64

// ----------------------------------------------------------------------------
// 1. _omega_v25_sarc_siac_harmonic_cone_arm64 (1-Pass Sliding-Window NEON .2d)
//    x0: double* state    (size: cone_len + 4)
//    x1: double* err      (size: cone_len + 4)
//    x2: double* siac_out (size: cone_len)
//    x3: size_t  cone_len
//    d0: double  delta    (Δ⁺ incremental arc perturbation)
// ----------------------------------------------------------------------------
_omega_v25_sarc_siac_harmonic_cone_arm64:
    cbz x3, L_sarc_siac_done
    dup v0.2d, v0.d[0]

    // Load exact IEEE-754 double bit patterns for 10/24, 6/24=0.25, 1/24 into v29..v31
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

    mov x5, x0
    mov x6, x1

    // Paired 256-bit Prologue: state[0..3] -> v16.2d, v17.2d
    ldp q1, q2, [x5]
    fadd v16.2d, v1.2d, v0.2d
    fadd v17.2d, v2.2d, v0.2d
    fsub v3.2d, v16.2d, v1.2d
    fsub v4.2d, v17.2d, v2.2d
    fsub v5.2d, v16.2d, v3.2d
    fsub v6.2d, v17.2d, v4.2d
    fsub v5.2d, v1.2d, v5.2d
    fsub v6.2d, v2.2d, v6.2d
    fsub v3.2d, v0.2d, v3.2d
    fsub v4.2d, v0.2d, v4.2d
    fadd v5.2d, v5.2d, v3.2d
    fadd v6.2d, v6.2d, v4.2d
    stp q16, q17, [x5], #32
    stp q5, q6, [x6], #32

    // 8-Double (64-Byte Cache-Line) Unrolled Main Loop (`ldp`/`stp` + Shared `ext`)
    lsr x7, x3, #3
    cbz x7, L_sarc_siac_rem2_check

L_fused_sarc_siac_vec8:
    ldp q1, q2, [x5]
    ldp q3, q4, [x5, #32]

    fadd v18.2d, v1.2d, v0.2d
    fadd v19.2d, v2.2d, v0.2d
    fadd v20.2d, v3.2d, v0.2d
    fadd v21.2d, v4.2d, v0.2d

    fsub v5.2d, v18.2d, v1.2d
    fsub v6.2d, v19.2d, v2.2d
    fsub v7.2d, v20.2d, v3.2d
    fsub v28.2d, v21.2d, v4.2d

    fsub v22.2d, v18.2d, v5.2d
    fsub v23.2d, v19.2d, v6.2d
    fsub v24.2d, v20.2d, v7.2d
    fsub v25.2d, v21.2d, v28.2d

    fsub v22.2d, v1.2d, v22.2d
    fsub v23.2d, v2.2d, v23.2d
    fsub v24.2d, v3.2d, v24.2d
    fsub v25.2d, v4.2d, v25.2d

    fsub v5.2d, v0.2d, v5.2d
    ext v1.16b, v16.16b, v17.16b, #8
    fsub v6.2d, v0.2d, v6.2d
    ext v2.16b, v17.16b, v18.16b, #8
    fsub v7.2d, v0.2d, v7.2d
    ext v3.16b, v18.16b, v19.16b, #8
    fsub v28.2d, v0.2d, v28.2d
    ext v4.16b, v19.16b, v20.16b, #8

    fadd v22.2d, v22.2d, v5.2d
    ext v5.16b, v20.16b, v21.16b, #8
    fadd v23.2d, v23.2d, v6.2d
    fadd v24.2d, v24.2d, v7.2d
    fadd v25.2d, v25.2d, v28.2d

    stp q18, q19, [x5]
    fadd v6.2d, v1.2d, v2.2d
    fadd v26.2d, v2.2d, v3.2d
    stp q20, q21, [x5, #32]
    fadd v28.2d, v3.2d, v4.2d
    fadd v3.2d, v4.2d, v5.2d

    stp q22, q23, [x6]
    fadd v7.2d, v16.2d, v18.2d
    fadd v27.2d, v17.2d, v19.2d
    stp q24, q25, [x6, #32]
    fadd v4.2d, v18.2d, v20.2d
    fadd v5.2d, v19.2d, v21.2d

    fmul v22.2d, v17.2d, v29.2d
    fmul v23.2d, v18.2d, v29.2d
    fmul v24.2d, v19.2d, v29.2d
    fmul v25.2d, v20.2d, v29.2d

    fmla v22.2d, v6.2d, v30.2d
    fmla v23.2d, v26.2d, v30.2d
    fmla v24.2d, v28.2d, v30.2d
    fmla v25.2d, v3.2d, v30.2d

    fmla v22.2d, v7.2d, v31.2d
    fmla v23.2d, v27.2d, v31.2d
    fmla v24.2d, v4.2d, v31.2d
    fmla v25.2d, v5.2d, v31.2d

    stp q22, q23, [x2]
    stp q24, q25, [x2, #32]

    mov v16.16b, v20.16b
    mov v17.16b, v21.16b
    add x5, x5, #64
    add x6, x6, #64
    add x2, x2, #64
    subs x7, x7, #1
    b.ne L_fused_sarc_siac_vec8

L_sarc_siac_rem2_check:
    and x7, x3, #6
    lsr x7, x7, #1
    cbz x7, L_sarc_siac_odd_tail

L_fused_sarc_siac_vec2:
    ldr q1, [x5]
    fadd v18.2d, v1.2d, v0.2d
    fsub v4.2d, v18.2d, v1.2d
    fsub v5.2d, v18.2d, v4.2d
    fsub v5.2d, v1.2d, v5.2d
    fsub v6.2d, v0.2d, v4.2d
    fadd v5.2d, v5.2d, v6.2d
    str q18, [x5], #16
    str q5, [x6], #16

    ext v19.16b, v16.16b, v17.16b, #8
    ext v20.16b, v17.16b, v18.16b, #8
    fadd v6.2d, v19.2d, v20.2d
    fadd v7.2d, v16.2d, v18.2d
    fmul v21.2d, v17.2d, v29.2d
    fmla v21.2d, v6.2d, v30.2d
    fmla v21.2d, v7.2d, v31.2d
    str q21, [x2], #16

    mov v16.16b, v17.16b
    mov v17.16b, v18.16b
    subs x7, x7, #1
    b.ne L_fused_sarc_siac_vec2

L_sarc_siac_odd_tail:
    tbz x3, #0, L_sarc_siac_done
    ldr d1, [x5]
    fadd d18, d1, d0
    fsub d4, d18, d1
    fsub d5, d18, d4
    fsub d5, d1, d5
    fsub d6, d0, d4
    fadd d5, d5, d6
    str d18, [x5]
    str d5, [x6]
    mov d1, v16.d[0]
    mov d2, v16.d[1]
    mov d3, v17.d[0]
    mov d4, v17.d[1]
    fadd d6, d2, d4
    fadd d7, d1, d18
    fmul d21, d3, d29
    fmadd d21, d6, d30, d21
    fmadd d21, d7, d31, d21
    str d21, [x2]

L_sarc_siac_done:
    ret

// ----------------------------------------------------------------------------
// 2. _omega_v25_sarc_decremental_retract_arm64 (NEON .2d Vectorized, AAPCS64)
//    x0: double* state
//    x1: double* err
//    x2: size_t  total_len
//    d0: double  delta (Δ⁻ decremental retraction)
// ----------------------------------------------------------------------------
_omega_v25_sarc_decremental_retract_arm64:
    cbz x2, L_retract_done
    dup v0.2d, v0.d[0]
    fneg v7.2d, v0.2d
    movi v16.2d, #0
    lsr x3, x2, #3
    cbz x3, L_retract_rem2_check

L_retract_vec8:
    ldp q1, q2, [x0]
    ldp q3, q4, [x0, #32]
    ldp q17, q18, [x1]
    ldp q19, q20, [x1, #32]
    fsub v21.2d, v1.2d, v0.2d
    fsub v22.2d, v2.2d, v0.2d
    fsub v23.2d, v3.2d, v0.2d
    fsub v24.2d, v4.2d, v0.2d
    fsub v25.2d, v21.2d, v1.2d
    fsub v26.2d, v22.2d, v2.2d
    fsub v27.2d, v23.2d, v3.2d
    fsub v28.2d, v24.2d, v4.2d
    fsub v29.2d, v21.2d, v25.2d
    fsub v30.2d, v22.2d, v26.2d
    fsub v31.2d, v23.2d, v27.2d
    fsub v5.2d, v24.2d, v28.2d
    fsub v29.2d, v1.2d, v29.2d
    fsub v30.2d, v2.2d, v30.2d
    fsub v31.2d, v3.2d, v31.2d
    fsub v5.2d, v4.2d, v5.2d
    fsub v25.2d, v7.2d, v25.2d
    fsub v26.2d, v7.2d, v26.2d
    fsub v27.2d, v7.2d, v27.2d
    fsub v28.2d, v7.2d, v28.2d
    fadd v29.2d, v29.2d, v25.2d
    fadd v30.2d, v30.2d, v26.2d
    fadd v31.2d, v31.2d, v27.2d
    fadd v5.2d, v5.2d, v28.2d
    fadd v29.2d, v29.2d, v17.2d
    fadd v30.2d, v30.2d, v18.2d
    fadd v31.2d, v31.2d, v19.2d
    fadd v5.2d, v5.2d, v20.2d
    fadd v21.2d, v21.2d, v29.2d
    fadd v22.2d, v22.2d, v30.2d
    fadd v23.2d, v23.2d, v31.2d
    fadd v24.2d, v24.2d, v5.2d
    stp q21, q22, [x0], #32
    stp q23, q24, [x0], #32
    stp q16, q16, [x1], #32
    stp q16, q16, [x1], #32
    subs x3, x3, #1
    b.ne L_retract_vec8

L_retract_rem2_check:
    and x3, x2, #6
    lsr x3, x3, #1
    cbz x3, L_retract_tail

L_retract_vec2:
    ldr q1, [x0]
    ldr q2, [x1]
    fsub v3.2d, v1.2d, v0.2d
    fsub v4.2d, v3.2d, v1.2d
    fsub v5.2d, v3.2d, v4.2d
    fsub v5.2d, v1.2d, v5.2d
    fsub v6.2d, v7.2d, v4.2d
    fadd v5.2d, v5.2d, v6.2d
    fadd v5.2d, v5.2d, v2.2d
    fadd v3.2d, v3.2d, v5.2d
    str q3, [x0], #16
    str q16, [x1], #16
    subs x3, x3, #1
    b.ne L_retract_vec2

L_retract_tail:
    tbz x2, #0, L_retract_done
    ldr d1, [x0]
    ldr d2, [x1]
    fsub d3, d1, d0
    fsub d4, d3, d1
    fsub d5, d3, d4
    fsub d5, d1, d5
    fsub d6, d7, d4
    fadd d5, d5, d6
    fadd d5, d5, d2
    fadd d3, d3, d5
    str d3, [x0]
    str d16, [x1]

L_retract_done:
    ret

// ----------------------------------------------------------------------------
// 3. _omega_v25_nanotech_atomic_bond_arc_arm64 (NEON .2d Cubic Morse Force)
// ----------------------------------------------------------------------------
_omega_v25_nanotech_atomic_bond_arc_arm64:
    cbz x3, L_nano_done
    dup v0.2d, v0.d[0]
    fneg v0.2d, v0.2d
    fmov v6.2d, #1.5
    fmov v7.2d, #0.5
    lsr x4, x3, #1
    cbz x4, L_nano_tail

L_nano_vec2:
    ldr q1, [x0], #16
    ldr q2, [x1], #16
    fsub v3.2d, v1.2d, v2.2d
    fmul v4.2d, v3.2d, v3.2d
    mov v5.16b, v3.16b
    fmls v5.2d, v4.2d, v6.2d
    fmul v4.2d, v4.2d, v3.2d
    fmla v5.2d, v4.2d, v7.2d
    fmul v5.2d, v5.2d, v0.2d
    str q5, [x2], #16
    subs x4, x4, #1
    b.ne L_nano_vec2

L_nano_tail:
    tbz x3, #0, L_nano_done
    ldr d1, [x0]
    ldr d2, [x1]
    fsub d3, d1, d2
    fmul d4, d3, d3
    fmsub d5, d4, d6, d3
    fmul d4, d4, d3
    fmadd d5, d4, d7, d5
    fmul d5, d5, d0
    str d5, [x2]

L_nano_done:
    ret

// ----------------------------------------------------------------------------
// 4. _omega_v25_qio_bifurcation_geodesic_arm64 (NEON .2d Vectorized)
// ----------------------------------------------------------------------------
_omega_v25_qio_bifurcation_geodesic_arm64:
    cbz x4, L_qio_done
    dup v0.2d, v0.d[0]
    fmov v6.2d, #1.0
    fneg v7.2d, v6.2d
    lsr x5, x4, #1
    cbz x5, L_qio_tail

L_qio_vec2:
    ldr q1, [x0]
    ldr q2, [x1]
    ldr q3, [x2], #16
    ldr q4, [x3], #16
    fmul v5.2d, v3.2d, v4.2d
    fmls v2.2d, v5.2d, v0.2d
    fmla v1.2d, v2.2d, v0.2d
    fminnm v1.2d, v1.2d, v6.2d
    fmaxnm v1.2d, v1.2d, v7.2d
    str q1, [x0], #16
    str q2, [x1], #16
    subs x5, x5, #1
    b.ne L_qio_vec2

L_qio_tail:
    tbz x4, #0, L_qio_done
    ldr d1, [x0]
    ldr d2, [x1]
    ldr d3, [x2]
    ldr d4, [x3]
    fmul d5, d3, d4
    fmsub d2, d5, d0, d2
    fmadd d1, d2, d0, d1
    fminnm d1, d1, d6
    fmaxnm d1, d1, d7
    str d1, [x0]
    str d2, [x1]

L_qio_done:
    ret
