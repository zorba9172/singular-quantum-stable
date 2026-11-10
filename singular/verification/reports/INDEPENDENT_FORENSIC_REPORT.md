# SINGULAR Ω++ × CYCRAFT Ω+ — Independent Forensic Certification Report (`P00`..`P44`)

- **Release Version**: `3.0.0-STABLE` (`PASS_STABLE_RELEASE_CERTIFIED`)
- **Workspace Enforcement**: `13/13` projects enforced (`92` canonical `L0..L6` `.cy` modules compiled, max file lines = `37 <= 400`).
- **`F01` / `P40` Cryptographic Ed25519**: Full 64-byte (`128` hex chars) RFC 8032 Ed25519 signature (`dd3d805e71270756fcb5a92ff80ca47f6f61cb93133dc6df9139a64a7168d709052f8d5e0892c9e2cfbb9b256d334db4147f86a9ca6844d9efa595ba5ba47c00`) + 32-byte (`64` hex chars) public key (`2f31763ae369353fba49a24284a3a4c8d64c47a183727e76493d1d91914bc616`) over untruncated 64-hex SHA-256 Merkle root (`5b9ed2b765cab4bb5115862f19bcb8b862f3e2de75e53a75e9a2abdaf6282ddb`).
- **`F02` / `P37`–`P38` Apples-to-Apples Incremental Parity (`K=32`)**: Separates `INCREMENTAL_K32` from `FULL_RECOMPUTE_N4096` with 2,000 warmup + 10,000 timed iterations, `volatile` anti-DCE sink, and 5-stage latency split.
- **`F03` / `P39` Reversibility Proof**: Bijective ops require `256 B` retained input delta (`0 B` state snapshot undo log); lossy ops (`clamp`/`relu`) without boundary log are rejected at compile time with `CY-REV-001`.
- **`F04` / `P41` 240-Pair Labeled Semantic Reuse Corpus**: `TP=120, FP=0, TN=120, FN=0` (`Precision=1.000, Recall=1.000, F1=1.000`).
- **`P42` Multi-Stage Self-Hosting**: `Stage-1 == Stage-2 == Stage-3` (`SHA-256 = cfc2cf1d9d1df13221e8dbf8706959edc577988a8340e212d38f235ee904a7b7`).
- **`P43` Production Safetensors Workload**: `mmap` `3325995712` B, loss `3.90756 -> 0.294336`.
- **`P44` Multi-Process OS Fault Injection**: 5 `fork()` child processes, minority `2/5` write rejected, majority `3/5` committed at `term=2`, partition healed across all 5 processes.
