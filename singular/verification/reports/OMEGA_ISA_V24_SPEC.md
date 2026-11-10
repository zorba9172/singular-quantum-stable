# OMEGA_ISA_V24_SPEC — Canonical Singular Arc Machine ISA Specification

- **Single Source of Truth**: `singular/core/v24/machine/isa/omega_isa.yaml`
- **Opcode Space**: RISC-V `custom-3` (`0x7B = 7'b1111011`), R-Type 32-bit, `funct7 = 0x01`.
- **Core Instructions**:
  1. `omega.track rd, rs1, rs2` (`funct3=0`, sample `0x02c5857b`): Register dependency bit `rs2` on state `rs1`.
  2. `omega.dirty rd, rs1, rs2` (`funct3=1`, sample `0x02c5957b`): Verify `rs1 < 1024` and `version[rs1] == rs2`, increment version and set `dirty=1`.
  3. `omega.prop  rd, rs1, rs2` (`funct3=2`): Propagate dirty bit across `deps_mask[rs1]` and return cone mask.
  4. `omega.valid rd, rs1, rs2` (`funct3=3`): Check/issue SARC reuse validity certificate.
- **Exhaustive Codec Verification**: `131,072 / 131,072` valid R-type encodings bijective; `133 / 133` illegal encodings rejected.
