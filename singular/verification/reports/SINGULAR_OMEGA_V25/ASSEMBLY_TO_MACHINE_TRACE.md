# ASSEMBLY_TO_MACHINE_TRACE — `.qcy -> .sga -> .ssa -> ELF 0x7B`

- **Hardware**: Apple M5 (`arm64`) + Verilator RTL & Spike `RV64GC + custom-3 0x7B` Co-Simulation
- **Compiler**: Apple clang 17 (`-std=c++23 -O3 -mcpu=native`), `rustc 1.98.1`, `singular-qbe`, `singular-as`
- **Commit**: `dbe5ce924d7a6612d3b5511b715241125d539b6c`

```text
[1] .sga Source : Ω.TRACK state[42], 7 -> Ω.DIRTY state[42], 0 -> affected ← Ω.PROP(state, 42)
[2] Machine Hex : 0x02c5857b (omega.track) -> 0x02c5957b (omega.dirty) -> 0x02c5a57b (omega.prop)
[3] 4-Way State : version[42]=1, dirty[42]=1, rd_cone_mask=0x0000040000080080 (Ref == QBE == Spike == RTL)
```
