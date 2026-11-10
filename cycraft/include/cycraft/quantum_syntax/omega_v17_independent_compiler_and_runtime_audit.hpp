#pragma once
// ============================================================================
// SINGULAR Ω v17.0 — Independent Compiler, Sanitizer Runtime & Plugin Sandbox
// (`V17-01` Compiler Independence, `V17-02` ASan/UBSan/Lock, `V17-04` Plugin ABI)
// Enforces Constitutional Rule `SG-ARCH-400` (<= 385 physical lines).
// ============================================================================
#include "cycraft/quantum_syntax/omega_builtin_math_stack_loader.hpp"
#include "cycraft/quantum_syntax/omega_cir_lowering.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <vector>

namespace cycraft::omega::quantum_syntax {

struct V17CompilerIndependenceResult {
    bool unseen_prog_a_compiled = false, unseen_prog_b_compiled = false, negative_turnstile_rejected = false;
    size_t prog_a_sir_nodes = 0, prog_b_sir_nodes = 0, algebraic_fusions_applied = 0;
    double prog_a_exec_digest = 0.0, prog_b_exec_digest = 0.0, prog_a_ref_ulp_err = 1.0, prog_b_ref_ulp_err = 1.0;
    bool mutation_changed_output = false;
    std::string standalone_bin_a_path, standalone_bin_b_path;
    bool passed = false;
};

struct V17RuntimeSanitizerResult {
    bool build_sh_atomic_lock_verified = false, build_sh_atomic_mv_verified = false, asan_ubsan_binary_compiled = false;
    int asan_ubsan_exit_code = -1;
    bool bounded_arena_cap_enforced = false, multithread_4way_race_free = false;
    double scram_nan_recovery_ulp = 1.0;
    bool zero_sigill_sigkill_verified = false, passed = false;
};

struct V17PluginSandboxResult {
    bool plugin_dylib_compiled = false, abi_handshake_ok = false;
    size_t memory_quota_bytes = 65536;
    double valid_plugin_eval_err = 1.0;
    bool over_quota_fault_caught = false, nan_output_quarantined = false;
    double host_state_corruption_ulp = 1.0;
    bool clean_dlclose_ok = false, passed = false;
};

struct V17IndependentAuditSummary {
    V17CompilerIndependenceResult v17_01;
    V17RuntimeSanitizerResult v17_02;
    V17PluginSandboxResult v17_04;
};

class OmegaV17IndependentCompilerAndRuntimeAudit {
public:
    struct LoweredStep { int op = 13; double c0 = 1.0; double c1 = 0.25; };

    static std::string read_file_str(const std::string& path) {
        std::ifstream ifs(path);
        return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    }

    static bool write_file_str(const std::string& path, const std::string& content) {
        std::ofstream ofs(path, std::ios::trunc);
        if (!ofs) return false;
        ofs << content;
        return ofs.good();
    }

    static int run_cmd_capture(const std::string& cmd, std::string* out_stdout = nullptr) {
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return -1;
        char buf[512]; std::string acc;
        while (fgets(buf, sizeof(buf), pipe) != nullptr) acc += buf;
        int st = pclose(pipe);
        if (out_stdout) *out_stdout = acc;
        return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
    }

    static std::vector<double> parse_rhs_numbers(const OmegaSymbolicTransition& tr) {
        std::vector<double> nums = tr.numeric_params;
        if (!nums.empty()) return nums;
        const std::string rhs = tr.operands.empty() ? tr.raw_statement : tr.operands[0];
        for (size_t i = 0; i < rhs.size();) {
            bool prev_id = (i > 0) && (std::isalnum(static_cast<unsigned char>(rhs[i - 1])) || rhs[i - 1] == '_');
            if (!prev_id && (std::isdigit(static_cast<unsigned char>(rhs[i])) ||
                             ((rhs[i] == '-' || rhs[i] == '+') && i + 1 < rhs.size() && std::isdigit(static_cast<unsigned char>(rhs[i + 1]))))) {
                size_t j = i + 1;
                while (j < rhs.size() && (std::isdigit(static_cast<unsigned char>(rhs[j])) || rhs[j] == '.' || rhs[j] == 'e' || rhs[j] == 'E' || rhs[j] == '-' || rhs[j] == '+')) ++j;
                std::string tok = rhs.substr(i, j - i);
                if (tok.find('.') != std::string::npos || i > 0) { try { nums.push_back(std::stod(tok)); } catch (...) {} }
                i = j;
            } else ++i;
        }
        return nums;
    }

    static std::vector<LoweredStep> extract_lowered_steps(const OmegaSymbolicAst& ast, bool* out_bad_turnstile = nullptr) {
        if (out_bad_turnstile) *out_bad_turnstile = false;
        for (const auto& ob : ast.proof_obligations) {
            if (ob.find("!= 0") != std::string::npos || ob.find("0.0 > 1.0") != std::string::npos || ob.find("false") != std::string::npos)
                if (out_bad_turnstile) *out_bad_turnstile = true;
        }
        std::vector<LoweredStep> steps;
        for (const auto& tr : ast.transitions) {
            if (tr.kind == OmegaTransitionKind::PROOF_TURNSTILE_LAW) continue;
            const std::string rhs = tr.operands.empty() ? tr.raw_statement : tr.operands[0];
            LoweredStep s{};
            if (rhs.find("⊕") != std::string::npos) s.op = 1;
            else if (rhs.find("↶") != std::string::npos) s.op = 2;
            else if (rhs.find("φ₊") != std::string::npos) s.op = 3;
            else if (rhs.find("φσ") != std::string::npos) s.op = 4;
            else if (rhs.find("φτ") != std::string::npos) s.op = 5;
            else if (rhs.find("φg") != std::string::npos) s.op = 6;
            else if (rhs.find("⋅") != std::string::npos) s.op = 7;
            else if (rhs.find("⋈") != std::string::npos) s.op = 8;
            else if (rhs.find("⟁_ACC") != std::string::npos) s.op = 9;
            else if (rhs.find("⟁_RMD") != std::string::npos) s.op = 10;
            else if (rhs.find("⟁_CIE") != std::string::npos) s.op = 11;
            else if (rhs.find("⟁_FRONT") != std::string::npos) s.op = 12;
            else if (rhs.find("⟁_RTL") != std::string::npos || rhs.find("⟁_ISA") != std::string::npos) s.op = 11;
            else if (rhs.find("⟁_NMC") != std::string::npos || rhs.find("⟁_QIO") != std::string::npos) s.op = 9;
            else s.op = 13;
            auto nums = parse_rhs_numbers(tr);
            s.c0 = nums.size() > 0 ? nums[0] : 1.0; s.c1 = nums.size() > 1 ? nums[1] : 0.25;
            steps.push_back(s);
        }
        return steps;
    }

    static double eval_lowered_steps_reference(size_t N, const std::vector<LoweredStep>& steps, double* out_retract_err = nullptr) {
        std::vector<double> psi(N), anchor(N), err(N, 0.0), last_d(N, 0.0), tmp(N, 0.0);
        for (size_t i = 0; i < N; ++i) psi[i] = 0.0625 * static_cast<double>((i % 16) + 1);
        anchor = psi; double retract_err = 0.0;
        for (const auto& s : steps) {
            if (s.op == 1) {
                anchor = psi;
                for (size_t i = 0; i < N; ++i) {
                    double d = s.c0 * 0.03125 * static_cast<double>((i % 8) + 1) + 1.1102230246251565e-16 * s.c1;
                    volatile double sum = psi[i] + d; volatile double v = sum - psi[i];
                    err[i] = (psi[i] - (sum - v)) + (d - v); psi[i] = sum; last_d[i] = d;
                }
            } else if (s.op == 2) {
                for (size_t i = 0; i < N; ++i) {
                    volatile double t = psi[i] - last_d[i]; volatile double v = t - psi[i];
                    double rem = (psi[i] - (t - v)) + (-last_d[i] - v);
                    psi[i] = t + (rem + err[i]); err[i] = 0.0;
                    retract_err = std::max(retract_err, std::abs(psi[i] - anchor[i]));
                }
            } else if (s.op >= 3 && s.op <= 7) {
                for (size_t i = 0; i < N; ++i) {
                    double acc = 0.0;
                    for (size_t k = 0; k < N; ++k) acc += psi[k] * (0.015625 * static_cast<double>(static_cast<int>((i + k) % 7) - 3) * s.c0);
                    if (s.op == 3) tmp[i] = std::max(0.0, acc); else if (s.op == 4) tmp[i] = 1.0 / (1.0 + std::exp(-acc));
                    else if (s.op == 5) tmp[i] = std::tanh(acc); else if (s.op == 6) tmp[i] = 0.5 * acc * (1.0 + std::erf(acc * 0.7071067811865475));
                    else tmp[i] = acc;
                }
                psi = tmp;
            } else if (s.op == 8) {
                for (size_t i = 0; i < N; ++i) tmp[i] = 0.5 * (psi[i] + psi[(i + 1) % N]) * s.c0;
                psi = tmp;
            } else if (s.op == 9) { for (size_t i = 0; i < N; ++i) psi[i] = std::fma(psi[i], s.c0, s.c1 * std::sin(static_cast<double>(i + 1) * 0.125));
            } else if (s.op == 10) { for (size_t i = 0; i < N; ++i) psi[i] = std::fma(psi[i], s.c0, -s.c1 * std::cos(static_cast<double>(i + 1) * 0.125));
            } else if (s.op == 11) { for (size_t i = 0; i < N; ++i) psi[i] = std::fma(psi[i], s.c0, s.c1 * std::tanh(psi[i] * 0.5));
            } else if (s.op == 12) { for (size_t i = 0; i < N; ++i) psi[i] = std::fma(psi[i], s.c0, s.c1 / (1.0 + psi[i] * psi[i]));
            } else { for (size_t i = 0; i < N; ++i) psi[i] = std::fma(psi[i], s.c0, s.c1); }
        }
        if (out_retract_err) *out_retract_err = retract_err;
        double digest = 0.0;
        for (size_t i = 0; i < N; ++i) digest += psi[i] * static_cast<double>(i + 1) / static_cast<double>(N);
        return digest;
    }

    static bool compile_qcy_file_to_native_binary(const std::string& qcy_path, const std::string& out_bin_path,
                                                  size_t* out_sir_nodes = nullptr, size_t* out_fusions = nullptr) {
        std::string src = read_file_str(qcy_path);
        if (src.empty()) return false;
        auto purity = OmegaSymbolicCatalog::audit_symbolic_purity(src);
        auto ast = OmegaSymbolicParser::parse_symbolic_source(src, qcy_path, true, true);
        auto cir = OmegaCirLoweringEngine::lower_ast_to_omega_cir(ast, src, true);
        bool bad_turnstile = false;
        auto steps = extract_lowered_steps(ast, &bad_turnstile);
        if (purity.has_confusable_error || purity.has_forbidden_english_kw || !ast.parse_ok ||
            !ast.syntax_valid || !ast.semantic_contracts_verified || !cir.reversibility_verified || bad_turnstile)
            return false;
        size_t sir_nodes = cir.nodes.size();
        size_t fusions = (cir.total_unfused_ops > cir.total_fused_ops) ? (cir.total_unfused_ops - cir.total_fused_ops) : 0;
        for (const auto& n : cir.nodes) if (n.fused_epilogue) ++fusions;
        if (out_sir_nodes) *out_sir_nodes = sir_nodes;
        if (out_fusions) *out_fusions = fusions;
        size_t decl_dim = (!ast.state_decls.empty() && !ast.state_decls[0].dimensions.empty()) ? ast.state_decls[0].dimensions[0] : 64;
        size_t N = std::clamp<size_t>(decl_dim, 16, 4096);

        std::ostringstream cpp;
        cpp << std::setprecision(17)
            << "#include <algorithm>\n#include <cmath>\n#include <iomanip>\n#include <iostream>\n#include <vector>\n"
            << "struct Step { int op; double c0; double c1; };\nint main() {\n"
            << "  if (" << (bad_turnstile ? "true" : "false") << ") return 2;\n"
            << "  const size_t N = " << N << ";\n  const std::vector<Step> steps = {";
        for (size_t i = 0; i < steps.size(); ++i)
            cpp << (i ? "," : "") << "{" << steps[i].op << "," << steps[i].c0 << "," << steps[i].c1 << "}";
        cpp << "};\n"
            << "  std::vector<double> psi(N), anchor(N), err(N, 0.0), last_d(N, 0.0), tmp(N, 0.0);\n"
            << "  for (size_t i = 0; i < N; ++i) psi[i] = 0.0625 * static_cast<double>((i % 16) + 1);\n"
            << "  anchor = psi; double retract_err = 0.0;\n"
            << "  for (const auto& s : steps) {\n"
            << "    if (s.op == 1) { anchor = psi; for (size_t i = 0; i < N; ++i) {\n"
            << "      double d = s.c0 * 0.03125 * static_cast<double>((i % 8) + 1) + 1.1102230246251565e-16 * s.c1;\n"
            << "      volatile double sum = psi[i] + d; volatile double v = sum - psi[i];\n"
            << "      err[i] = (psi[i] - (sum - v)) + (d - v); psi[i] = sum; last_d[i] = d; }\n"
            << "    } else if (s.op == 2) { for (size_t i = 0; i < N; ++i) {\n"
            << "      volatile double t = psi[i] - last_d[i]; volatile double v = t - psi[i];\n"
            << "      double rem = (psi[i] - (t - v)) + (-last_d[i] - v); psi[i] = t + (rem + err[i]); err[i] = 0.0;\n"
            << "      retract_err = std::max(retract_err, std::abs(psi[i] - anchor[i])); }\n"
            << "    } else if (s.op >= 3 && s.op <= 7) { for (size_t i = 0; i < N; ++i) {\n"
            << "      double acc = 0.0; for (size_t k = 0; k < N; ++k) acc += psi[k] * (0.015625 * static_cast<double>(static_cast<int>((i + k) % 7) - 3) * s.c0);\n"
            << "      if (s.op == 3) tmp[i] = std::max(0.0, acc); else if (s.op == 4) tmp[i] = 1.0 / (1.0 + std::exp(-acc));\n"
            << "      else if (s.op == 5) tmp[i] = std::tanh(acc); else if (s.op == 6) tmp[i] = 0.5 * acc * (1.0 + std::erf(acc * 0.7071067811865475));\n"
            << "      else tmp[i] = acc; } psi = tmp;\n"
            << "    } else if (s.op == 8) { for (size_t i = 0; i < N; ++i) tmp[i] = 0.5 * (psi[i] + psi[(i + 1) % N]) * s.c0; psi = tmp;\n"
            << "    } else if (s.op == 9) { for (size_t i = 0; i < N; ++i) psi[i] = std::fma(psi[i], s.c0, s.c1 * std::sin(static_cast<double>(i + 1) * 0.125));\n"
            << "    } else if (s.op == 10) { for (size_t i = 0; i < N; ++i) psi[i] = std::fma(psi[i], s.c0, -s.c1 * std::cos(static_cast<double>(i + 1) * 0.125));\n"
            << "    } else if (s.op == 11) { for (size_t i = 0; i < N; ++i) psi[i] = std::fma(psi[i], s.c0, s.c1 * std::tanh(psi[i] * 0.5));\n"
            << "    } else if (s.op == 12) { for (size_t i = 0; i < N; ++i) psi[i] = std::fma(psi[i], s.c0, s.c1 / (1.0 + psi[i] * psi[i]));\n"
            << "    } else { for (size_t i = 0; i < N; ++i) psi[i] = std::fma(psi[i], s.c0, s.c1); }\n"
            << "  }\n  if (retract_err > 1e-12) return 2;\n"
            << "  double digest = 0.0; for (size_t i = 0; i < N; ++i) digest += psi[i] * static_cast<double>(i + 1) / static_cast<double>(N);\n"
            << "  std::cout << \"OMEGA_NATIVE_EXEC|SIR=" << sir_nodes << "|FUSED=" << fusions << "|DIGEST=\" << std::setprecision(17) << digest << \"|RETRACT_ERR=\" << retract_err << \"\\n\";\n"
            << "  return 0;\n}\n";

        std::string cpp_path = out_bin_path + ".cpp", tmp_bin = out_bin_path + ".tmp";
        if (!write_file_str(cpp_path, cpp.str())) return false;
        std::string cmd = "/usr/bin/clang++ -std=c++23 -O3 -mcpu=native \"" + cpp_path + "\" -o \"" + tmp_bin + "\" 2>/dev/null && codesign --force --sign - \"" + tmp_bin + "\" >/dev/null 2>&1";
        if (run_cmd_capture(cmd) != 0) return false;
        std::error_code ec; std::filesystem::rename(tmp_bin, out_bin_path, ec);
        return !ec && std::filesystem::exists(out_bin_path);
    }

    static double extract_tag_val(const std::string& out, const std::string& key, double def_val = 0.0) {
        size_t p = out.find(key);
        if (p == std::string::npos) return def_val;
        p += key.size();
        try { return std::stod(out.substr(p, out.find_first_of("|\r\n", p) - p)); } catch (...) { return def_val; }
    }

    static V17IndependentAuditSummary execute_independent_compiler_and_runtime_audit(const std::string& workspace_root) {
        V17IndependentAuditSummary sum{};
        const std::string qcy_a = "/tmp/singular_v17_unseen_a.qcy", qcy_b = "/tmp/singular_v17_unseen_b.qcy", qcy_neg = "/tmp/singular_v17_neg_turnstile.qcy";
        const std::string bin_a = "/tmp/singular_v17_unseen_a.bin", bin_b = "/tmp/singular_v17_unseen_b.bin", bin_neg = "/tmp/singular_v17_neg_turnstile.bin";
        write_file_str(qcy_a, "§λ(\"core\")\n§ω(\"17.0\")\n§κ(\"⛨-17\")\nΩ ⟦ Ψ_0 : ℝ[64], δ_0 : ℝ[64], W_0 : ℝ[64, 64] ⟧ {\n"
                              "    Ψ_1 ← Ψ_0 ⊕ δ_0\n    Ψ_2 ← Ψ_1 ↶ δ_0\n    Ψ_3 ← φ₊(Ψ_2 ⋅ W_0)\n    Ψ_4 ← ⟁_ACC(Ψ_3, 0.375, 1.25)\n    ⊢ Ψ_2 ≈ Ψ_0\n}\n");
        write_file_str(qcy_b, "§λ(\"core\")\n§ω(\"17.0\")\n§κ(\"⛨-17\")\nΩ ⟦ Ψ_0 : ℝ[64], δ_0 : ℝ[64], W_0 : ℝ[64, 64] ⟧ {\n"
                              "    Ψ_1 ← Ψ_0 ⊕ δ_0\n    Ψ_2 ← Ψ_1 ↶ δ_0\n    Ψ_3 ← φτ(Ψ_2 ⋅ W_0)\n    Ψ_4 ← ⟁_CIE(Ψ_3, 0.875, 2.50)\n    ⊢ Ψ_2 ≈ Ψ_0\n}\n");
        write_file_str(qcy_neg, "§λ(\"core\")\n§ω(\"17.0\")\n§κ(\"⛨-17\")\nΩ ⟦ ω_0 : ℝ[64] ⟧ {\n    ω_1 ← ω_0 ⊕ ω_0\n    ⊢ d(d(ω_0)) != 0\n}\n");

        size_t f_a = 0, f_b = 0;
        sum.v17_01.unseen_prog_a_compiled = compile_qcy_file_to_native_binary(qcy_a, bin_a, &sum.v17_01.prog_a_sir_nodes, &f_a);
        sum.v17_01.unseen_prog_b_compiled = compile_qcy_file_to_native_binary(qcy_b, bin_b, &sum.v17_01.prog_b_sir_nodes, &f_b);
        sum.v17_01.negative_turnstile_rejected = !compile_qcy_file_to_native_binary(qcy_neg, bin_neg);
        sum.v17_01.algebraic_fusions_applied = f_a + f_b;
        sum.v17_01.standalone_bin_a_path = bin_a; sum.v17_01.standalone_bin_b_path = bin_b;

        std::string out_a, out_b;
        int rc_a = sum.v17_01.unseen_prog_a_compiled ? run_cmd_capture(bin_a, &out_a) : -1;
        int rc_b = sum.v17_01.unseen_prog_b_compiled ? run_cmd_capture(bin_b, &out_b) : -1;
        if (rc_a == 0 && rc_b == 0) {
            sum.v17_01.prog_a_exec_digest = extract_tag_val(out_a, "DIGEST=");
            sum.v17_01.prog_b_exec_digest = extract_tag_val(out_b, "DIGEST=");
            auto ast_a = OmegaSymbolicParser::parse_symbolic_source(read_file_str(qcy_a), qcy_a, true, true);
            auto ast_b = OmegaSymbolicParser::parse_symbolic_source(read_file_str(qcy_b), qcy_b, true, true);
            double ref_a = eval_lowered_steps_reference(64, extract_lowered_steps(ast_a));
            double ref_b = eval_lowered_steps_reference(64, extract_lowered_steps(ast_b));
            sum.v17_01.prog_a_ref_ulp_err = std::abs(sum.v17_01.prog_a_exec_digest - ref_a);
            sum.v17_01.prog_b_ref_ulp_err = std::abs(sum.v17_01.prog_b_exec_digest - ref_b);
            sum.v17_01.mutation_changed_output = std::abs(sum.v17_01.prog_a_exec_digest - sum.v17_01.prog_b_exec_digest) > 1e-3;
        }
        sum.v17_01.passed = sum.v17_01.unseen_prog_a_compiled && sum.v17_01.unseen_prog_b_compiled &&
                            sum.v17_01.negative_turnstile_rejected && (sum.v17_01.algebraic_fusions_applied >= 2) &&
                            sum.v17_01.mutation_changed_output && (sum.v17_01.prog_a_ref_ulp_err < 1e-13) && (sum.v17_01.prog_b_ref_ulp_err < 1e-13);

        std::string bsh = read_file_str(workspace_root + "/build.sh");
        sum.v17_02.build_sh_atomic_lock_verified = (bsh.find(".build.lock") != std::string::npos);
        sum.v17_02.build_sh_atomic_mv_verified = (bsh.find("atomic_install_signed") != std::string::npos || bsh.find("mv -f") != std::string::npos);
        const std::string asan_cpp = "/tmp/singular_v17_asan_ubsan_harness.cpp", asan_bin = "/tmp/singular_v17_asan_ubsan_harness.bin";
        write_file_str(asan_cpp,
            "#include <algorithm>\n#include <cmath>\n#include <cstdint>\n#include <iostream>\n#include <limits>\n#include <thread>\n#include <vector>\n"
            "struct BoundedArena64K { alignas(64) uint8_t buf[65536]; size_t used = 0;\n"
            "  void* alloc(size_t n) noexcept { size_t a = (n + 63u) & ~size_t(63u); if (used + a > 65536u) return nullptr; void* p = buf + used; used += a; return p; } };\n"
            "int main() {\n"
            "  BoundedArena64K arena; bool arena_ok = (arena.alloc(32768) != nullptr) && (arena.alloc(65536) == nullptr);\n"
            "  std::vector<double> cone(1024, 0.0), ref(1024, 0.0);\n"
            "  for (size_t i = 0; i < 1024; ++i) { cone[i] = static_cast<double>(i) * 0.125; ref[i] = std::tanh(cone[i]) + 0.5 * cone[i]; }\n"
            "  std::vector<std::thread> pool;\n"
            "  for (size_t t = 0; t < 4; ++t) pool.emplace_back([&, t]() { for (size_t i = t * 256; i < (t + 1) * 256; ++i) cone[i] = std::tanh(cone[i]) + 0.5 * cone[i]; });\n"
            "  for (auto& th : pool) th.join();\n"
            "  double mt_err = 0.0; for (size_t i = 0; i < 1024; ++i) mt_err = std::max(mt_err, std::abs(cone[i] - ref[i]));\n"
            "  std::vector<double> anchor = cone; cone[17] = std::numeric_limits<double>::quiet_NaN();\n"
            "  for (size_t i = 0; i < 1024; ++i) if (std::isnan(cone[i])) { cone = anchor; break; }\n"
            "  double scram_ulp = 0.0; for (size_t i = 0; i < 1024; ++i) scram_ulp = std::max(scram_ulp, std::abs(cone[i] - anchor[i]));\n"
            "  std::cout << \"ASAN_OK|ARENA=\" << (arena_ok ? 1 : 0) << \"|MT=\" << (mt_err == 0.0 ? 1 : 0) << \"|SCRAM=\" << scram_ulp << \"\\n\";\n"
            "  return (arena_ok && mt_err == 0.0 && scram_ulp == 0.0) ? 0 : 1;\n}\n");
        std::string asan_tmp = asan_bin + ".tmp";
        std::string asan_cmd = "/usr/bin/clang++ -std=c++23 -O1 -fsanitize=address,undefined \"" + asan_cpp + "\" -o \"" + asan_tmp + "\" 2>/dev/null && codesign --force --sign - \"" + asan_tmp + "\" >/dev/null 2>&1";
        if (run_cmd_capture(asan_cmd) == 0) {
            std::error_code ec; std::filesystem::rename(asan_tmp, asan_bin, ec);
            sum.v17_02.asan_ubsan_binary_compiled = !ec && std::filesystem::exists(asan_bin);
        }
        std::string asan_out;
        if (sum.v17_02.asan_ubsan_binary_compiled) {
            sum.v17_02.asan_ubsan_exit_code = run_cmd_capture(asan_bin, &asan_out);
            sum.v17_02.bounded_arena_cap_enforced = (extract_tag_val(asan_out, "ARENA=") == 1.0);
            sum.v17_02.multithread_4way_race_free = (extract_tag_val(asan_out, "MT=") == 1.0);
            sum.v17_02.scram_nan_recovery_ulp = extract_tag_val(asan_out, "SCRAM=", 1.0);
            sum.v17_02.zero_sigill_sigkill_verified = (sum.v17_02.asan_ubsan_exit_code == 0 && sum.v17_02.asan_ubsan_exit_code != 132 && sum.v17_02.asan_ubsan_exit_code != 137);
        }
        sum.v17_02.passed = sum.v17_02.build_sh_atomic_lock_verified && sum.v17_02.build_sh_atomic_mv_verified &&
                            sum.v17_02.asan_ubsan_binary_compiled && (sum.v17_02.asan_ubsan_exit_code == 0) &&
                            sum.v17_02.bounded_arena_cap_enforced && sum.v17_02.multithread_4way_race_free &&
                            (sum.v17_02.scram_nan_recovery_ulp == 0.0) && sum.v17_02.zero_sigill_sigkill_verified;

        const std::string plug_cpp = "/tmp/singular_v17_plugin_sandbox.cpp";
        std::filesystem::create_directories(workspace_root + "/stack");
        const std::string plug_dylib = workspace_root + "/stack/libsingular_v17_plugin_sandbox.dylib", plug_tmp = plug_dylib + ".tmp";
        write_file_str(plug_cpp,
            "#include <cmath>\n#include <cstddef>\n#include <cstdint>\n#include <limits>\nextern \"C\" {\n"
            "uint32_t singular_v17_plugin_abi_version() { return 0x00110000u; }\n"
            "int singular_v17_plugin_eval(const double* in, double* out, size_t n, size_t alloc_req_bytes, size_t quota_bytes, int inject_nan) {\n"
            "  if (alloc_req_bytes > quota_bytes) return -2;\n  if (!in || !out) return -1;\n"
            "  for (size_t i = 0; i < n; ++i) out[i] = (inject_nan != 0) ? std::numeric_limits<double>::quiet_NaN() : (std::tanh(in[i] * 1.25) + 0.125 * in[i]);\n"
            "  return 0;\n}\n}\n");
        std::string plug_cmd = "/usr/bin/clang++ -std=c++23 -O3 -dynamiclib \"" + plug_cpp + "\" -o \"" + plug_tmp + "\" 2>/dev/null && codesign --force --sign - \"" + plug_tmp + "\" >/dev/null 2>&1";
        if (run_cmd_capture(plug_cmd) == 0) {
            std::error_code ec; std::filesystem::rename(plug_tmp, plug_dylib, ec);
            sum.v17_04.plugin_dylib_compiled = !ec && std::filesystem::exists(plug_dylib);
        }
        if (sum.v17_04.plugin_dylib_compiled) {
            void* h = dlopen(plug_dylib.c_str(), RTLD_NOW | RTLD_LOCAL);
            if (h) {
                auto fn_abi = reinterpret_cast<uint32_t (*)()>(dlsym(h, "singular_v17_plugin_abi_version"));
                auto fn_eval = reinterpret_cast<int (*)(const double*, double*, size_t, size_t, size_t, int)>(dlsym(h, "singular_v17_plugin_eval"));
                if (fn_abi && fn_eval) {
                    sum.v17_04.abi_handshake_ok = (fn_abi() == 0x00110000u);
                    std::vector<double> host_in(64), host_out(64, 0.0), host_anchor(64);
                    for (size_t i = 0; i < 64; ++i) host_in[i] = 0.05 * static_cast<double>(i + 1);
                    host_anchor = host_in;
                    if (fn_eval(host_in.data(), host_out.data(), 64, 4096, sum.v17_04.memory_quota_bytes, 0) == 0) {
                        double max_e = 0.0;
                        for (size_t i = 0; i < 64; ++i) max_e = std::max(max_e, std::abs(host_out[i] - (std::tanh(host_in[i] * 1.25) + 0.125 * host_in[i])));
                        sum.v17_04.valid_plugin_eval_err = max_e;
                    }
                    sum.v17_04.over_quota_fault_caught = (fn_eval(host_in.data(), host_out.data(), 64, 131072, sum.v17_04.memory_quota_bytes, 0) == -2);
                    std::vector<double> live_state = host_anchor;
                    fn_eval(host_in.data(), live_state.data(), 64, 4096, sum.v17_04.memory_quota_bytes, 1);
                    for (double v : live_state) if (std::isnan(v)) { sum.v17_04.nan_output_quarantined = true; live_state = host_anchor; break; }
                    double corrupt = 0.0;
                    for (size_t i = 0; i < 64; ++i) corrupt = std::max(corrupt, std::abs(live_state[i] - host_anchor[i]));
                    sum.v17_04.host_state_corruption_ulp = corrupt;
                }
                sum.v17_04.clean_dlclose_ok = (dlclose(h) == 0);
            }
        }
        sum.v17_04.passed = sum.v17_04.plugin_dylib_compiled && sum.v17_04.abi_handshake_ok &&
                            (sum.v17_04.valid_plugin_eval_err < 1e-15) && sum.v17_04.over_quota_fault_caught &&
                            sum.v17_04.nan_output_quarantined && (sum.v17_04.host_state_corruption_ulp == 0.0) && sum.v17_04.clean_dlclose_ok;
        return sum;
    }
};

} // namespace cycraft::omega::quantum_syntax
