# ISA_SEMANTICS — RISC-V `custom-3` (`0x7B`) Singular Arc Machine

- **Hardware**: Apple M5 (`arm64`) + Verilator RTL & Spike `RV64GC + custom-3 0x7B` Co-Simulation
- **Compiler**: Apple clang 17 (`-std=c++23 -O3 -mcpu=native`), `rustc 1.98.1`, `singular-qbe`, `singular-as`
- **Commit**: `dbe5ce924d7a6612d3b5511b715241125d539b6c`

- `omega.track rd, rs1, rs2` (`0x02c5857b`): Register dependency bit `rs2` in `deps_mask[rs1]`.
- `omega.dirty rd, rs1, rs2` (`0x02c5957b`): Verify `rs1 < 1024` & `version[rs1] == rs2`, increment version, set `dirty=1`.
- `omega.prop rd, rs1, rs2` (`0x02c5a57b`): Return 64-bit causal cone `deps_mask[rs1]` (`0x40000080080`).
- `omega.valid rd, rs1, rs2` (`0x02c5b57b`): Validate SARC certificate (`131,072` encodings, `133` illegal rejected).
