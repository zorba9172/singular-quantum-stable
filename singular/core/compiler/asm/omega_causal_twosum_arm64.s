.text
.align 4
.globl _omega_arm64_causal_twosum_step
_omega_arm64_causal_twosum_step:
    cbz x3, L_done
L_loop:
    ldr w4, [x2], #4
    lsl x5, x4, #3
    ldr d1, [x0, x5]
    ldr d2, [x1, x5]
    fadd d3, d1, d0
    fsub d4, d3, d1
    fsub d5, d3, d4
    fsub d5, d1, d5
    fsub d6, d0, d4
    fadd d5, d5, d6
    fadd d2, d2, d5
    str d3, [x0, x5]
    str d2, [x1, x5]
    subs x3, x3, #1
    b.ne L_loop
L_done:
    ret
