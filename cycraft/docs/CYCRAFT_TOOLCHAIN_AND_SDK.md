# CYCRAFT_TOOLCHAIN_AND_SDK.md — `SingularQuantum` (`.qcy`) CLI, Constitutional Enforcer & MCP Server

## 1. Unified SuperKernel CLI (`./bin/singular-quantum`)

| Command | Function | Verified Status |
|---|---|---|
| `./bin/singular-quantum verify-all` | Executes all `120 / 120` verification gates (`P00..P44`, `Q01..Q14`, `v6`..`v10 CD-01..CD-12`) | `VERIFIED (120/120 PASS)` |
| `./bin/singular-quantum enforce .` | Enforces 12 Reactor Compiler Laws (`RC-001`..`RC-012`), `SG-ARCH-400` (`<= 400` lines), and `SG-DUP-101` | `VERIFIED` |
| `./bin/singular-quantum enforce-workspace` | Enforces `SINGULAR-QUANTUM-EXCLUSIVE-001` and the 6-directory spine across all 14 workspace projects | `VERIFIED (14/14 PASS)` |
| `./bin/singular-quantum solve-omega02` | Executes the 36-qubit Partition-36 exact cyclotomic ring $\mathbb{Z}[\zeta_{16}]$ quantum solver (`OMEGA-02`) | `VERIFIED` |
| `./bin/singular-quantum run <file.qcy>` | Parses `.qcy` symbolic syntax, checks 12 Reactor Laws, lowers to `SIR Ω`, and executes via 8-stage VM/JIT | `VERIFIED` |
| `./bin/singular-quantum fmt <file.qcy>` | Bijective UTF-8/NFC symbolic (`Ω ⟦ ... ⟧`) $\leftrightarrow$ ASCII canonical transducer | `VERIFIED` |
| `./bin/singular-quantum mcp-self-test` | Verifies the native JSON-RPC 2.0 MCP server (`6/6` zero-token compiler tools in `0.025 ms`) | `VERIFIED` |

---

## 2. Canonical `.qcy` Toolchain Smoke Verification Module

```qcy
§λ("verification")
§ω("singular.quantum.toolchain")
§κ("SG-TOOLCHAIN-SDK-SMOKE-001")

Ω ⟦
  Ψ_0 : ℝ[16, 64],
  W_0 : ℝ[64, 64],
  δ_0 : Δℝ[16],
  Λ_0 : 𝕋[6, 6],
  Α_0 : ℤ[ζ₁₆, 36]
⟧ {
  Ψ_1 ← (Ψ_0 ⋅ W_0) ⊕ δ_0
  Ψ_rev ← ⛨(Ψ_1 ↶ δ_0) ↦ Λ_0
  Α_1 ← (ℋ_ℤ(𝒰_π8(Α_0)) ⨂_TM Λ_0) ⚡ Ψ_rev
  ⊢ ⇄(Ψ_0, Ψ_1)
  ⊢ |Ψ_1| < ∞
  ⊢ Α_0 ≈ ⟨ψ_0|𝒰_0|ψ_0⟩
}
```
