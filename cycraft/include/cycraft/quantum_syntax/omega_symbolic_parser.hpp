#pragma once
// SINGULAR Ω — Quantum-Mathematical Syntax & Strict Reactor Sema (`<= 400` lines, `SG-ARCH-400`)
#include "cycraft/quantum_syntax/omega_symbolic_catalog.hpp"
#include "cycraft/quantum_syntax/omega_shape_dimensions.hpp"
#include "cycraft/quantum_syntax/omega_language_contract.hpp"
#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace cycraft::omega::quantum_syntax {

enum class OmegaStateFieldKind : uint8_t {
    UNKNOWN_FIELD, REAL_TENSOR, COMPLEX_QUANTUM, SPARSE_DELTA, HOLOGRAPHIC_PHASE, BIT_REGISTER, SECRET_MANIFOLD,
    SCALAR_FIELD, REVERSIBLE_STATE, SYMBOLIC_ALGEBRA, PROBABILISTIC_DIST, AUTONOMOUS_AGENT
};

struct OmegaStateManifoldDecl {
    std::string symbol_name; OmegaStateFieldKind field_kind = OmegaStateFieldKind::REAL_TENSOR;
    std::vector<size_t> dimensions; std::string raw_type_expr; size_t source_line = 0; bool shape_valid = true;
};

enum class OmegaTransitionKind : uint8_t {
    VECTOR_MAP_TENSOR, COMPOSE_CHAIN_FUSED, CATALOG_FUSED_MATMUL_ACT, PURE_DENSE_GEMM,
    SPARSE_DELTA_INJECT, FRECHET_INCREMENTAL_EVAL, EXACT_STATE_RETRACT, GRADIENT_NABLA_EVAL,
    PARALLEL_DISJOINT_OP, PROOF_TURNSTILE_LAW, QUANTUM_KET_INIT, QUANTUM_UNITARY_GATE,
    QUANTUM_EXPECTATION_BRA, QUANTUM_MEASURE_BORN, HPAM_PHASE_BIND_RESONATE, AI_SURPRISAL_TRIGGER,
    SYMBIOTIC_MUTUAL_JOIN, REACTOR_OSR_PROMOTE, SCRAM_ANCHOR_GUARD, MULTISCALE_BRIDGE_OP, AGENT_OPTIMIZE_SLICE,
    CYCLOTOMIC_ROPE_OP, SELECTIVE_SSM_SCAN, SIGMA_ORTHO_PROJ, SHEAF_SPEC_DECODE,
    STABILIZER_STAR_DECL, PERTURBED_CLUSTER_SOLVE, BIPARTITE_ENTANGLEMENT_CUT,
    NONCOMMUTING_HAM_DECL, AUTO_QUANTUM_EIGENSOLVE
};

struct OmegaSymbolicTransition {
    OmegaTransitionKind kind = OmegaTransitionKind::VECTOR_MAP_TENSOR;
    std::string lhs_target, operator_symbol;
    std::vector<std::string> operands;
    std::string raw_statement;
    bool requires_reversibility_proof = false, is_lossy_without_boundary_log = false;
    size_t source_line = 0;
    std::vector<double> numeric_params;
};

struct OmegaSymbolicAst {
    std::string source_path, canonical_layer, layer, owner, contract_id, system_name, error_code, error_message;
    std::vector<OmegaStateManifoldDecl> state_decls;
    std::vector<OmegaSymbolicTransition> transitions;
    std::vector<std::string> proof_obligations;
    ConfusableDiagnostic purity_diagnostic;
    size_t proof_obligations_count = 0, raw_byte_len = 0;
    bool semantic_contracts_verified = false, parse_ok = true, syntax_valid = true;
};

class OmegaSymbolicParser {
public:
    static std::string trim(const std::string& s) {
        size_t a = 0, b = s.size();
        while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
        while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
        return s.substr(a, b - a);
    }

    static std::string strip_comment(const std::string& line) {
        size_t p = line.find("//"); if (p != std::string::npos) return line.substr(0, p);
        size_t h = line.find('#'); return (h != std::string::npos) ? line.substr(0, h) : line;
    }

    static std::string extract_quoted(const std::string& line, size_t start_pos = 0) {
        size_t q1 = line.find('"', start_pos);
        if (q1 != std::string::npos) {
            size_t q2 = line.find('"', q1 + 1);
            return (q2 == std::string::npos) ? "" : line.substr(q1 + 1, q2 - q1 - 1);
        }
        size_t p1 = line.find('(', start_pos), p2 = line.find(')', start_pos);
        return (p1 != std::string::npos && p2 != std::string::npos && p2 > p1 + 1) ? trim(line.substr(p1 + 1, p2 - p1 - 1)) : "";
    }

    static bool is_valid_canonical_layer(const std::string& layer) {
        return valid_canonical_layer(layer);
    }

    static std::vector<size_t> parse_shape_brackets(const std::string& type_str) {
        bool valid = true;
        return parse_omega_shape_dimensions(type_str, valid);
    }

    static OmegaStateManifoldDecl parse_single_decl(const std::string& raw, size_t line_no) {
        OmegaStateManifoldDecl d{}; d.source_line = line_no; std::string cleaned = raw;
        for (const char* t : {"⟦", "⟧", "[[", "]]"}) {
            size_t pos = 0; std::string ts(t);
            while ((pos = cleaned.find(ts, pos)) != std::string::npos) cleaned.erase(pos, ts.size());
        }
        cleaned = trim(cleaned); size_t colon = cleaned.find(':');
        if (colon != std::string::npos) {
            d.symbol_name = trim(cleaned.substr(0, colon)); d.raw_type_expr = trim(cleaned.substr(colon + 1));
            if (!d.raw_type_expr.empty() && d.raw_type_expr.back() == ',') d.raw_type_expr.pop_back();
            d.dimensions = parse_omega_shape_dimensions(d.raw_type_expr, d.shape_valid);
            if (d.raw_type_expr.find("⚿") != std::string::npos || d.raw_type_expr.find("Secret") != std::string::npos) d.field_kind = OmegaStateFieldKind::SECRET_MANIFOLD;
            else if (d.raw_type_expr.find("ℂ") != std::string::npos || d.raw_type_expr.find("Quantum") != std::string::npos) d.field_kind = OmegaStateFieldKind::COMPLEX_QUANTUM;
            else if (d.raw_type_expr.find("Δℝ") != std::string::npos || d.raw_type_expr.find("SparseDelta") != std::string::npos || d.raw_type_expr.find("Causal") != std::string::npos) d.field_kind = OmegaStateFieldKind::SPARSE_DELTA;
            else if (d.raw_type_expr.find("⛨ℝ") != std::string::npos || d.raw_type_expr.find("Reversible") != std::string::npos) d.field_kind = OmegaStateFieldKind::REVERSIBLE_STATE;
            else if (d.raw_type_expr.find("𝕊") != std::string::npos || d.raw_type_expr.find("ℤ") != std::string::npos || d.raw_type_expr.find("Symbolic") != std::string::npos) d.field_kind = OmegaStateFieldKind::SYMBOLIC_ALGEBRA;
            else if (d.raw_type_expr.find("ℙ") != std::string::npos || d.raw_type_expr.find("Probabilistic") != std::string::npos) d.field_kind = OmegaStateFieldKind::PROBABILISTIC_DIST;
            else if (d.raw_type_expr.find("𝒜") != std::string::npos || d.raw_type_expr.find("Agent") != std::string::npos) d.field_kind = OmegaStateFieldKind::AUTONOMOUS_AGENT;
            else if (d.raw_type_expr == "ℝ[1]" || d.raw_type_expr.find("Scalar") != std::string::npos) d.field_kind = OmegaStateFieldKind::SCALAR_FIELD;
            else if (d.raw_type_expr.find("Φℝ") != std::string::npos || d.raw_type_expr.find("PhaseManifold") != std::string::npos) d.field_kind = OmegaStateFieldKind::HOLOGRAPHIC_PHASE;
            else if (d.raw_type_expr.find("𝔹") != std::string::npos || d.raw_type_expr.find("Bit") != std::string::npos) d.field_kind = OmegaStateFieldKind::BIT_REGISTER;
            else d.field_kind = OmegaStateFieldKind::REAL_TENSOR;
        }
        return d;
    }

    static OmegaSymbolicAst parse_qcy_source(const std::string& raw_src, const std::string& path = "<memory.qcy>") {
        return parse_symbolic_source(raw_src, path, true, false);
    }

    static OmegaSymbolicAst apply_singular_arc_line_delta(const OmegaSymbolicAst& base_ast, size_t trans_idx,
                                                          const std::string& delta_stmt, bool is_retract,
                                                          double& two_sum_acc, double& two_sum_err, double delta_w) {
        OmegaSymbolicAst out = base_ast;
        if (!is_retract) {
            double s = two_sum_acc + delta_w, v = s - two_sum_acc;
            two_sum_err += (two_sum_acc - (s - v)) + (delta_w - v); two_sum_acc = s;
            size_t arr = delta_stmt.find("←");
            if (arr != std::string::npos && trans_idx < out.transitions.size())
                out.transitions[trans_idx] = classify_transition(trim(delta_stmt.substr(0, arr)), trim(delta_stmt.substr(arr + std::string("←").size())), delta_stmt, trans_idx + 1);
        } else {
            double t = two_sum_acc - delta_w, v = t - two_sum_acc, rem = (two_sum_acc - (t - v)) + (-delta_w - v);
            two_sum_acc = t + (rem + two_sum_err); two_sum_err = 0.0;
        }
        return out;
    }

    static OmegaSymbolicAst parse_symbolic_source(const std::string& raw_src, const std::string& path = "<memory.qcy>",
                                                  bool force_strict_contract = false, bool bypass_siac_cache = false) {
        const bool strict_mode = force_strict_contract || (path != "<memory.qcy>" && path.find('<') == std::string::npos);
        const uint64_t siac_key = OmegaSymbolicCatalog::fast_fnv1a_64(raw_src) ^ (static_cast<uint64_t>(raw_src.size()) << 32) ^ (strict_mode ? 0x9E3779B97F4A7C15ULL : 0ULL);
        static std::unordered_map<uint64_t, OmegaSymbolicAst> s_siac_ast_cache;
        if (!bypass_siac_cache) {
            auto it = s_siac_ast_cache.find(siac_key);
            if (it != s_siac_ast_cache.end() && it->second.raw_byte_len == raw_src.size()) {
                OmegaSymbolicAst cached = it->second; cached.source_path = path; return cached;
            }
        }
        OmegaSymbolicAst ast{}; ast.source_path = path; ast.raw_byte_len = raw_src.size();
        ast.purity_diagnostic = OmegaSymbolicCatalog::audit_symbolic_purity(raw_src);
        if (ast.purity_diagnostic.has_confusable_error || ast.purity_diagnostic.has_forbidden_english_kw) {
            set_err(ast, ast.purity_diagnostic.code, ast.purity_diagnostic.message); return ast;
        }
        std::string canonical_src = OmegaSymbolicCatalog::canonicalize_nfc(OmegaSymbolicCatalog::to_symbolic_canonical(raw_src));
        std::istringstream iss(canonical_src); std::vector<std::string> lines;
        for (std::string raw_line; std::getline(iss, raw_line);) lines.push_back(raw_line);
        if (lines.size() > 400) { set_err(ast, "CY-RC-010-REACTOR-BUDGET", "File exceeds 400 physical lines limit"); return ast; }
        bool in_omega_header = false;
        for (size_t i = 0; i < lines.size(); ++i) {
            std::string line = trim(strip_comment(lines[i])); size_t line_no = i + 1;
            if (line.empty() || line == "{" || line == "}") continue;
            if (line.rfind("§λ(", 0) == 0 || line.rfind("@layer(", 0) == 0) { ast.canonical_layer = extract_quoted(line); ast.layer = ast.canonical_layer; continue; }
            if (line.rfind("§ω(", 0) == 0 || line.rfind("@owner(", 0) == 0) { ast.owner = extract_quoted(line); continue; }
            if (line.rfind("§κ(", 0) == 0 || line.rfind("@contract(", 0) == 0) { ast.contract_id = extract_quoted(line); continue; }
            if (line.rfind("@domain", 0) == 0) {
                ast.canonical_layer = "domain"; ast.layer = "domain"; ast.owner = extract_quoted(line, 0);
                size_t at2 = line.find('@', 7); if (at2 != std::string::npos) ast.contract_id = extract_quoted(line, at2);
                continue;
            }
            if (line.rfind("Ω[", 0) == 0 || line.rfind("Ω ⟦", 0) == 0 || line.rfind("Ω⟦", 0) == 0) {
                ast.system_name = "Ω_System";
                size_t open_len = (line.rfind("Ω ⟦", 0) == 0) ? std::string("Ω ⟦").size() : (line.rfind("Ω⟦", 0) == 0) ? std::string("Ω⟦").size() : std::string("Ω[").size();
                std::string rest = trim(line.substr(open_len)); size_t close_pos = rest.find("⟧");
                if (close_pos == std::string::npos && line.rfind("Ω[", 0) == 0) {
                    int b_depth = 1;
                    for (size_t k = 0; k < rest.size(); ++k) {
                        if (rest[k] == '[') ++b_depth; else if (rest[k] == ']' && --b_depth == 0) { close_pos = k; break; }
                    }
                }
                if (close_pos != std::string::npos) { parse_comma_decls(rest.substr(0, close_pos), line_no, ast.state_decls); in_omega_header = false; }
                else { if (!rest.empty()) parse_comma_decls(rest, line_no, ast.state_decls); in_omega_header = true; }
                continue;
            }
            if (line.find("⟦") != std::string::npos && line.find("⟧") != std::string::npos && line.find(':') != std::string::npos) {
                ast.state_decls.push_back(parse_single_decl(line, line_no)); continue;
            }
            if (in_omega_header) {
                size_t close_b = line.find("⟧");
                if (close_b == std::string::npos && (line == "]" || line.rfind("]", 0) == 0 || line.find("] {") != std::string::npos)) close_b = line.find(']');
                if (close_b != std::string::npos) {
                    std::string before = trim(line.substr(0, close_b));
                    if (!before.empty()) parse_comma_decls(before, line_no, ast.state_decls);
                    in_omega_header = false;
                } else parse_comma_decls(line, line_no, ast.state_decls);
                continue;
            }
            if (line.rfind("⊢", 0) == 0) {
                std::string p_expr = trim(line.substr(std::string("⊢").size()));
                if (p_expr.find("ℳ†") != std::string::npos || p_expr.find("unitary(ℳ)") != std::string::npos || p_expr.find("unitary(<MEAS>)") != std::string::npos) {
                    set_err(ast, "CY-MT-MEAS-NON-UNITARY", "Measurement ℳ is non-unitary and cannot satisfy ⊢ ℳ† ⋅ ℳ ≈ 1.0"); return ast;
                }
                ast.transitions.push_back({OmegaTransitionKind::PROOF_TURNSTILE_LAW, "", "⊢", {p_expr}, line, false, false, line_no});
                ast.proof_obligations.push_back(p_expr); ++ast.proof_obligations_count; continue;
            }
            size_t arrow = line.find("←");
            if (arrow != std::string::npos) {
                while (i + 1 < lines.size() && (std::count(line.begin(), line.end(), '(') > std::count(line.begin(), line.end(), ')') || (!line.empty() && line.back() == '\\'))) {
                    if (!line.empty() && line.back() == '\\') line.pop_back();
                    line += " " + trim(strip_comment(lines[++i]));
                }
                std::string lhs_part = trim(line.substr(0, arrow)), rhs_full = trim(line.substr(arrow + std::string("←").size())), inline_proof;
                size_t turn_pos = rhs_full.find("⊢");
                if (turn_pos != std::string::npos) { inline_proof = trim(rhs_full.substr(turn_pos + std::string("⊢").size())); rhs_full = trim(rhs_full.substr(0, turn_pos)); }
                ast.transitions.push_back(classify_transition(lhs_part, rhs_full, line, line_no));
                if (!inline_proof.empty()) {
                    ast.transitions.push_back({OmegaTransitionKind::PROOF_TURNSTILE_LAW, "", "⊢", {inline_proof}, "⊢ " + inline_proof, false, false, line_no});
                    ast.proof_obligations.push_back(inline_proof); ++ast.proof_obligations_count;
                }
                continue;
            }
            set_err(ast, "CY-RC-002-SYNTAX-UNRECOGNIZED", "Line " + std::to_string(line_no) + ": Unrecognized .qcy statement: " + line); return ast;
        }
        for (const auto& d : ast.state_decls) {
            if (!d.shape_valid) {
                set_err(ast, "CY-RC-005-SHAPE-MISMATCH", "Line " + std::to_string(d.source_line) + ": Invalid or overflowing dimension in " + d.raw_type_expr); return ast;
            }
            if ((d.raw_type_expr.rfind("Quantum[", 0) == 0 || d.raw_type_expr.rfind("ℂ[2^", 0) == 0) && !d.dimensions.empty() && d.dimensions[0] > 28) {
                set_err(ast, "CY-MT-QUANTUM-EXP-COST", "Dense statevector " + d.raw_type_expr + " exceeds 2^28 hardware limit"); return ast;
            }
        }
        if (strict_mode) validate_reactor_sema(ast, path);
        if (s_siac_ast_cache.size() >= 4096) s_siac_ast_cache.clear();
        s_siac_ast_cache[siac_key] = ast;
        return ast;
    }

private:
    static void set_err(OmegaSymbolicAst& ast, const std::string& code, const std::string& msg) {
        ast.parse_ok = false; ast.syntax_valid = false; ast.error_code = code; ast.error_message = msg;
    }

    static void validate_reactor_sema(OmegaSymbolicAst& ast, const std::string& path) {
        if (ast.canonical_layer.empty() || ast.owner.empty() || ast.contract_id.empty() || !is_valid_canonical_layer(ast.canonical_layer))
            return set_err(ast, "CY-RC-001-HEADER-MISSING", "Missing or invalid §λ/§ω/§κ header in " + path);
        if (ast.system_name.empty() || ast.state_decls.empty())
            return set_err(ast, "CY-RC-004-UNDECLARED-SYMBOL", "Missing Ω[...] system block or state declarations in " + path);
        if (ast.proof_obligations_count == 0)
            return set_err(ast, "CY-RC-006-CRITICALITY-TURNSTILE", "Missing mandatory ⊢ reactor criticality turnstile in " + path);
        if (ast.transitions.size() > 50)
            return set_err(ast, "CY-RC-010-REACTOR-BUDGET", "Ω[...] block exceeds 50 transitions limit in " + path);

        std::map<std::string, OmegaStateManifoldDecl> symtab;
        for (const auto& d : ast.state_decls) {
            if (!symtab.count(d.symbol_name) || d.dimensions.size() >= symtab[d.symbol_name].dimensions.size()) symtab[d.symbol_name] = d;
        }
        for (const auto& tr : ast.transitions) {
            if (tr.kind == OmegaTransitionKind::PROOF_TURNSTILE_LAW) continue;
            const std::string rhs = tr.operands.empty() ? "" : tr.operands[0];
            if (((rhs.find("_atomic") != std::string::npos && rhs.find("_continuum") != std::string::npos) ||
                 (rhs.find("_α") != std::string::npos && rhs.find("_κ") != std::string::npos)) && rhs.find("↦") == std::string::npos)
                return set_err(ast, "CY-RC-009-MULTISCALE-UNBRIDGED", "Unbridged Atomic+Continuum coupling");
            for (const auto& [sname, sdecl] : symtab) {
                if (sdecl.field_kind == OmegaStateFieldKind::SECRET_MANIFOLD && rhs.find(sname) != std::string::npos) {
                    if (tr.kind == OmegaTransitionKind::QUANTUM_MEASURE_BORN || rhs == sname)
                        return set_err(ast, "CY-RC-008-SECRET-LEAK", "Unsealed Secret[T] leak of " + sname);
                }
            }
            size_t dot_p = rhs.find("⋅");
            if (dot_p != std::string::npos && rhs.find("†") == std::string::npos && rhs.find("⊗") == std::string::npos) {
                std::string left_s = trim(rhs.substr(0, dot_p));
                size_t lp = left_s.rfind('(');
                if (lp != std::string::npos) left_s = trim(left_s.substr(lp + 1));
                std::string right_s = trim(rhs.substr(dot_p + std::string("⋅").size())), b_s;
                size_t plus_p = right_s.find('+');
                if (plus_p != std::string::npos) {
                    b_s = trim(right_s.substr(plus_p + 1));
                    if (!b_s.empty() && b_s.back() == ')') b_s.pop_back();
                    b_s = trim(b_s); right_s = trim(right_s.substr(0, plus_p));
                } else if (!right_s.empty() && right_s.back() == ')') { right_s.pop_back(); right_s = trim(right_s); }
                if (left_s.find('(') == std::string::npos) {
                    if (!symtab.count(left_s) || !symtab.count(right_s) || (!b_s.empty() && !symtab.count(b_s)))
                        return set_err(ast, "CY-RC-004-UNDECLARED-SYMBOL", "Undeclared tensor in contraction");
                    const auto& xd = symtab[left_s].dimensions;
                    const auto& wd = symtab[right_s].dimensions;
                    if (!xd.empty() && !wd.empty()) {
                        if (wd.size() != 2 || xd.back() != wd[0] || (!b_s.empty() && !symtab[b_s].dimensions.empty() && symtab[b_s].dimensions.back() != wd[1]))
                            return set_err(ast, "CY-RC-005-SHAPE-MISMATCH", "Incompatible tensor dimensions in " + left_s + " ⋅ " + right_s);
                        OmegaStateManifoldDecl out_d = symtab[left_s];
                        out_d.symbol_name = tr.lhs_target;
                        out_d.dimensions = (xd.size() == 2) ? std::vector<size_t>{xd[0], wd[1]} : std::vector<size_t>{wd[1]};
                        symtab[tr.lhs_target] = out_d; continue;
                    }
                }
            }
            for (const char* op_tok : {"⊕", "↶", "⋈"}) {
                std::string ops(op_tok); size_t op_p = rhs.find(ops);
                if (op_p != std::string::npos && rhs.find("⊕_trop") == std::string::npos) {
                    std::string a_s = trim(rhs.substr(0, op_p)), b_s = trim(rhs.substr(op_p + ops.size()));
                    if ((a_s.rfind("lossy_clamp(", 0) == 0 || a_s.rfind("φ₊(", 0) == 0) && a_s.back() == ')') {
                        size_t lp = a_s.find('('); a_s = trim(a_s.substr(lp + 1, a_s.size() - lp - 2));
                    }
                    if (a_s.find('(') == std::string::npos) {
                        if (!symtab.count(a_s)) return set_err(ast, "CY-RC-004-UNDECLARED-SYMBOL", "Undeclared state manifold '" + a_s + "'");
                        if (symtab.count(b_s) && symtab[a_s].field_kind == OmegaStateFieldKind::REAL_TENSOR &&
                            symtab[b_s].field_kind == OmegaStateFieldKind::REAL_TENSOR &&
                            !symtab[a_s].dimensions.empty() && !symtab[b_s].dimensions.empty() &&
                            symtab[a_s].dimensions != symtab[b_s].dimensions)
                            return set_err(ast, "CY-RC-005-SHAPE-MISMATCH", "Shape mismatch between " + a_s + " and " + b_s);
                        OmegaStateManifoldDecl out_d = symtab[a_s]; out_d.symbol_name = tr.lhs_target; symtab[tr.lhs_target] = out_d;
                    }
                    break;
                }
            }
            if (!tr.lhs_target.empty() && !symtab.count(tr.lhs_target))
                symtab[tr.lhs_target] = {tr.lhs_target, OmegaStateFieldKind::UNKNOWN_FIELD, {}, "auto", tr.source_line};
        }
        ast.semantic_contracts_verified = true;
    }

    static void parse_comma_decls(const std::string& chunk, size_t line_no, std::vector<OmegaStateManifoldDecl>& out) {
        int depth = 0; std::string cur;
        for (char c : chunk) {
            if (c == '[') ++depth; else if (c == ']') --depth;
            if (c == ',' && depth == 0) {
                std::string t = trim(cur);
                if (t.find(':') != std::string::npos) out.push_back(parse_single_decl(t, line_no));
                cur.clear();
            } else cur.push_back(c);
        }
        std::string t = trim(cur);
        if (t.find(':') != std::string::npos) out.push_back(parse_single_decl(t, line_no));
    }

    static OmegaSymbolicTransition classify_transition(const std::string& lhs, const std::string& rhs,
                                                       const std::string& raw_line, size_t line_no) {
        OmegaSymbolicTransition tr{OmegaTransitionKind::VECTOR_MAP_TENSOR, lhs, "⊗", {rhs}, raw_line, false, false, line_no};
        if (rhs.find("⚡") != std::string::npos) { tr.kind = OmegaTransitionKind::REACTOR_OSR_PROMOTE; tr.operator_symbol = "⚡"; }
        else if (rhs.find("⛨") != std::string::npos) { tr.kind = OmegaTransitionKind::SCRAM_ANCHOR_GUARD; tr.operator_symbol = "⛨"; tr.requires_reversibility_proof = true; }
        else if (rhs.find("↶") != std::string::npos) {
            tr.kind = OmegaTransitionKind::EXACT_STATE_RETRACT; tr.operator_symbol = "↶"; tr.requires_reversibility_proof = true;
            tr.is_lossy_without_boundary_log = (rhs.find("φ₊") != std::string::npos || rhs.find("lossy_clamp") != std::string::npos || rhs.find("NaN_unanchored") != std::string::npos);
        }
        else if (rhs.find("⋈") != std::string::npos) { tr.kind = OmegaTransitionKind::SYMBIOTIC_MUTUAL_JOIN; tr.operator_symbol = "⋈"; }
        else if (rhs.find("↦") != std::string::npos) { tr.kind = OmegaTransitionKind::MULTISCALE_BRIDGE_OP; tr.operator_symbol = "↦"; }
        else if (rhs.find("𝒜(") != std::string::npos || rhs.find("⟁_") != std::string::npos || rhs.find(".optimize(") != std::string::npos) { tr.kind = OmegaTransitionKind::AGENT_OPTIMIZE_SLICE; tr.operator_symbol = (rhs.find("⟁_") != std::string::npos) ? "⟁" : "𝒜"; }
        else if (rhs.find("|") != std::string::npos && rhs.find("⟩") != std::string::npos && rhs.find("⟨") == std::string::npos) { tr.kind = OmegaTransitionKind::QUANTUM_KET_INIT; tr.operator_symbol = "|ψ⟩"; }
        else if (rhs.find("⟨") != std::string::npos && rhs.find("⟩") != std::string::npos) { tr.kind = OmegaTransitionKind::QUANTUM_EXPECTATION_BRA; tr.operator_symbol = (rhs.find("⨂_TM") != std::string::npos) ? "⨂_TM" : "⟨Ψ|M|Ψ⟩"; }
        else if (rhs.find("ℳ(") != std::string::npos) { tr.kind = OmegaTransitionKind::QUANTUM_MEASURE_BORN; tr.operator_symbol = "ℳ"; }
        else if (rhs.find("⨂_TM") != std::string::npos || rhs.find("ℋ_ℤ") != std::string::npos || rhs.find("𝒰_π8") != std::string::npos || rhs.find("(H ⊗") != std::string::npos || rhs.rfind("U(", 0) == 0 || rhs.rfind("𝒰(", 0) == 0) { tr.kind = OmegaTransitionKind::QUANTUM_UNITARY_GATE; tr.operator_symbol = "𝒰_π8"; }
        else if (rhs.find("⊛_θ") != std::string::npos) { tr.kind = OmegaTransitionKind::CYCLOTOMIC_ROPE_OP; tr.operator_symbol = "⊛_θ"; }
        else if (rhs.find("⟐_S") != std::string::npos) { tr.kind = OmegaTransitionKind::SELECTIVE_SSM_SCAN; tr.operator_symbol = "⟐_S"; }
        else if (rhs.find("⊥_Σ") != std::string::npos) { tr.kind = OmegaTransitionKind::SIGMA_ORTHO_PROJ; tr.operator_symbol = "⊥_Σ"; }
        else if (rhs.find("◈_H") != std::string::npos) { tr.kind = OmegaTransitionKind::SHEAF_SPEC_DECODE; tr.operator_symbol = "◈_H"; }
        else if (rhs.find("𝒜_★") != std::string::npos || rhs.find("𝒬_θ") != std::string::npos || rhs.find("𝒫_θ") != std::string::npos) { tr.kind = OmegaTransitionKind::STABILIZER_STAR_DECL; tr.operator_symbol = "𝒜_★"; }
        else if (rhs.find("ℋ_λ") != std::string::npos) { tr.kind = OmegaTransitionKind::PERTURBED_CLUSTER_SOLVE; tr.operator_symbol = "ℋ_λ"; }
        else if (rhs.find("ℋ_ℚ") != std::string::npos) { tr.kind = OmegaTransitionKind::NONCOMMUTING_HAM_DECL; tr.operator_symbol = "ℋ_ℚ"; }
        else if (rhs.find("ℰ_λ") != std::string::npos) { tr.kind = OmegaTransitionKind::AUTO_QUANTUM_EIGENSOLVE; tr.operator_symbol = "ℰ_λ"; }
        else if (rhs.find("𝒮_vN") != std::string::npos || rhs.find("ℜ_Sch") != std::string::npos) { tr.kind = OmegaTransitionKind::BIPARTITE_ENTANGLEMENT_CUT; tr.operator_symbol = (rhs.find("𝒮_vN") != std::string::npos) ? "𝒮_vN" : "ℜ_Sch"; }
        else if (rhs.find("φ₊") != std::string::npos || rhs.find("φσ") != std::string::npos || rhs.find("φτ") != std::string::npos || rhs.find("φg") != std::string::npos) { tr.kind = OmegaTransitionKind::CATALOG_FUSED_MATMUL_ACT; tr.operator_symbol = (rhs.find("⋅") != std::string::npos) ? "φ₊∘⋅" : "φ₊"; }
        else if (rhs.find("⋅") != std::string::npos) { tr.kind = OmegaTransitionKind::PURE_DENSE_GEMM; tr.operator_symbol = "⋅"; }
        else if (rhs.find("∘") != std::string::npos) { tr.kind = OmegaTransitionKind::COMPOSE_CHAIN_FUSED; tr.operator_symbol = "∘"; }
        else if (rhs.find("⊕") != std::string::npos) { tr.kind = OmegaTransitionKind::SPARSE_DELTA_INJECT; tr.operator_symbol = "⊕"; }
        else if (rhs.find("Δ") != std::string::npos) { tr.kind = OmegaTransitionKind::FRECHET_INCREMENTAL_EVAL; tr.operator_symbol = "Δ"; }
        else if (rhs.find("∇") != std::string::npos) { tr.kind = OmegaTransitionKind::GRADIENT_NABLA_EVAL; tr.operator_symbol = "∇"; }
        else if (rhs.find("∥") != std::string::npos) { tr.kind = OmegaTransitionKind::PARALLEL_DISJOINT_OP; tr.operator_symbol = "∥"; }
        else if (rhs.find("Φ(") != std::string::npos || rhs.find("HPAM_") != std::string::npos) { tr.kind = OmegaTransitionKind::HPAM_PHASE_BIND_RESONATE; tr.operator_symbol = "Φ"; }
        else if (rhs.find("ℋ_CD") != std::string::npos || rhs.find("Φ_AI") != std::string::npos) { tr.kind = OmegaTransitionKind::AI_SURPRISAL_TRIGGER; tr.operator_symbol = "ℋ_CD"; }
        size_t p1 = rhs.find('('), p2 = rhs.rfind(')');
        if (p1 != std::string::npos && p2 != std::string::npos && p2 > p1 + 1) {
            std::istringstream pss(rhs.substr(p1 + 1, p2 - p1 - 1));
            for (std::string tok; std::getline(pss, tok, ',');) {
                tok = trim(tok);
                if (!tok.empty() && (std::isdigit(static_cast<unsigned char>(tok[0])) || ((tok[0] == '-' || tok[0] == '+') && tok.size() > 1 && std::isdigit(static_cast<unsigned char>(tok[1]))))) tr.numeric_params.push_back(std::stod(tok));
            }
        }
        return tr;
    }
};
} // namespace cycraft::omega::quantum_syntax
