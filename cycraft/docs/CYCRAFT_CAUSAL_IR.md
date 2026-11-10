# CYCRAFT_CAUSAL_IR.md — `SIR Ω` / `Ω-CIR` Causal Hypergraph IR & Ising/QUBO Scheduler

## 1. Why `SIR Ω` (`Ω-CIR`) Transcends One-Way SSA IRs

Standard SSA-based IRs (LLVM IR, MLIR `linalg`/`arith`) represent **one-way dataflow values**: once an SSA value is computed, the IR discards the causal dependency cone, the $\mathbb{Z}^7$ SI dimension lattice, the Fréchet-Leibniz forward perturbation rule (`⊕`), the 0-ULP Knuth `TwoSum` retraction rule (`↶`), and exact cyclotomic ring structure (`ℤ[ζ₁₆, 36]`).

**`SIR Ω` (`Ω-CIR`)** represents a `.qcy` module as a **Bitemporal Multi-Type Causal Hypergraph** $\mathcal{G} = (\mathcal{V}_{\Omega}, \mathcal{E}_{\leftarrow}, \mathcal{P}_{\vdash})$:

```qcy
§λ("core")
§ω("singular.quantum.ir")
§κ("SG-OMEGA-CIR-HYPERGRAPH-001")

Ω ⟦
  Ψ_0 : ℝ[16, 64],
  W_0 : ℝ[64, 64],
  δ_0 : Δℝ[16],
  Λ_0 : 𝕋[6, 6],
  Α_0 : ℤ[ζ₁₆, 36]
⟧ {
  Ψ_1 ← (Ψ_0 ⋅ W_0) ⊕ δ_0
  Ψ_retracted ← Ψ_1 ↶ δ_0
  Ψ_bridged ← ⛨(Ψ_retracted) ↦ Λ_0
  Α_1 ← ℋ_ℤ(𝒰_π8(Α_0)) ⨂_TM Λ_0
  ⊢ ⇄(Ψ_0, Ψ_1)
  ⊢ |Ψ_1| < ∞
  ⊢ Α_0 ≈ ⟨ψ_0|𝒰_0|ψ_0⟩
}
```

Lowered `Ω-CIR` hypergraph descriptor:
```text
  %manifold.0 = omega_cir.cell @Ψ_0 : ℝ[16, 64], align(64B, offset=0)
  %manifold.1 = omega_cir.cell @W_0 : ℝ[64, 64], align(64B, offset=8192)
  %manifold.2 = omega_cir.delta @δ_0 : Δℝ[16], active_cone(k=16)
  %manifold.3 = omega_cir.torus @Λ_0 : 𝕋[6, 6], partition(36)
  %manifold.4 = omega_cir.cyclotomic @Α_0 : ℤ[ζ₁₆, 36], exact_ring(zeta16)
  %arc.0      = omega_cir.siac_arc (%manifold.0, %manifold.1, %manifold.2)
      forward_op   : Ψ_1 ← (Ψ_0 ⋅ W_0) ⊕ δ_0
      retract_op   : Ψ_retracted ← Ψ_1 ↶ δ_0 (Knuth TwoSum 0-ULP, 0-Byte Log)
      scram_anchor : ⛨(Ψ_retracted) ↦ Λ_0
      proof_gate   : ⊢ ⇄(Ψ_0, Ψ_1) ∧ (|Ψ_1| < ∞)
```

---

## 2. Automatic Fréchet-Leibniz Delta (`⊕`) & 0-ULP Retraction (`↶`) Lowering

For every symbolic causal arc transition in `Ω-CIR`, the compiler synthesizes three coupled execution paths:
1. **Incremental Singular Arc (`Δ⁺`, `⊕`)**:
   $$\Delta \Psi = J_F(\Psi_0) \cdot \delta_0 + \mathcal{R}_2(\Psi_0, \delta_0), \quad \Psi_1 \leftarrow \Psi_0 \oplus \delta_0$$
2. **Decremental Singular Arc (`Δ⁻`, `↶`)**:
   - Emits in-place AArch64 `FSUB` / Knuth `TwoSum` error-free transformation (`0` bytes of undo snapshot, `0 ULP` round-trip drift).
3. **Guarded Scram Recovery (`⛨`)**:
   - Restores the pre-mutation `ReactorScramAnchor` frame in $O(1)$ if any non-finite or cone-bound invariant trips.

---

## 3. Quantum-Inspired Ballistic Ising/QUBO Hypergraph Scheduler

To assign `Ω-CIR` hyperedges across heterogeneous hardware tracks (`TRACK_0_SIAC_MAP_JIT`, `TRACK_1_AMX_TENSOR_MPS`, `TRACK_2_CYCLOTOMIC_ZETA16`), `Ω-CIR` minimizes the Ising Hamiltonian:
$$\mathcal{H}(\sigma) = -\sum_{i < j} J_{ij} \sigma_i \sigma_j - \sum_i h_i \sigma_i, \quad \sigma_i \in \{-1, +1\}$$
via 16 steps of **Ballistic Simulated Bifurcation (bSB)**:
- 6-spin manifold graph: Ising energy minimized from `+2.0500` $\to$ `-7.1500`.
- 10-spin multiphysics graph: Ising energy minimized from `+3.6500` $\to$ `-14.1500`.
