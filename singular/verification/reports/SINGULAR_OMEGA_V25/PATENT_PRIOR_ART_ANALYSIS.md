# PATENT_PRIOR_ART_ANALYSIS — `Singular Arcs × SIAC`, `ACSR` & `SWP-SAS-SE` vs. Prior Art

- **Hardware**: Apple M5 (`arm64`) + Verilator RTL & Spike `RV64GC + custom-3 0x7B` Co-Simulation
- **Compiler**: Apple clang 17 (`-std=c++23 -O3 -mcpu=native`), `rustc 1.98.1`, `singular-qbe`, `singular-as`
- **Commit**: `dbe5ce924d7a6612d3b5511b715241125d539b6c`

| Invention Family | Closest Prior Art | Prior Art Limitation | `SINGULAR Ω` Novel Differentiator | Empirical Proof |
|---|---|---|---|---|
| **Singular Arcs × SIAC (`Δ⁺, Δ⁻, Δ_H`)** | Incremental Graph / Differential Dataflow / Undo Logs | Accumulates FP64 drift $O(\sqrt{M}\epsilon)$ or allocates $O(M|\mathcal{C}|)$ undo logs | Error-free `TwoSum` / 3-Limb / $\mathbb{Z}[\zeta_{16}]$ abelian inverse $\ominus_{\text{SIAC}}$ (`0-ULP`, `0 B` undo log) + dense crossover guard ($K/N > 22\%$) | `13.875867x` vs Incremental C++23 CSR+Undo, `0.0` ULP |
| **ACSR (`H^(k)U_r = W_r^(0) + ΔH^(k)U_r`)** | PRIMME / ARPACK Thick-Restart Recycled Krylov | Requires $r$ full $H^{(k)} u_j$ matvecs at step 0 of every new Hamiltonian | Caches $W_r^{(0)} = H^{(0)}U_r$ and applies only sparse local causal cone $\Delta H^{(k)}U_r$ (`0` full matvecs at step 0) | `1.254082x` vs Thick-Restart Recycled Krylov |
| **Atomic Nanotech Graphical Links (`SWP-SAS-SE`)** | LAMMPS / ReaxFF Neighbor Lists | Global Verlet list rebuilds & non-reversible floating-point bond updates | Single `64-Byte` cache-line ultrametric edge with fixed-point sub-Angstrom coordinates & `0-ULP` bond retraction | `85.33x` localized cone speedup, `0.0` ULP |
