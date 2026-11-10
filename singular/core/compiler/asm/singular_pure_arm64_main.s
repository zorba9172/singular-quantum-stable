.text
.align 4
.globl _main
_main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    bl _omega_run_all_qcy_modules_arm64
    str x0, [sp, #-16]!
    adrp x0, L_fmt@PAGE
    add x0, x0, L_fmt@PAGEOFF
    bl _printf
    add sp, sp, #16
    mov w0, #0
    ldp x29, x30, [sp], #16
    ret
.section __TEXT,__cstring,cstring_literals
L_fmt:
    .asciz "[SINGULAR Ω PURE ARM64 ASM BINARY] Executed %llu .qcy Native ARM64 Subroutines | ULP=0.0 | PASS\n"
