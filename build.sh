#!/usr/bin/env bash
set -euo pipefail

export COPYFILE_DISABLE=1
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

STAGE_DIR="${SINGULAR_STAGE_DIR:-/tmp/singular_nvme_build_cache}"
mkdir -p "$STAGE_DIR"

VCPKG_ROOT="${VCPKG_ROOT:-$HOME/.singularc-tools/vcpkg/installed/arm64-osx}"
SODIUM_INC="$VCPKG_ROOT/include"
SODIUM_LIB="$VCPKG_ROOT/lib/libsodium.a"

t0_ms=$(python3 -c 'import time; print(int(time.time()*1000))' 2>/dev/null || echo $(( $(date +%s) * 1000 )))

CXX_COMMON_FLAGS=(
    -std=c++23 -O3 -mcpu=native -DACCELERATE_NEW_LAPACK
    -I"$ROOT_DIR/cycraft/include" -I"$ROOT_DIR" -I"$SODIUM_INC"
    -DCYCRAFT_HAVE_LIBSODIUM=1 -DCYCRAFT_HAVE_MLX_CPP=1 -DCYCRAFT_HAVE_EIGEN3=1
    -DCYCRAFT_HAVE_HNSWLIB=1 -DCYCRAFT_HAVE_DLPACK=1 -DCYCRAFT_HAVE_SQLITE3=1
    -DCYCRAFT_HAVE_LZ4=1 -DCYCRAFT_HAVE_XXHASH=1 -DCYCRAFT_HAVE_SIMDJSON=1
)
if [ -n "${SINGULAR_EXTRA_CXXFLAGS:-}" ]; then
    CXX_COMMON_FLAGS+=("${SINGULAR_EXTRA_CXXFLAGS}")
fi

TOOL_VER="${SINGULAR_TOOL_VERSION_OVERRIDE:-$(clang++ --version 2>/dev/null | head -n 1)}"
SDK_VER="$(xcrun --show-sdk-version 2>/dev/null || echo unknown)"
DEP_LIBS=(
    "$ROOT_DIR/singular.lock"
    "$VCPKG_ROOT/lib/libmlx.dylib" "$VCPKG_ROOT/lib/libjaccl.dylib"
    "$VCPKG_ROOT/lib/liblz4.a" "$VCPKG_ROOT/lib/libxxhash.a"
    "$VCPKG_ROOT/lib/libsimdjson.a" "$VCPKG_ROOT/lib/libsqlite3.a" "$SODIUM_LIB"
)
DEP_META=$(stat -f "%N:%z:%m" "${DEP_LIBS[@]}" 2>/dev/null || true)
SCRIPT_SHA=$(shasum -a 256 "$ROOT_DIR/build.sh" 2>/dev/null | awk '{print $1}')
FLAGS_SHA=$(printf "%s" "${CXX_COMMON_FLAGS[*]}" | shasum -a 256 | awk '{print $1}')
DEP_SHA=$(printf "%s|%s|%s|%s" "$TOOL_VER" "$SDK_VER" "$DEP_META" "${SINGULAR_DEP_STAMP_OVERRIDE:-}" | shasum -a 256 | awk '{print $1}')
ENV_SHA=$(printf "%s|%s|%s" "$SCRIPT_SHA" "$FLAGS_SHA" "$DEP_SHA" | shasum -a 256 | awk '{print $1}')

# Compute hermetic SHA-256 stamp of all C++23 sources, headers, flags, build script, tool version, and transitive deps
SRC_SHA=$(find "$ROOT_DIR/cycraft" "$ROOT_DIR/singular/core/compiler/constitution" -type f \( -name "*.cpp" -o -name "*.hpp" \) ! -name "._*" -print0 2>/dev/null | sort -z | xargs -0 shasum -a 256 2>/dev/null | shasum -a 256 | awk '{print $1}')
TREE_SHA=$(printf "%s|%s" "$SRC_SHA" "$ENV_SHA" | shasum -a 256 | awk '{print $1}')
STAMP_FILE="$STAGE_DIR/last_tree.sha256"
FORCE_BUILD="${1:-}"

if [ "$FORCE_BUILD" = "--print-manifest-hash" ]; then
    echo "$TREE_SHA"
    exit 0
fi

valid_macho_file() {
    [ -f "$1" ] && [ ! -L "$1" ] && [ "$(stat -f "%z" "$1" 2>/dev/null || echo 0)" -gt 16384 ]
}
all_outputs_valid() {
    valid_macho_file "$ROOT_DIR/bin/singular-quantum" && [ -x "$ROOT_DIR/bin/singular-quantum" ] && \
    valid_macho_file "$ROOT_DIR/bin/cycraft" && [ -x "$ROOT_DIR/bin/cycraft" ] && \
    valid_macho_file "$ROOT_DIR/singular/core/compiler/bin/singular-quantum" && \
    valid_macho_file "$ROOT_DIR/stack/libsingular_math_stack.dylib" && \
    valid_macho_file "$ROOT_DIR/lib/libsingular_math_stack.dylib" && \
    { [ ! -d "$ROOT_DIR/../Supercomputing" ] || valid_macho_file "$ROOT_DIR/../Supercomputing/bin/singular-quantum"; }
}

if [ "$FORCE_BUILD" = "--print-build-stamp" ]; then
    LAST_SHA=$(cat "$STAMP_FILE" 2>/dev/null || echo "NONE")
    STATUS="MISS"
    if [ "$TREE_SHA" = "$LAST_SHA" ] && all_outputs_valid; then
        STATUS="HIT"
    fi
    printf 'TREE_SHA=%s\nSRC_SHA=%s\nFLAGS_SHA=%s\nDEP_SHA=%s\nLAST_SHA=%s\nCACHE_STATUS=%s\n' \
        "$TREE_SHA" "$SRC_SHA" "$FLAGS_SHA" "$DEP_SHA" "$LAST_SHA" "$STATUS"
    exit 0
fi

LOCK_DIR="$ROOT_DIR/.build.lock"
if ! mkdir "$LOCK_DIR" 2>/dev/null; then
    echo "Build unavailable: lock already exists at $LOCK_DIR; no lock was removed." >&2
    exit 75
fi
trap 'rm -f "$STAGE_DIR"/*.tmp.$$ "$ROOT_DIR"/bin/*.stage.$$ "$ROOT_DIR"/lib/*.stage.$$ "$ROOT_DIR"/stack/*.stage.$$ 2>/dev/null || true; rmdir "$LOCK_DIR" 2>/dev/null || true' EXIT INT TERM

mkdir -p bin lib stack singular/shared/stack singular/core/compiler/bin singular/verification/reports

atomic_install_signed() {
    local src="$1"
    local dst="$2"
    local stage="${dst}.stage.$$"
    mkdir -p "$(dirname "$dst")"
    rm -f "$dst" 2>/dev/null || true
    cp -f "$src" "$stage"
    codesign --force --identifier singular-quantum --sign - "$stage" 2>/dev/null || true
    mv -f "$stage" "$dst"
}

PCORE_RUN=(env)

if [ "$FORCE_BUILD" != "--force" ] && [ -s "$STAMP_FILE" ] && all_outputs_valid; then
    LAST_SHA=$(cat "$STAMP_FILE" 2>/dev/null || echo "")
    if [ "$TREE_SHA" = "$LAST_SHA" ]; then
        t1_ms=$(python3 -c 'import time; print(int(time.time()*1000))' 2>/dev/null || echo $(( $(date +%s) * 1000 )))
        elapsed=$(( t1_ms - t0_ms ))
        echo "[SingularQuantum | SINGULAR Ω v23.0.0-IMMORTAL-AGENT-HYPERVELOCITY] Content-Hash Cache HIT (${TREE_SHA:0:16}...) — Verified in ${elapsed} ms:"
        echo "  -> $ROOT_DIR/bin/singular-quantum"
        echo "  -> $ROOT_DIR/stack/libsingular_math_stack.dylib"
        exit 0
    fi
fi

echo "[SingularQuantum | SINGULAR Ω v23.0.0-IMMORTAL-AGENT-HYPERVELOCITY (.qcy)] Building on NVMe APFS Stage ($STAGE_DIR) with Apple M5 P-Core QoS..."

DYLIB_SHA=$(shasum -a 256 "$ROOT_DIR/cycraft/src/singular_math_stack_dylib.cpp" "$ROOT_DIR/cycraft/include/cycraft/quantum_syntax/omega_v14_elite_math_core.hpp" 2>/dev/null | shasum -a 256 | awk '{print $1}')
DYLIB_SHA=$(printf "%s|%s" "$DYLIB_SHA" "$ENV_SHA" | shasum -a 256 | awk '{print $1}')
DYLIB_STAMP="$STAGE_DIR/last_dylib.sha256"
if [ "$FORCE_BUILD" = "--force" ] || [ ! -s "$DYLIB_STAMP" ] || [ "$(cat "$DYLIB_STAMP" 2>/dev/null)" != "$DYLIB_SHA" ] || [ ! -s "$ROOT_DIR/stack/libsingular_math_stack.dylib" ] || [ ! -s "$ROOT_DIR/lib/libsingular_math_stack.dylib" ]; then
    TMP_DYLIB="$STAGE_DIR/libsingular_math_stack.dylib.tmp.$$"
    clang++ -std=c++23 -O3 -mcpu=native -dynamiclib -DACCELERATE_NEW_LAPACK \
        -I"$ROOT_DIR/cycraft/include" -I"$ROOT_DIR" \
        "$ROOT_DIR/cycraft/src/singular_math_stack_dylib.cpp" \
        -framework Accelerate \
        -install_name "@rpath/libsingular_math_stack.dylib" \
        -o "$TMP_DYLIB"
    strip -x "$TMP_DYLIB"
    codesign --force --sign - "$TMP_DYLIB" 2>/dev/null || true
    atomic_install_signed "$TMP_DYLIB" "$ROOT_DIR/singular/shared/stack/libsingular_math_stack.dylib"
    atomic_install_signed "$TMP_DYLIB" "$ROOT_DIR/lib/libsingular_math_stack.dylib"
    atomic_install_signed "$TMP_DYLIB" "$ROOT_DIR/stack/libsingular_math_stack.dylib"
    if [ -d "$ROOT_DIR/../Supercomputing" ]; then
        atomic_install_signed "$TMP_DYLIB" "$ROOT_DIR/../Supercomputing/singular/shared/stack/libsingular_math_stack.dylib"
        atomic_install_signed "$TMP_DYLIB" "$ROOT_DIR/../Supercomputing/stack/libsingular_math_stack.dylib"
    fi
    rm -f "$TMP_DYLIB"
    echo "$DYLIB_SHA" > "$DYLIB_STAMP"
fi

PCH_HDR="$STAGE_DIR/singular_foundation_pch.hpp"
PCH_OUT="$STAGE_DIR/singular_foundation_pch.hpp.pch"
PCH_STAMP="$STAGE_DIR/last_pch.sha256"
PCH_FILES_SHA=$(find "$ROOT_DIR/cycraft/include" "$ROOT_DIR/singular/core/compiler/constitution" -type f -name "*.hpp" ! -name "._*" ! -name "omega_v23_*.hpp" ! -name "omega_v24_*.hpp" ! -name "omega_v25_*.hpp" ! -name "omega_v26_*.hpp" ! -name "omega_v27_*.hpp" ! -name "omega_v30_*.hpp" ! -name "omega_v33_*.hpp" ! -name "omega_v34_*.hpp" ! -name "omega_v35_*.hpp" ! -name "omega_v36_*.hpp" ! -name "omega_v37_*.hpp" ! -name "omega_v38_*.hpp" ! -name "omega_v39_*.hpp" ! -name "omega_v40_*.hpp" ! -name "omega_v41_*.hpp" ! -name "omega_v42_*.hpp" ! -name "omega_v43_*.hpp" ! -name "omega_v44_*.hpp" ! -name "omega_v45_*.hpp" -print0 2>/dev/null | sort -z | xargs -0 shasum -a 256 2>/dev/null | shasum -a 256 | awk '{print $1}')
PCH_IN_SHA=$(printf "%s|%s|%s" "$PCH_FILES_SHA" "$FLAGS_SHA" "$DEP_SHA" | shasum -a 256 | awk '{print $1}')

if [ ! -f "$PCH_OUT" ] || [ ! -f "$PCH_HDR" ] || [ ! -f "$PCH_STAMP" ] || [ "$(cat "$PCH_STAMP" 2>/dev/null)" != "$PCH_IN_SHA" ]; then
    cat > "$PCH_HDR" << 'EOF'
#pragma once
#include "cycraft/compiler/cycraft_causal_ir.hpp"
#include "cycraft/fabric/cycraft_native_app_fabric.hpp"
#include "cycraft/fabric/cycraft_release_evidence_engine.hpp"
#include "cycraft/ir/cycraft_ssa_qbe_backend.hpp"
#include "cycraft/language/cycraft_lexer_parser.hpp"
#include "cycraft/language/cycraft_sema_and_laws.hpp"
#include "cycraft/quantum_syntax/omega_agent_native_superlanguage.hpp"
#include "cycraft/quantum_syntax/omega_cryptographic_superkernel.hpp"
#include "cycraft/quantum_syntax/omega_frustrated_lattice_cyclotomic_solver.hpp"
#include "cycraft/quantum_syntax/omega_governance_and_anomaly_intelligence.hpp"
#include "cycraft/quantum_syntax/omega_multitype_quantum_reactor_ir.hpp"
#include "cycraft/quantum_syntax/omega_nuclear_and_symbiotic_superkernel.hpp"
#include "cycraft/quantum_syntax/omega_q01_q06_benchmark_suite.hpp"
#include "cycraft/quantum_syntax/omega_realtime_diagnostics_and_chaos.hpp"
#include "cycraft/quantum_syntax/omega_stabilizer_entanglement_omega04_solver.hpp"
#include "cycraft/quantum_syntax/omega_v8_reactor_compiler_and_cython.hpp"
#include "cycraft/quantum_syntax/omega_v9_reactor_constitution_and_infinite_math.hpp"
#include "cycraft/quantum_syntax/omega_v10_cross_domain_discovery_plugins.hpp"
#include "cycraft/quantum_syntax/omega_v12_forensic_report_emitter.hpp"
#include "cycraft/quantum_syntax/omega_v12_quantum_model_synthesis.hpp"
#include "cycraft/quantum_syntax/omega_v13_deliverable_emitter.hpp"
#include "cycraft/quantum_syntax/omega_v14_unified_db_orchestrator_otzar_20domains.hpp"
#include "cycraft/quantum_syntax/omega_v14_forensic_deliverable_emitter.hpp"
#include "cycraft/quantum_syntax/omega_v15_ai_superlibrary_and_large_model_trainer.hpp"
#include "cycraft/quantum_syntax/omega_v15_sarc_swp_fusion_superkernel.hpp"
#include "cycraft/quantum_syntax/omega_v16_aaa_cinema_renderer_and_audio.hpp"
#include "cycraft/quantum_syntax/omega_self_hosting_bootstrap_engine.hpp"
#include "cycraft/quantum_syntax/omega_elite_siac_compiler_benchmark.hpp"
#include "cycraft/quantum_syntax/omega_builtin_math_stack_loader.hpp"
#include "cycraft/quantum_syntax/omega_v16_independent_truth_and_incremental_lab.hpp"
#include "cycraft/quantum_syntax/omega_v17_causal_breakthrough_and_accelerator_engine.hpp"
#include "cycraft/quantum_syntax/omega_v18_supercomputing_silicon_and_isa_engine.hpp"
#include "cycraft/quantum_syntax/omega_v19_all_20_domains_live_breakthrough_engine.hpp"
#include "cycraft/quantum_syntax/omega_v20_silicon_netlist_and_ip_fortress_engine.hpp"
#include "cycraft/quantum_syntax/omega_v21_native_asm_fusion_ai_and_antithesis_engine.hpp"
#include "cycraft/quantum_syntax/omega_v22_project_wide_asm_riemann_and_28q_engine.hpp"
#include "cycraft/runtime/cycraft_vm_and_jit_engine.hpp"
#include "singular/core/compiler/constitution/singular_constitutional_engine.hpp"
EOF
    clang++ "${CXX_COMMON_FLAGS[@]}" -x c++-header "$PCH_HDR" -o "$PCH_OUT"
    echo "$PCH_IN_SHA" > "$PCH_STAMP"
fi

TMP_BIN="$STAGE_DIR/cycraft.tmp.$$"
clang++ "${CXX_COMMON_FLAGS[@]}" -include-pch "$PCH_OUT" \
    "$ROOT_DIR/cycraft/src/cycraft_main.cpp" \
    -L"$VCPKG_ROOT/lib" -Wl,-rpath,"$VCPKG_ROOT/lib" \
    -lmlx -ljaccl \
    "$VCPKG_ROOT/lib/liblz4.a" \
    "$VCPKG_ROOT/lib/libxxhash.a" \
    "$VCPKG_ROOT/lib/libsimdjson.a" \
    "$VCPKG_ROOT/lib/libsqlite3.a" \
    "$SODIUM_LIB" \
    -lcompression \
    -framework Accelerate -framework Metal -framework Foundation \
    -o "$TMP_BIN"

strip -x "$TMP_BIN"
codesign --force --sign - "$TMP_BIN" 2>/dev/null || true
atomic_install_signed "$TMP_BIN" "$ROOT_DIR/bin/singular-quantum"
atomic_install_signed "$TMP_BIN" "$ROOT_DIR/singular/core/compiler/bin/singular-quantum"
atomic_install_signed "$TMP_BIN" "$ROOT_DIR/bin/cycraft"
if [ -d "$ROOT_DIR/../Supercomputing" ]; then
    atomic_install_signed "$TMP_BIN" "$ROOT_DIR/../Supercomputing/bin/singular-quantum"
    atomic_install_signed "$TMP_BIN" "$ROOT_DIR/../Supercomputing/singular/core/compiler/bin/singular-quantum"
fi
rm -f "$TMP_BIN"
echo "$TREE_SHA" > "$STAMP_FILE"

if [ ! -f "$ROOT_DIR/singular/core/compiler/bin/singular-self-hosted-stage2" ]; then
    "$ROOT_DIR/bin/singular-quantum" bootstrap >/dev/null
fi

t1_ms=$(python3 -c 'import time; print(int(time.time()*1000))' 2>/dev/null || echo $(( $(date +%s) * 1000 )))
elapsed=$(( t1_ms - t0_ms ))

echo "[SingularQuantum | SINGULAR Ω v23.0.0-IMMORTAL-AGENT-HYPERVELOCITY] Build succeeded in ${elapsed} ms:"
echo "  -> $ROOT_DIR/bin/singular-quantum"
echo "  -> $ROOT_DIR/bin/cycraft"
echo "  -> $ROOT_DIR/singular/core/compiler/bin/singular-quantum"
echo "  -> $ROOT_DIR/stack/libsingular_math_stack.dylib"
