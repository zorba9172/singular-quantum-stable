# CYCRAFT_STANDARD_LIBRARY.md — `SingularQuantum` (`.qcy`) Symbolic Standard Library (`singular::shared`)

## 1. Core Symbolic Primitives (`singular/shared/` & `singular/modules/`)

| Symbolic Namespace | Spine Directory | Exported Symbolic Types & `Ω∞` Operators |
|---|---|---|
| `singular.shared.units` | `singular/shared/` | $\mathbb{Z}^7$ SI dimensional algebra (`@m`, `@kg`, `@s`, `@A`, `@K`, `@mol`, `@cd`, `@Pa`, `@W`, `@J`) |
| `singular.shared.siac` | `singular/shared/` | `Δℝ[K]`, `⊕` (Incremental `Δ⁺`), `↶` (Decremental `Δ⁻`), `φ₊` (Harmonic `Δ_H`), `⛨` (`ReactorScramAnchor`) |
| `singular.shared.tensor` | `singular/shared/` | `ℝ[d₁, ..., dᵣ]`, `⋅` (`AUTO_AMX_PURE_DGEMM`), `⨂_MPS` (TT-SVD), `⨂_TM` (Toric Toom-Cook), `⊗`, `∘` |
| `singular.modules.quantum` | `singular/modules/` | `ℤ[ζ₁₆, 36]`, `𝕋[6, 6]`, `ℂ[2^n]`, `ℋ_ℤ`, `𝒰_π8`, `†`, `Tr(`, `ℳ`, `⟨ψ_0\|𝒰_0\|ψ_0⟩` |
| `singular.modules.topology` | `singular/modules/` | `d(`, `∧`, `⋆`, `∮`, `H¹_Sheaf` (Cellular Sheaf Cohomology), `∇_g` (Fisher-Rao), `ℋ_CD` |
| `singular.modules.plugins` | `singular/modules/` | 20 Cross-Domain Scientific Discovery Plugins (`⋈`, `↦`, `⚡`) across Layers I, II, and III |

---

## 2. Standard Symbolic Proof Obligations (`⊢`)

```qcy
§λ("shared")
§ω("singular.shared.stdlib")
§κ("SG-STDLIB-SYMBOLIC-INVARIANTS")

Ω ⟦
  Ψ_0 : ℝ[16, 64],
  W_0 : ℝ[64, 64],
  δ_0 : Δℝ[16],
  Λ_0 : 𝕋[6, 6],
  Α_0 : ℤ[ζ₁₆, 36]
⟧ {
  Ψ_1 ← φ₊(Ψ_0 ⋅ W_0) ⊕ δ_0
  Ψ_0_rev ← ⛨(Ψ_1 ↶ δ_0) ↦ Λ_0
  Α_1 ← ℋ_ℤ(𝒰_π8(Α_0)) ⨂_TM Λ_0
  ⊢ ⇄(Ψ_0, Ψ_1)
  ⊢ |Ψ_1| < ∞
  ⊢ Α_0 ≈ ⟨ψ_0|𝒰_0|ψ_0⟩
}
```
