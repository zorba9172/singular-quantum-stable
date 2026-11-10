# CYCRAFT_TYPE_AND_EFFECT_SYSTEM.md — Multi-Type Symbolic Quantum Computing (`MTSQC`), $\mathbb{Z}^7$ SI Units & Cyclotomic Rings

## 1. Compile-Time $\mathbb{Z}^7$ Abelian Dimensional Lattice & Shape Calculus (`RC-005`)

Every symbolic manifold in `SingularQuantum` (`.qcy`) carries a compile-time shape and dimensional signature:
$$D = (e_{\text{m}}, e_{\text{kg}}, e_{\text{s}}, e_{\text{A}}, e_{\text{K}}, e_{\text{mol}}, e_{\text{cd}}) \in \mathbb{Z}^7$$

1. **Additive & Incremental Homogeneity (`+`, `-`, `⊕`, `↶`)**:
   $$\frac{\Gamma \vdash \Psi_0 : \mathbb{R}[M, N] @ D_1 \quad \Gamma \vdash \delta_0 : \Delta\mathbb{R}[M] @ D_2 \quad D_1 = D_2}{\Gamma \vdash \Psi_0 \oplus \delta_0 : \mathbb{R}[M, N] @ D_1}$$
2. **Contraction & Tensor-Train Composition (`⋅`, `⨂_MPS`, `⨂_TM`)**:
   $$\frac{\Gamma \vdash \Psi_0 : \mathbb{R}[M, K] \quad \Gamma \vdash W_0 : \mathbb{R}[K, N]}{\Gamma \vdash \Psi_0 \cdot W_0 : \mathbb{R}[M, N]}$$
   Mismatched inner dimensions are rejected statically with `CY-RC-005-SHAPE-MISMATCH`.

---

## 2. Exact Cyclotomic Ring $\mathbb{Z}[\zeta_{16}]$ & Toric Partition Types (`OMEGA-02`)

To eliminate floating-point drift in 36-qubit Clifford+$T$ quantum circuits, `MTSQC` provides exact algebraic types:
- **`Λ_0 : 𝕋[6, 6]`**: 2D Toric Partition-36 lattice topology supporting Toom-Cook polynomial tensor contractions (`⨂_TM`).
- **`Α_0 : ℤ[ζ₁₆, 36]`**: Exact cyclotomic integer ring $\mathbb{Z}[\zeta_{16}] = \mathbb{Z}[x]/(x^8 + 1)$ where each amplitude is represented as an 8-tuple of exact integers $\sum_{k=0}^7 c_k \zeta_{16}^k$ transformed by exact Walsh-Hadamard (`ℋ_ℤ`) and $\pi/8$ $T$-gate phase shifts (`𝒰_π8`).

```qcy
§λ("core")
§ω("singular.quantum.types")
§κ("SG-MTSQC-TYPE-EFFECT-001")

Ω ⟦
  Ψ_0 : ℝ[16, 64],
  W_0 : ℝ[64, 64],
  δ_0 : Δℝ[16],
  Λ_0 : 𝕋[6, 6],
  Α_0 : ℤ[ζ₁₆, 36]
⟧ {
  Ψ_1 ← φ₊(Ψ_0 ⋅ W_0) ⊕ δ_0
  Ψ_rev ← ⛨(Ψ_1 ↶ δ_0) ↦ Λ_0
  Ψ_mps ← Ψ_rev ⋈ (Ψ_0 ⨂_MPS W_0)
  Α_1 ← (ℋ_ℤ(𝒰_π8(Α_0)) ⨂_TM Λ_0) ⚡ Ψ_mps
  ⊢ ⇄(Ψ_0, Ψ_1)
  ⊢ |Ψ_1| < ∞
  ⊢ Α_0 ≈ ⟨ψ_0|𝒰_0|ψ_0⟩
}
```

---

## 3. Effect & Security Lattice (`RC-007`, `RC-008`, `RC-009`, `RC-012`)

- **Reversibility Effect (`⇄(Ψ_0, Ψ_1)`)**: Enforces that `↶` retractions operate on bijective cones or `⛨`-anchored states (`RC-007`).
- **Information-Flow Secrecy (`RC-008`)**: Blocks unsealed `Secret[N]` manifolds from entering public Born measurements `ℳ`.
- **Exterior Nilpotency (`RC-012`)**: Symbolically and numerically enforces $d(d(\omega)) = 0$ on differential forms.
