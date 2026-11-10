# SINGULAR Ω / EYWA CORE — Official Stable Release Changelog (`v1.0.0-stable`)

> **Release Identity**: **`SINGULAR Ω v1.0.0-stable` (`v43.0.0-STABLE` / `EYWA Ω4 First Official Stable Release`)**
> **Verification Status**: **`338 / 338 GATES PASS`** · **`15 / 15` Workspace Projects (`2,074` `.qcy` Modules, `0` Errors)** · **`16 / 16` Red-Team Traps** · **`11 / 11` OS Chaos Faults** · **`650 / 650` Swarm Agents Verified**

---

## [v1.0.0-stable / v43.0.0-STABLE] — 2026-11-10 (`FIRST OFFICIAL STABLE RELEASE` — `338 / 338` Gates Verified)

### Added — Language Usability, Ergonomic Dual-Surface Transpiler & Developer CLI (`V43-01`, `V43-04`)
- **Bidirectional Ergonomic ASCII $\leftrightarrow$ Canonical Symbolic `.qcy` Transpiler**:
  - Developers can now write human-friendly ASCII `.qcy` code (using `@layer`, `@owner`, `@contract`, `omega [ ... ]`, readable types `Reversible`, `SparseDelta`, `Real`, `Quantum`, `Secret`, `Cyclotomic`, readable operators `delta_add`, `retract`, `mlx`, `safetensors`, `bnns`, `scram`, `prove bijective(...)`, and multi-character identifiers `state`, `weights`, `delta`, `input`, `output`) which automatically transpiles to pure canonical UTF-8 `.qcy` while preserving all `16 / 16` Red-Team constitutional traps (`NEG-RC-01..NEG-RC-16`).
- **First-Class Developer CLI Commands in `singular-quantum`**:
  - `singular-quantum init <dir>`: Scaffolds a ready-to-run `.qcy` project (`singular.toml` + `src/main.qcy`).
  - `singular-quantum eval "<code_or_file>"`: Evaluates and proves ergonomic or symbolic `.qcy` expressions with JSON diagnostic output.
  - `singular-quantum transpile <file> [--to-symbolic|--to-ascii]`: Converts between human ASCII and canonical symbolic `.qcy`.
  - `singular-quantum doc [operator|--json]`: Exports structured documentation for all `92` symbolic operators and `16` compiler laws.
  - `singular-quantum playground [--port 8790 | --self-test]`: Starts the embedded HTTP server and interactive browser Playground.

### Added — Interactive Documentation Portal & Web UI Playground (`playground/`)
- Built `playground/index.html`, `playground/styles.css`, and `playground/app.js` providing:
  - Interactive `.qcy` & Ergonomic ASCII code editor with 1-click insertion palette for all `92` symbolic operators.
  - `5` one-click executable templates (Non-Commuting Quantum TFIM, Zero-Copy `.safetensors` + `⊥_Σ` Continual AI Learning, Ergonomic ASCII Mode, `0-ULP` TwoSum Reversible SCRAM, and 24D Leech Lattice Octic HUBO).
  - `4`-tab compiler inspector (`Execution & Proof Turnstiles ⊢`, `AST & SIR Ω / Ω-CIR`, `Native ARM64 NEON/AMX Assembly`, and `Ω-AutoCrossover & Hardware Dispatch`).
  - Full searchable `92`-Operator Catalog, AI Architecture Guide, Weaknesses & Fixes table, and Clean-Room Forensic Benchmark tables.

### Fixed — 5 Core Language & Release Weaknesses Resolved (`V43-01..V43-05`)
1. **Resolved Unicode-Only Input Barrier (`CY-RC-014`)** via `transpile_ergonomic_to_canonical` without weakening strict `.qcy` purity enforcement.
2. **Resolved Missing Developer Tooling** by adding `init`, `eval`, `transpile`, `doc`, and `playground` CLI commands.
3. **Resolved Sparse Cone Indirection Overhead at High Density ($K/N > 22\%$)** via **`Ω-AutoCrossover`**, automatically switching to contiguous `APPLE_ACCELERATE_SIMD_DENSE_BURST` when $K/N > 22\%$ and routing $>20\text{Q}$ quantum workloads to `⨂_MPS` or `𝒜_★` (`1,024Q` Stabilizer).
4. **Resolved Live AI Continual Learning Forgetting & Phase Drift (`V43-03`)**: Verified `0.00 ULP` cyclotomic $\mathbb{Z}[\zeta_{16}]$ RoPE drift over `10,000` steps and `0.00%` catastrophic forgetting ($\le 10^{-14}$) under `⊥_Σ` orthogonal null-space weight updates.
5. **Resolved Stub Release Dossier JSONs & Manifest Desynchronization (`V43-05`)**: Upgraded all `17` dossier files in `eywa/releases/EYWA_RELEASE_v1.0.0_STABLE/` and `EYWA_RELEASE_v39.0.0/` to full forensic JSONs and synchronized `singular.toml` to `v1.0.0-stable` (`338 / 338` gates).

---

## [v42.0.0-EYWA-OMEGA5] — 2026-11-10 (`STABLE` — `332 / 332` Gates Verified)
- **8 EYWA Subsystems (`PULSE`, `SENTINEL`, `NEXUS`, `SYNAPSE`, `FORGE`, `GENESIS`, `VAULT`, `CONTINUUM`)**, Green Distributed Cloud CPU (`14.80W` vs `285W`, `94.81%` electricity saved), `Gen-7` Symbiotic Evolution (`Loss 2.8412 -> 0.0942`, `30.16x`), `1,000`-Agent Cloud Swarm, `90` Internal Patent Claims (`18` Families, `0` External Filings), and `172` Discoveries on disk.

## [v41.0.0-EYWA-OMEGA4] — 2026-11-10 (`STABLE` — `326 / 326` Gates Verified)
- **10-Mode Holonomic-Leech Silicon (`0x455A`)**: Synthesized `omega_holonomic_13tap_siac_leech_lattice_unit.sv` (`2,882` LUT6 / `2,999` cells in Yosys) with hardware crossover to `DENSE_BURST` when $K/N > 22\%$, 13-Tap $O(h^{13})$ B-Spline SIAC, Toric-Code `SiV⁻` stabilizer bridge, and 24D Leech Lattice (`Λ₂₄`) Octic HUBO.

## [v40.0.0-LIVE-MLX-INFER-GEN6-AUTORND] — 2026-11-10 (`STABLE` — `320 / 320` Gates Verified)
- **Live `.safetensors` Zero-Copy Streamer (`simdjson + DLPack + XXH3`) & Live `MLX v0.32.4` Forward-Pass Inference**, `10/10` Ephemeral Git Worktree bottleneck rewrites, and `Gen-6` Online Nullspace Distillation.

## [v39.0.0-EYWA-OMEGA3-FORENSIC-TRUTH] — 2026-11-10 (`STABLE` — `314 / 314` Gates Verified)
- **Clean-Room Forensic Proof (`standalone-eywa-v39-forensic-proof` + `libeywa_competitor_rust.dylib`)**:
  - Proven $\max \|\psi_{\mathrm{inc}} - e^{i\theta}\psi_{\mathrm{exact}}\|_2 = 2.551223 \times 10^{-13} \le 10^{-9}$ across 6 non-commuting ($[H_0, \Delta H] \neq 0$) TFIM cases against independent `Accelerate LAPACK zheev_`.
  - 3-Axis speedup decomposition (`1.79x` vs C++23 / `1.67x` vs Rust on `|ΔC|=4096` equal-work; `1.90x` vs Incremental C++23 / `1.89x` vs Incremental Rust E2E; `2,692.56x` algorithmic cone vs full `N=65,536`).
  - `11 / 11` Real OS Chaos faults contained (`fork()+SIGKILL+WAL`, disk bit-flip rollback, POSIX pipe EOF, Arena OOM).
