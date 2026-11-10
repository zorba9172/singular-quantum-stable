#!/usr/bin/env bash
# ============================================================================
# SINGULAR Ω v25.2 / v32 — GATE-INDEPENDENT Clean-Room Reproducible Harness
# Compiles and executes `standalone_external_competitor_benchmark.cpp` from a
# clean directory outside the internal test runner, capturing compiler
# vectorization remarks, Mach-O disassembly, and raw CSV/JSON artifacts.
# ============================================================================
set -euo pipefail
export COPYFILE_DISABLE=1

SC_ROOT="/Volumes/Lenovo PS8S/workspace/Supercomputing"
SQ_ROOT="/Volumes/Lenovo PS8S/workspace/SingularQuantum"
BENCH_SRC="$SC_ROOT/singular/core/v24/causal_machine/bench/standalone_external_competitor_benchmark.cpp"
ASM_SRC="$SC_ROOT/singular/core/compiler/asm/omega_v25_total_singular_arc_siac_arm64.s"
CLEAN_DIR="/tmp/singular_cleanroom_independent_verify"
OUT_SC="$SC_ROOT/singular/verification/reports/independent_external_proof"
OUT_SQ="$SQ_ROOT/singular/verification/reports/independent_external_proof"

mkdir -p "$CLEAN_DIR" "$OUT_SC" "$OUT_SQ"

# 1. Clean-room compile of the ARM64 NEON assembly dylib
clang -O3 -dynamiclib -arch arm64 "$ASM_SRC" -o "$CLEAN_DIR/libsingular_arc_siac_asm.dylib"
codesign --force --identifier singular-arc-siac-asm --sign - "$CLEAN_DIR/libsingular_arc_siac_asm.dylib" >/dev/null 2>&1 || true

# 2. Clean-room compile of the standalone C++23 + Eigen3 + Accelerate + NEON benchmark
#    Capture Clang loop-vectorization remarks (`-Rpass=loop-vectorize`)
clang++ -std=c++23 -O3 -mcpu=native -DACCELERATE_NEW_LAPACK \
  -Rpass=loop-vectorize \
  -I/Users/moshemoran/.singularc-tools/vcpkg/installed/arm64-osx/include \
  "$BENCH_SRC" \
  "$ASM_SRC" \
  -framework Accelerate \
  -o "$CLEAN_DIR/standalone_external_competitor_benchmark" \
  2> "$OUT_SC/clang_vectorization_remarks.txt"
codesign --force --identifier standalone-external-benchmark --sign - "$CLEAN_DIR/standalone_external_competitor_benchmark" >/dev/null 2>&1 || true

# 3. Capture ARM64 disassembly of the fused assembly kernel vs C++23 kernels
otool -tvV "$CLEAN_DIR/libsingular_arc_siac_asm.dylib" > "$OUT_SC/arm64_fused_kernel_disassembly.s"

# 4. Execute clean-room benchmark and emit raw CSV/JSON artifacts
"$CLEAN_DIR/standalone_external_competitor_benchmark" "$OUT_SC" | tee "$OUT_SC/standalone_stdout.log"

# 5. Sync artifacts to SingularQuantum
cp -f "$OUT_SC/"* "$OUT_SQ/"
cp -f "$CLEAN_DIR/standalone_external_competitor_benchmark" "$SC_ROOT/singular/core/v24/causal_machine/bin/standalone_external_competitor_benchmark"
