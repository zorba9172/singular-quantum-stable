#!/usr/bin/env bash
# SINGULAR Ω v24 causal_machine — build + verification pipeline (Xomegacausal).
#   ./build_v24.sh [all|quick]     quick skips benchmarks and FPGA place-and-route
# Every gate's full output goes to release/reports/v24_causal_machine/evidence/<gate>.log and
# its exit code to gates.tsv. A gate is PASS only if its command exited 0.
# External tools (not vendored): $SINGULAR_TOOLS (default ~/.singularc-tools) must contain
#   oss-cad-suite/bin (yosys, sby, verilator, nextpnr-ecp5, bitwuzla), bin/dtc, spike-build/.
set -uo pipefail
export COPYFILE_DISABLE=1
MODE="${1:-all}"
TOOLS="${SINGULAR_TOOLS:-$HOME/.singularc-tools}"
REAL_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd -P)"
# make, Verilator and yosys scripts cannot handle the space in "Lenovo PS8S": work through a symlink.
mkdir -p "$TOOLS" && ln -sfn "$REAL_ROOT" "$TOOLS/sc"
ROOT="$TOOLS/sc"
HERE="$ROOT/singular/core/v24/causal_machine"
OSS="$TOOLS/oss-cad-suite/bin"
SPIKE_SRC="$ROOT/sources/spike"
SPIKE_BUILD="$TOOLS/spike-build"
SPIKE="$SPIKE_BUILD/spike-omega"
WORK="$TOOLS/omega-work"
REPORT="$ROOT/release/reports/v24_causal_machine"
EVID="$REPORT/evidence"
BIN="$HERE/bin"
TCC="$ROOT/sources/bin/singular-riscv64-tcc"
QBE="$ROOT/sources/bin/singular-qbe"
CXX=(clang++ -std=c++20 -Wall -Wextra -Wno-missing-field-initializers)
if [ -n "${SINGULAR_EXTRA_CXXFLAGS:-}" ]; then CXX+=("${SINGULAR_EXTRA_CXXFLAGS}"); fi
export PATH="$TOOLS/bin:$PATH"

mkdir -p "$EVID" "$BIN" "$WORK"
V24_SHA=$({
    echo "mode:${MODE}"
    echo "cxx_flags:${CXX[*]}"
    echo "tool_override:${SINGULAR_TOOL_VERSION_OVERRIDE:-}"
    echo "dep_override:${SINGULAR_DEP_STAMP_OVERRIDE:-}"
    echo "clang:$(clang++ --version 2>/dev/null | head -1 || echo UNAVAILABLE)"
    echo "gxx:$(g++ --version 2>/dev/null | head -1 || echo UNAVAILABLE)"
    echo "dtc:$(dtc --version 2>/dev/null | head -1 || echo UNAVAILABLE)"
    echo "verilator:$([ -x "$OSS/verilator" ] && "$OSS/verilator" --version 2>/dev/null | head -1 || echo UNAVAILABLE)"
    echo "yosys:$([ -x "$OSS/yosys" ] && "$OSS/yosys" -V 2>/dev/null | head -1 || echo UNAVAILABLE)"
    echo "nextpnr-ecp5:$([ -x "$OSS/nextpnr-ecp5" ] && "$OSS/nextpnr-ecp5" --version 2>&1 | head -1 || echo UNAVAILABLE)"
    echo "sby:$([ -x "$OSS/sby" ] && shasum -a 256 "$OSS/sby" 2>/dev/null | awk '{print $1}' || echo UNAVAILABLE)"
    echo "bitwuzla:$([ -x "$OSS/bitwuzla" ] && "$OSS/bitwuzla" --version 2>&1 | head -1 || echo UNAVAILABLE)"
    echo "spike_commit:$(git -C "$SPIKE_SRC" rev-parse HEAD 2>/dev/null || echo UNAVAILABLE)"
    for f in "$SPIKE_SRC/customext/omega_causal.cc" "$SPIKE_SRC/customext/omega_isa_gen.h" "$SPIKE_SRC/customext/customext.mk.in" "$TCC" "$QBE"; do
        echo "ext:$f:$([ -f "$f" ] && shasum -a 256 "$f" 2>/dev/null | awk '{print $1}' || echo UNAVAILABLE)" | sed "s#$ROOT/##"
    done
    find "$HERE" -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.sv' -o -name '*.sga' -o -name '*.yaml' -o -name '*.sby' -o -name '*.h' -o -name '*.sh' \) ! -name '._*' -print0 2>/dev/null | sort -z | xargs -0 shasum -a 256 2>/dev/null | sed "s#$ROOT/##"
} | shasum -a 256 | awk '{print $1}')
V24_STAMP="$WORK/last_v24_causal.sha256"

if [ "$MODE" = "--print-manifest-hash" ]; then
    echo "$V24_SHA"
    exit 0
fi

LOCK="$HERE/.build.lock"
if ! mkdir "$LOCK" 2>/dev/null; then echo "another build_v24.sh holds $LOCK" >&2; exit 3; fi
trap 'rm -rf "$LOCK"' EXIT INT TERM

if [ "$MODE" != "--force" ] && [ -s "$V24_STAMP" ] && [ "$(cat "$V24_STAMP" 2>/dev/null)" = "$V24_SHA" ] && \
   [ -s "$EVID/summary.txt" ] && grep -q "fail=0" "$EVID/summary.txt" 2>/dev/null && [ -s "$EVID/gates.tsv" ] && \
   [ -s "$EVID/environment.txt" ] && [ -s "$REPORT/OMEGA_SPIKE_EXECUTION_EVIDENCE.json" ] && \
   { [ "$MODE" = "quick" ] || [ -s "$REPORT/OMEGA_BENCHMARK_RESULTS.csv" ]; }; then
    awk -F'\t' 'NR>1 {printf "[%s] %-28s %s\n", $2, $1, $4}' "$EVID/gates.tsv"
    cat "$EVID/summary.txt"
    exit 0
fi
rm -f "$EVID"/*.log "$EVID/gates.tsv" "$EVID/summary.txt" "$V24_STAMP"
printf "gate\tstatus\texit\tsummary\n" > "$EVID/gates.tsv"
PASS=0; FAIL=0; SKIP=0

gate() {  # gate <name> <command...>
    local name="$1"; shift
    local log="$EVID/$name.log" t0 t1 rc summary
    t0=$(date +%s)
    { echo "# gate: $name"; echo "# cwd: $PWD"; echo "# command: $*"; echo "# started: $(date -u +%FT%TZ)"; } > "$log"
    "$@" >> "$log" 2>&1; rc=$?
    t1=$(date +%s)
    echo "# exit: $rc  elapsed_s: $((t1 - t0))" >> "$log"
    summary=$(grep -E "RESULT=|CHECK=|MATCH|PASS|FAIL|Max frequency|DONE|REGION|Walltime" "$log" | grep -v "^#" | tail -1 | tr '\t' ' ' | cut -c1-200)
    if [ $rc -eq 0 ]; then PASS=$((PASS + 1)); st=PASS; else FAIL=$((FAIL + 1)); st=FAIL; fi
    printf "%s\t%s\t%s\t%s\n" "$name" "$st" "$rc" "$summary" >> "$EVID/gates.tsv"
    printf "[%s] %-28s %s\n" "$st" "$name" "$summary"
}
skip() { SKIP=$((SKIP + 1)); printf "%s\tSKIP\t-\t%s\n" "$1" "$2" >> "$EVID/gates.tsv"; printf "[SKIP] %-28s %s\n" "$1" "$2"; }
need() { command -v "$1" >/dev/null 2>&1 || [ -x "$1" ]; }

cd "$HERE"
# ---------------------------------------------------------------- environment record
{
    echo "date_utc: $(date -u +%FT%TZ)"; echo "host: $(uname -a)"; echo "clang: $(clang++ --version | head -1)"
    echo "spike_commit: $(git -C "$SPIKE_SRC" rev-parse HEAD 2>/dev/null || echo UNKNOWN)"
    [ -x "$OSS/verilator" ] && echo "verilator: $("$OSS/verilator" --version)"
    [ -x "$OSS/yosys" ] && echo "yosys: $("$OSS/yosys" -V)"
    [ -x "$OSS/nextpnr-ecp5" ] && echo "nextpnr-ecp5: $("$OSS/nextpnr-ecp5" --version 2>&1 | head -1)"
    echo "dtc: $(dtc --version 2>/dev/null || echo UNAVAILABLE)"
    echo "source_sha256:"; find "$HERE" -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.sv' -o -name '*.sga' -o -name '*.yaml' -o -name '*.sby' -o -name '*.h' -o -name '*.sh' \) ! -name '._*' -print0 | sort -z | xargs -0 shasum -a 256 | sed "s#$ROOT/##"
    echo "spike_extension_sha256: $(shasum -a 256 "$SPIKE_SRC/customext/omega_causal.cc" | cut -d' ' -f1)"
} > "$EVID/environment.txt"

# ---------------------------------------------------------------- G1 ISA single source of truth
"${CXX[@]}" -O2 -o "$BIN/omega_isagen" tools/omega_isagen.cpp || exit 1
gate G01_isa_ssot_in_sync "$BIN/omega_isagen" isa/omega_isa.yaml --check isa/gen "$SPIKE_SRC/customext/omega_isa_gen.h"

# ---------------------------------------------------------------- tools
"${CXX[@]}" -O2 -o "$BIN/singular-as" tools/singular_as.cpp && cp -f "$BIN/singular-as" "$BIN/singular-objdump" &&
"${CXX[@]}" -O2 -o "$BIN/omega-cosim" tools/omega_cosim.cpp &&
"${CXX[@]}" -O2 -o "$BIN/omega-fuzz" tools/omega_fuzz.cpp || { echo "tool build failed"; exit 1; }
gate G02_encoding_exhaustive "$BIN/singular-as" --selftest-encoding

# ---------------------------------------------------------------- G3 TinyCC cross-assembly of base instructions
TCC="$ROOT/sources/bin/singular-riscv64-tcc"
xasm() {
    local ok=0 f
    for f in sga/*.sga; do
        local b; b=$(basename "$f" .sga)
        "$BIN/singular-as" --emit-gnu "$f" -o "$WORK/$b.s" >/dev/null || return 1
        "$TCC" -c "$WORK/$b.s" -o "$WORK/$b.tcc.o" || return 1
        "$BIN/singular-as" --text-words "$WORK/$b.tcc.o" > "$WORK/$b.tcc.words"
        "$BIN/singular-as" "$f" -o "$WORK/$b.elf" --listing | grep -E '^[0-9a-f]{8}:' | awk '{print $2}' > "$WORK/$b.as.words"
        local mnem; mnem=$(grep -cvE '^\s*(\.int|#|\.text)' "$WORK/$b.s")
        if diff -q "$WORK/$b.tcc.words" "$WORK/$b.as.words" >/dev/null; then echo "$b: words identical ($mnem base-instruction mnemonics assembled by TinyCC)"; else echo "$b: MISMATCH"; ok=1; fi
    done
    [ $ok -eq 0 ] && echo "RESULT=PASS" || echo "RESULT=FAIL"
    return $ok
}
if [ -x "$TCC" ]; then gate G03_tinycc_cross_assembly xasm; else skip G03_tinycc_cross_assembly "TinyCC RV64 binary UNAVAILABLE"; fi

# ---------------------------------------------------------------- Spike (build extension, run programs)
build_spike() {
    if [ ! -f "$SPIKE_BUILD/Makefile" ]; then
        mkdir -p "$SPIKE_BUILD" && (cd "$SPIKE_BUILD" && "$ROOT/sources/spike/configure" --prefix="$TOOLS/spike-install") || return 1
    fi
    (cd "$SPIKE_BUILD" && make -j8 && g++ -L. -o spike-omega spike.o omega_causal.o libspike_main.a libriscv.a libdisasm.a \
        libsoftfloat.a libfesvr.a libfdt.a -lpthread) || return 1
    echo "RESULT=PASS spike-omega linked with the omega_causal extension"
}
run_spike() { perl -e 'alarm 120; exec @ARGV' "$SPIKE" --isa=rv64imac --extension=omega_causal --log-commits "$1" 2> "$2"; }
spike_prog() {  # spike_prog <name>: assemble, execute on Spike, compare with reference, keep Ω trace for RTL
    local b="$1"
    "$BIN/singular-as" "sga/$b.sga" -o "$WORK/$b.elf" || return 1
    run_spike "$WORK/$b.elf" "$WORK/$b.spike.log"; local rc=$?
    echo "spike_exit_code=$rc"
    "$BIN/omega-cosim" diff "$WORK/$b.elf" "$WORK/$b.spike.log" "$WORK/$b.trace" || return 1
    grep -E "^core +0: 3 0x00000000800" "$WORK/$b.spike.log" | grep -m 12 "7b) " || true
    return $rc
}
if need dtc && [ -d "$SPIKE_SRC" ]; then
    gate G04_spike_build build_spike
    for p in causal_update priv_trap prop_sw_vs_hw; do gate "G05_spike_$p" spike_prog "$p"; done
    gate G06_isa_dynamic_instructions "$BIN/omega-cosim" count "$WORK/prop_sw_vs_hw.elf" "$WORK/prop_sw_vs_hw.spike.log" sw_begin sw_end hw_setup_begin hw_setup_end hw_begin hw_end
    fuzz() {
        local s fails=0 ops=0
        for s in $(seq 1 40); do
            "$BIN/omega-fuzz" "$s" 500 > "$WORK/fz$s.sga" && "$BIN/singular-as" "$WORK/fz$s.sga" -o "$WORK/fz$s.elf" >/dev/null || return 1
            run_spike "$WORK/fz$s.elf" "$WORK/fz$s.log"
            local r; r=$("$BIN/omega-cosim" diff "$WORK/fz$s.elf" "$WORK/fz$s.log" "$WORK/fz$s.trace"); echo "seed $s: $r"
            echo "$r" | grep -q "RESULT=MATCH" || fails=$((fails + 1))
            ops=$((ops + $(wc -l < "$WORK/fz$s.trace")))
        done
        echo "FUZZ seeds=40 omega_ops=$ops mismatching_seeds=$fails RESULT=$([ $fails -eq 0 ] && echo PASS || echo FAIL)"
        [ $fails -eq 0 ]
    }
    gate G07_spike_vs_ref_fuzz fuzz
else
    skip G04_spike_build "dtc or Spike sources UNAVAILABLE"
fi

# ---------------------------------------------------------------- RTL: lint, simulation, mutation, formal, synthesis
RTL=(isa/gen/omega_isa_pkg.sv rtl/omega_causal_decoder.sv rtl/omega_causal_unit.sv)
if [ -x "$OSS/verilator" ]; then
    gate G08_rtl_lint "$OSS/verilator" --lint-only -Wall --top-module omega_causal_unit "${RTL[@]}"
    vbuild() {  # vbuild <unit.sv> <objdir> <exe>
        rm -rf "$2"; "$OSS/verilator" --cc --exe --build -j 8 -O3 -Wno-fatal --top-module omega_causal_unit --Mdir "$2" \
            -CFLAGS "-std=c++20 -O2" isa/gen/omega_isa_pkg.sv rtl/omega_causal_decoder.sv "$1" "$HERE/tb/tb_omega_causal.cpp" -o "$3" >/dev/null
    }
    rtl_sim() {
        vbuild rtl/omega_causal_unit.sv "$WORK/vobj" tb_omega || return 1
        cp -f "$WORK/vobj/tb_omega" "$BIN/tb_omega"
        local rc=0 t
        "$BIN/tb_omega" random 1 200000 || rc=1
        "$BIN/tb_omega" random 2 200000 || rc=1
        for t in "$WORK"/*.trace; do [ -s "$t" ] && { "$BIN/tb_omega" trace "$t" || rc=1; }; done
        echo "RESULT=$([ $rc -eq 0 ] && echo PASS || echo FAIL) (random vs C++ reference + Spike-checked traces)"
        return $rc
    }
    gate G09_rtl_sim_3way rtl_sim
    mutants() {
        local m e killed=0
        for m in 1 2 3; do
            case $m in
                1) e='s/version_q\[j\*32 \+: 32\] <= version_q\[j\*32 \+: 32\] \+ 32.d1;/version_q[j*32 +: 32] <= version_q[j*32 +: 32] + 32'"'"'d2;/';;
                2) e='s/else if \(ai == bi\)  res_q/else if (1'"'"'b0)  res_q/';;
                3) e='s/if \(reach_q\[j\] && !dirty_q\[j\]\) begin/if (reach_q[j]) begin/';;
            esac
            perl -pe "$e" rtl/omega_causal_unit.sv > "$WORK/mutant$m.sv"
            if cmp -s rtl/omega_causal_unit.sv "$WORK/mutant$m.sv"; then echo "mutant $m: NOT APPLIED"; continue; fi
            vbuild "$WORK/mutant$m.sv" "$WORK/mobj$m" tbm || return 1
            if "$WORK/mobj$m/tbm" random 1 20000 >/dev/null 2>&1; then echo "mutant $m: SURVIVED"; else echo "mutant $m: killed"; killed=$((killed + 1)); fi
        done
        echo "MUTANTS killed=$killed/3 RESULT=$([ $killed -eq 3 ] && echo PASS || echo FAIL)"; [ $killed -eq 3 ]
    }
    gate G10_rtl_mutation_sensitivity mutants
else
    skip G08_rtl_lint "verilator UNAVAILABLE"
fi
if [ -x "$OSS/sby" ]; then
    formal() {
        local d="$WORK/formal" t rc=0
        rm -rf "$d"; mkdir -p "$d"
        cp -f "${RTL[@]}" formal/omega_causal_formal.sv "$d/"
        sed -e 's#^\.\./isa/gen/##' -e 's#^\.\./rtl/##' formal/omega_causal.sby > "$d/omega_causal.sby"
        (cd "$d" && "$OSS/sby" -f omega_causal.sby func_bmc func_prove reset_bmc func_cover) || rc=1
        for t in func_bmc func_prove reset_bmc func_cover; do echo "task $t: $(cat "$d/omega_causal_$t/status" 2>/dev/null || echo MISSING)"; done
        perl -pe 's/if \(reach_q\[j\] && !dirty_q\[j\]\) begin/if (reach_q[j]) begin/' rtl/omega_causal_unit.sv > "$d/omega_causal_unit.sv"
        if (cd "$d" && "$OSS/sby" -f omega_causal.sby func_bmc >/dev/null 2>&1); then echo "formal mutant: SURVIVED"; rc=1; else echo "formal mutant: detected (assertion failure)"; fi
        echo "FORMAL RESULT=$([ $rc -eq 0 ] && echo PASS || echo FAIL)"; return $rc
    }
    gate G11_formal_sby formal
    synth() {
        local d="$WORK/synth"; mkdir -p "$d"
        "$OSS/yosys" -q -l "$d/yosys.log" -p "read_verilog -sv ${RTL[*]} synth/omega_causal_synth_top.sv; synth_ecp5 -top omega_causal_synth_top -json $d/omega.json; tee -o $d/stat.txt stat" || return 1
        grep -E "LUT4|TRELLIS_FF|CCU2C|PFUMX|L6MUX21" "$d/stat.txt"
        [ "$MODE" = quick ] && { echo "RESULT=PASS (P&R skipped in quick mode)"; return 0; }
        "$OSS/nextpnr-ecp5" --85k --package CABGA756 --json "$d/omega.json" --lpf-allow-unconstrained --freq 25 --seed 1 -l "$d/nextpnr.log" >/dev/null 2>&1; local rc=$?
        grep -E "Max frequency|Device utilisation|TRELLIS_COMB|TRELLIS_FF" "$d/nextpnr.log" | tail -8
        echo "RESULT=$([ $rc -eq 0 ] && echo PASS || echo FAIL) (ECP5-85k place-and-route timing estimate, no bitstream/board)"; return $rc
    }
    gate G12_synthesis_ecp5_timing synth
else
    skip G11_formal_sby "SymbiYosys UNAVAILABLE"
fi

# ---------------------------------------------------------------- QBE emulated ARM64 path
QBE="$ROOT/sources/bin/singular-qbe"
emu() {
    local b fails=0
    "${CXX[@]}" -O2 -c tools/omega_emu_rt.cpp -o "$WORK/omega_emu_rt.o" || return 1
    for b in causal_update fz1 fz2 fz3 fz4 fz5; do
        local src="sga/$b.sga"; [ -f "$src" ] || src="$WORK/$b.sga"
        "$BIN/singular-as" --emit-qbe "$src" -o "$WORK/$b.ssa" >/dev/null && "$QBE" -t arm64_apple -o "$WORK/$b.arm64.s" "$WORK/$b.ssa" &&
            clang++ -o "$WORK/$b.emu" "$WORK/omega_emu_rt.o" "$WORK/$b.arm64.s" || return 1
        "$WORK/$b.emu" > "$WORK/$b.emu.regs" 2>/dev/null
        "$BIN/singular-as" "$src" -o "$WORK/$b.elf" >/dev/null
        "$BIN/omega-cosim" ref "$WORK/$b.elf" | awk '$4 ~ /^x/ && $4 != "x30" && $4 != "x31" {v[$4]=$5} END {for (r in v) print r, v[r]}' | sort > "$WORK/$b.ref.regs"
        local miss; miss=$(sort "$WORK/$b.emu.regs" | comm -13 - "$WORK/$b.ref.regs" | wc -l | tr -d ' ')
        echo "$b: registers written by the program=$(wc -l < "$WORK/$b.ref.regs" | tr -d ' ') differing=$miss"
        [ "$miss" = 0 ] || fails=$((fails + 1))
    done
    echo "EMULATED_ARM64 RESULT=$([ $fails -eq 0 ] && echo PASS || echo FAIL)"; [ $fails -eq 0 ]
}
if [ -x "$QBE" ]; then gate G13_qbe_arm64_emulated_path emu; else skip G13_qbe_arm64_emulated_path "singular-qbe UNAVAILABLE"; fi

# ---------------------------------------------------------------- Ω-IR / cognitive / benchmarks
CSV="$REPORT/OMEGA_BENCHMARK_RESULTS.csv"
"${CXX[@]}" -O3 -ffp-contract=off -o "$BIN/omega-cognitive" cognitive/omega_cognitive.cpp &&
"${CXX[@]}" -O3 -ffp-contract=off -o "$BIN/omega-bench" bench/omega_bench.cpp || exit 1
if [ "$MODE" = quick ]; then
    gate G14_cognitive_engine "$BIN/omega-cognitive" "$WORK/cognitive_quick.csv" 18
    skip G15_benchmarks "quick mode"
else
    echo "family,workload,param,baseline_ns,candidate_ns,affected_fraction,speedup,correctness,note" > "$CSV"
    gate G14_cognitive_engine "$BIN/omega-cognitive" "$CSV" 22
    gate G15_benchmarks "$BIN/omega-bench" "$CSV"
    isa_row() {  # dynamic instruction counts from the Spike commit log (G06), appended to the CSV
        local sw hw setup
        sw=$(grep "sw_begin" "$EVID/G06_isa_dynamic_instructions.log" | sed "s/.*=//")
        setup=$(grep "hw_setup_begin" "$EVID/G06_isa_dynamic_instructions.log" | sed "s/.*=//")
        hw=$(grep "hw_begin\.\." "$EVID/G06_isa_dynamic_instructions.log" | sed "s/.*=//")
        [ -n "$sw" ] && [ -n "$hw" ] && echo "ISA_dynamic_instructions,prop_chain64_spike,instructions,$sw,$hw,,$(awk -v a="$sw" -v b="$hw" "BEGIN{printf \"%.1f\", a/b}"),spike_cosim_match,baseline=RV64I worklist software candidate=omega.dirty+omega.prop (one-time track setup=$setup instructions not included)" >> "$CSV"
    }
    isa_row
fi

# ---------------------------------------------------------------- machine-readable Spike evidence (derived from the logs above)
spike_json() {
    local out="$REPORT/OMEGA_SPIKE_EXECUTION_EVIDENCE.json" first=1 p line
    {
        echo "{"
        echo "  \"generated_utc\": \"$(date -u +%FT%TZ)\","
        echo "  \"generator\": \"singular/core/v24/causal_machine/build_v24.sh (values parsed from evidence/*.log)\","
        echo "  \"simulator\": \"riscv-isa-sim (Spike) commit $(git -C "$SPIKE_SRC" rev-parse HEAD 2>/dev/null || echo UNKNOWN) + customext/omega_causal.cc\","
        echo "  \"spike_extension_sha256\": \"$(shasum -a 256 "$SPIKE_SRC/customext/omega_causal.cc" | cut -d' ' -f1)\","
        echo "  \"isa\": \"rv64imac + Xomegacausal $(grep -m1 'version:' isa/omega_isa.yaml | awk '{print $2}')\","
        echo "  \"reference\": \"tools/rv64_ref.hpp + tools/omega_ref.hpp (independent implementation)\","
        echo "  \"programs\": ["
        for p in causal_update priv_trap prop_sw_vs_hw; do
            line=$(grep -m1 "^COSIM" "$EVID/G05_spike_$p.log" 2>/dev/null) || line=""
            [ $first -eq 0 ] && echo ","; first=0
            printf '    {"name": "%s", "spike_exit_code": %s, "cosim": "%s", "gate": "%s"}' "$p" \
                "$(grep -m1 '^spike_exit_code=' "$EVID/G05_spike_$p.log" 2>/dev/null | cut -d= -f2 || echo null)" "$line" \
                "$(awk -F'\t' -v g="G05_spike_$p" '$1 == g {print $2}' "$EVID/gates.tsv")"
        done
        echo ""; echo "  ],"
        echo "  \"fuzz\": \"$(grep -m1 '^FUZZ' "$EVID/G07_spike_vs_ref_fuzz.log" 2>/dev/null)\","
        echo "  \"isa_dynamic_instructions\": ["
        grep '^REGION' "$EVID/G06_isa_dynamic_instructions.log" 2>/dev/null | sed 's/.*/    "&"/' | paste -sd, - | sed 's/,/,\n/g'
        echo "  ],"
        echo "  \"not_claimed\": [\"FPGA or silicon execution\", \"CVA6 integration\", \"multi-hart semantics\", \"cycle-accurate timing (Spike is an ISA simulator)\"]"
        echo "}"
    } > "$out"
    echo "wrote $out"
}
spike_json

echo "TOTAL pass=$PASS fail=$FAIL skip=$SKIP" | tee "$EVID/summary.txt"
[ $FAIL -eq 0 ] && echo "$V24_SHA" > "$V24_STAMP"
[ $FAIL -eq 0 ]
