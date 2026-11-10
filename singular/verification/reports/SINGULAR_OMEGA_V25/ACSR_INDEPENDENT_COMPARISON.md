# ACSR_INDEPENDENT_COMPARISON — Unseen Hamiltonian Series (`seed=20262509ULL`) at Identical Tolerance ($10^{-10}$)

- **Hardware**: Apple M5 (`arm64`) + Verilator RTL & Spike `RV64GC + custom-3 0x7B` Co-Simulation
- **Compiler**: Apple clang 17 (`-std=c++23 -O3 -mcpu=native`), `rustc 1.98.1`, `singular-qbe`, `singular-as`
- **Commit**: `dbe5ce924d7a6612d3b5511b715241125d539b6c`

| Solver Method | Total Matvecs (8 Unseen Steps) | End-to-End Time (ms, incl. Maintenance) | Speedup vs Cold | Speedup vs Recycled Krylov | Max Residual $\|r\|_2$ |
|---|---:|---:|---:|---:|---:|
| `Cold Matrix-Free Lanczos` | `328` | `626.834834` | `1.00x` | `-` | `0.000004` |
| `1-Vector Warm-Start Lanczos` | `73` | `64.098792` | `9.779199x` | `-` | `0.000005` |
| `Advanced Thick-Restart Recycled Krylov (r=8)` | `111` | `129.350458` | `4.846019x` | `1.00x` | `0.000005` |
| **`Full ACSR (W_r^(0) + Sparse ΔH U_r, r=8)`** | **`47`** | **`103.143543`** | **`6.077306x`** | **`1.254082x`** | `0.000005` |
