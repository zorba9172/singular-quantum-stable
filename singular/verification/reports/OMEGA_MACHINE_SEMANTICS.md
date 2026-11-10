# OMEGA_MACHINE_SEMANTICS — 1024-Entry Architectural State Table & Error Semantics

- **State Table Capacity**: `1,024` entries (`version: u32`, `dirty: u1`, `deps_mask: u64`, `valid_cert: u32`, `owner_cap: u16`).
- **Error Codes (Architectural State Unchanged on Error)**:
  - `0 = OMEGA_OK`
  - `1 = ERR_INVALID_STATE_ID` (`rs1 >= 1024` or `dep_bit >= 64`)
  - `2 = ERR_VERSION_MISMATCH` (`version[rs1] != rs2`)
  - `3 = ERR_CAPABILITY_DENIED` (`caller_cap != owner_cap`)
  - `4 = ERR_VERSION_OVERFLOW` (`version[rs1] == 0xFFFFFFFF`)
