.text
.align 4
.globl _omega_v22_riemann_arc_step
.globl _omega_v22_tokamak_pedestal_step
.globl _omega_v22_factorized_gemv_step
_omega_v22_riemann_arc_step:
    cbz x3, L_r_done
L_r_loop:
    ldr d1, [x0]
    ldr d2, [x1], #8
    ldr d3, [x2], #8
    fmul d4, d2, d3
    fmul d4, d4, d0
    fsub d1, d1, d4
    str d1, [x0], #8
    subs x3, x3, #1
    b.ne L_r_loop
L_r_done:
    ret
_omega_v22_tokamak_pedestal_step:
    cbz x2, L_t_done
L_t_loop:
    ldr d1, [x0]
    ldr d2, [x1]
    fadd d3, d1, d0
    fsub d4, d3, d1
    fsub d5, d3, d4
    fsub d5, d1, d5
    fsub d6, d0, d4
    fadd d5, d5, d6
    fadd d2, d2, d5
    str d3, [x0], #8
    str d2, [x1], #8
    subs x2, x2, #1
    b.ne L_t_loop
L_t_done:
    ret
_omega_v22_factorized_gemv_step:
    cbz x2, L_g_done
L_g_loop:
    ldr d1, [x0]
    ldr d2, [x1], #8
    fmadd d1, d2, d0, d1
    str d1, [x0], #8
    subs x2, x2, #1
    b.ne L_g_loop
L_g_done:
    ret
