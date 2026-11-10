# OMEGA_COMPILER_AND_ASSEMBLER_AUDIT — Forensic Audit of `sources/` (`QBE`, `TinyCC`, `AsmJit`, `singular-as`)

- **Role Separation**: `sources/qbe` = Bootstrap SSA Backend (`ARM64` & `RV64`); `sources/tinycc` = Fast C/ASM Encoder & Test Engine; `sources/asmjit` = AArch64 Runtime JIT only (not used for RISC-V).
- **Reproducible Patches**: `sources/patches/qbe-singular-v23.patch`, `tinycc-singular-v24.patch`, `asmjit-singular-v23.patch`.
- **Heterogeneous Lowering**: Verified `20` `.qcy` modules producing `14` distinct SSA graphs.
