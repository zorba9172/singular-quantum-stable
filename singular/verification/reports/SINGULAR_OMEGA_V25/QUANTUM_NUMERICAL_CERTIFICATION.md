# QUANTUM_NUMERICAL_CERTIFICATION — 4-Tier Epistemic Precision Classification

- **Hardware**: Apple M5 (`arm64`) + Verilator RTL & Spike `RV64GC + custom-3 0x7B` Co-Simulation
- **Compiler**: Apple clang 17 (`-std=c++23 -O3 -mcpu=native`), `rustc 1.98.1`, `singular-qbe`, `singular-as`
- **Commit**: `dbe5ce924d7a6612d3b5511b715241125d539b6c`

1. **`EXACT_ALGEBRA`**: `0-ULP` ring arithmetic in $\mathbb{Z}[\zeta_{16}]$, $\mathbb{F}_2$ stabilizer tableau (`1024Q`), and `TwoSum`/3-Limb reversible $\Delta^+/\Delta^-$ retraction (`ADV-10`, `0` bit diff).
2. **`CERTIFIED_ERROR_BOUNDS`**: Temple–Kato spectral bounds, Krylov residual $\|H\psi - E\psi\|_2 \le 10^{-10}$, and `Q14-E` (`24Q`) Vidal MPS with $\varepsilon_{\text{trunc}} = 1.819\times 10^{-3} < 0.025$.
3. **`NUMERICAL_CONVERGENCE`**: Iterative Lanczos/Krylov and step-size $\Delta t \to 0$ convergence (`Q14-C`, `Q14-D`).
4. **`REFERENCE_AGREEMENT`**: Cross-solver parity ($|\Delta E_0| \le 2.66\times 10^{-15}$ vs. independent `zheev_` / NumPy `eigh`, and $2.685\times 10^{-5}$ vs. 400-step `RK4`).
- **Quarantine Policy (`Q14-F`, `32Q`)**: Flagged `REJECTED_BUDGET_EXCEEDED [DIAGNOSTIC_ONLY_UNVALIDATED]` (`TruncErr = 0.1898 > 0.025`).
