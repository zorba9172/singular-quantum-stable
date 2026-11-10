# RTL_FORMAL_PROOF_REPORT — SystemVerilog Lint, CDC & 40 Formal Assertions

- **Hardware**: Apple M5 (`arm64`) + Verilator RTL & Spike `RV64GC + custom-3 0x7B` Co-Simulation
- **Compiler**: Apple clang 17 (`-std=c++23 -O3 -mcpu=native`), `rustc 1.98.1`, `singular-qbe`, `singular-as`
- **Commit**: `dbe5ce924d7a6612d3b5511b715241125d539b6c`

- **Modules Verified (`7/7`)**: `omega_decoder.sv`, `omega_state_table.sv`, `omega_dirty_unit.sv`, `omega_causal_processing_unit.sv`, `omega_riscv_custom3_decoder.sv`, `omega_sarc_near_memory_sram.sv`, `omega_twosum_cyclotomic_alu.sv`.
- **Lint & CDC**: `0` lint errors, `0` asynchronous CDC violations.
- **Formal Assertions**: `40 / 40 PASS` (`0` failures) via `sources/bin/singular-rtl-sim`.
