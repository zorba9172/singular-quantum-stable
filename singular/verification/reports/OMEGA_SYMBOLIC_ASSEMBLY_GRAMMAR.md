# OMEGA_SYMBOLIC_ASSEMBLY_GRAMMAR — `.sga` Mother-Tongue Specification

- **Canonical File**: `singular/core/v24/language/symbolic_assembly/causal_update.sga`
- **Strict Operator Separation**: `∂` is reserved exclusively for mathematical differentiation (`∂(f)/∂(x)`), while `Ω.PROP` denotes causal dependency cone propagation.
- **Lowering Distinction**: ARM64 lowers `omtwosum..omloop` to multi-instruction native sequences; RISC-V lowers `omega.track..omega.valid` to physical `0x7B` instructions.
