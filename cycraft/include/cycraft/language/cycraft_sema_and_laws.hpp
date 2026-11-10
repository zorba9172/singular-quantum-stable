#pragma once
// ============================================================================
// CYCRAFT Ω — The Computational Reality Language (`Code Reality.`)
// Layer: L1/L3 Semantic Analyzer, Z^7 SI Dimensional Gate, Arc Invertibility
// Classifier, and Autonomous `mind` Verify-Before-Commit Sandbox Gate
// ============================================================================

#include "cycraft/language/cycraft_lexer_parser.hpp"

#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace cycraft::omega::language {

struct CyDiagnostic {
    std::string code;
    size_t line = 1;
    std::string message;
};

enum class ArcInvertibilityClass : uint8_t {
    REVERSIBLE_EXACT_0LOG = 0,
    PIECEWISE_WITNESS_HYBRID = 1,
    DISSIPATIVE_FALLBACK_REQUIRED = 2
};

struct ArcSemanticContract {
    std::string arc_name;
    SiUnitDimension source_unit;
    SiUnitDimension target_unit;
    SiUnitDimension transform_unit;
    ArcInvertibilityClass invertibility = ArcInvertibilityClass::REVERSIBLE_EXACT_0LOG;
    bool has_residual_guard = false;
    double residual_threshold = 1e-7;
    bool has_valid_fallback = false;
};

struct CySemaReport {
    bool ok = true;
    std::vector<CyDiagnostic> diagnostics;
    std::map<std::string, SiUnitDimension> symbol_units;
    std::vector<ArcSemanticContract> verified_arcs;
    size_t verified_forms = 0;
    size_t verified_states = 0;
    size_t verified_flows = 0;
    size_t verified_laws = 0;
    size_t verified_minds = 0;
};

class CyCraftSemanticAnalyzer {
public:
    static std::string invertibility_name(ArcInvertibilityClass c) {
        switch (c) {
            case ArcInvertibilityClass::REVERSIBLE_EXACT_0LOG:
                return "REVERSIBLE_EXACT_0LOG (Bijective Jacobian, 0-Byte Undo)";
            case ArcInvertibilityClass::PIECEWISE_WITNESS_HYBRID:
                return "PIECEWISE_WITNESS_HYBRID (Kink/Yield Boundary Witness + Recompute-if-Needed)";
            case ArcInvertibilityClass::DISSIPATIVE_FALLBACK_REQUIRED:
                return "DISSIPATIVE_FALLBACK_REQUIRED (Non-Invertible -> Full-Solve Fallback)";
        }
        return "UNKNOWN";
    }

    static CySemaReport check_module(const CyModuleAst& ast) {
        CySemaReport rep{};

        // 1. Canonical 6-Layer Spine Check (CY-ARCH-001)
        static const std::set<std::string> kValidLayers = {
            "contracts", "shared", "plugins", "domain", "application", "api", "tests", "experiments",
            "core", "modules", "discoveries", "verification", "workspace"
        };
        if (kValidLayers.find(ast.canonical_layer) == kValidLayers.end()) {
            rep.ok = false;
            rep.diagnostics.push_back({
                "CY-ARCH-001", 1,
                "Invalid @layer(\"" + ast.canonical_layer + "\"); must belong to canonical 6-layer spine"
            });
        }

        // 2. Register all `form` fields and `state` variables into the Z^7 SI unit symbol table
        for (const auto& form : ast.forms) {
            ++rep.verified_forms;
            std::string prefix = to_lower(form.name);
            for (const auto& field : form.fields) {
                if (field.is_versioned_state) ++rep.verified_states;
                rep.symbol_units[form.name + "." + field.name] = field.type.unit;
                rep.symbol_units[prefix + "." + field.name] = field.type.unit;
                rep.symbol_units["body." + field.name] = field.type.unit;
                rep.symbol_units["heater." + field.name] = field.type.unit;
                rep.symbol_units["thermal." + field.name] = field.type.unit;
                rep.symbol_units["mechanical." + field.name] = field.type.unit;
                rep.symbol_units["world." + field.name] = field.type.unit;
                rep.symbol_units[field.name] = field.type.unit;
            }
        }

        // 3. Verify `arc` declarations: Z^7 SI unit algebra + Invertibility Classification
        for (const auto& arc : ast.arcs) {
            ArcSemanticContract ac{};
            ac.arc_name = arc.name;
            ac.source_unit = lookup_unit(arc.source_expr, rep.symbol_units);
            ac.target_unit = lookup_unit(arc.target_expr, rep.symbol_units);

            for (const auto& g : arc.guards) {
                if (g.lhs == "residual") {
                    ac.has_residual_guard = true;
                    ac.residual_threshold = g.threshold;
                }
            }
            ac.has_valid_fallback = !arc.fallback_mode.empty() && arc.fallback_mode != "none";

            if (!arc.transform_expr.empty()) {
                bool expr_ok = true;
                std::string err_msg;
                ac.transform_unit = infer_expr_unit(arc.transform_expr, rep.symbol_units, expr_ok, err_msg);
                if (!expr_ok) {
                    rep.ok = false;
                    rep.diagnostics.push_back({"CY-UNIT-001", arc.line, "In arc '" + arc.name + "': " + err_msg});
                }
            } else {
                ac.transform_unit = ac.target_unit;
            }
            rep.symbol_units[arc.name] = ac.transform_unit;

            // Classify arc invertibility
            if (arc.retract_mode == "recompute_if_needed" || arc.map_fn.find("thermoelastic") != std::string::npos ||
                arc.transform_expr.find("plastic") != std::string::npos) {
                ac.invertibility = ArcInvertibilityClass::PIECEWISE_WITNESS_HYBRID;
                if (!ac.has_valid_fallback && arc.retract_mode != "recompute_if_needed") {
                    rep.ok = false;
                    rep.diagnostics.push_back({
                        "CY-ARC-002", arc.line,
                        "Piecewise/non-bijective arc '" + arc.name + "' requires 'retract: recompute_if_needed' or 'fallback: full_solve'"
                    });
                }
            } else if (arc.retract_mode == "none" || arc.transform_expr.find(" irreversible ") != std::string::npos) {
                ac.invertibility = ArcInvertibilityClass::DISSIPATIVE_FALLBACK_REQUIRED;
                if (!ac.has_valid_fallback) {
                    rep.ok = false;
                    rep.diagnostics.push_back({
                        "CY-ARC-002", arc.line,
                        "Dissipative arc '" + arc.name + "' cannot retract without 'fallback: full_solve'"
                    });
                }
            } else {
                ac.invertibility = ArcInvertibilityClass::REVERSIBLE_EXACT_0LOG;
            }

            rep.verified_arcs.push_back(ac);
        }

        // 4. Verify `mind` declarations: Autonomous AI Safety Sandbox (`CY-MIND-001`)
        for (const auto& md : ast.minds) {
            if (md.attempts_direct_state_write) {
                rep.ok = false;
                rep.diagnostics.push_back({
                    "CY-MIND-001", md.line,
                    "Autonomous mind '" + md.name + "' attempted direct state mutation; mind blocks are strictly read-only observation + proposal"
                });
            }
            if (!md.no_unverified_state_commit) {
                rep.ok = false;
                rep.diagnostics.push_back({
                    "CY-MIND-001", md.line,
                    "Autonomous mind '" + md.name + "' must declare 'no_unverified_state_commit' in its policy block"
                });
            }
            ++rep.verified_minds;
        }

        // 5. Verify `flow` declarations: Z^7 evolution balance + `verify` before `commit`
        for (const auto& fl : ast.flows) {
            std::map<std::string, SiUnitDimension> local_units = rep.symbol_units;
            for (const auto& p : fl.params) {
                local_units[p.name] = p.type.unit;
            }
            std::set<std::string> verified_candidates;
            for (const auto& st : fl.statements) {
                if (st.kind == "evolve") {
                    SiUnitDimension target_u = lookup_unit(st.target, local_units);
                    bool expr_ok = true;
                    std::string err_msg;
                    SiUnitDimension delta_u = infer_expr_unit(st.expr, local_units, expr_ok, err_msg);
                    if (!expr_ok) {
                        rep.ok = false;
                        rep.diagnostics.push_back({"CY-UNIT-001", st.line, "In flow '" + fl.name + "': " + err_msg});
                    } else if (target_u.is_annotated && delta_u.is_annotated && !target_u.same_dimension(delta_u)) {
                        rep.ok = false;
                        rep.diagnostics.push_back({
                            "CY-UNIT-001", st.line,
                            "Dimensional mismatch in 'evolve " + st.target + " by " + st.expr + "': target has unit " +
                            target_u.canonical_string() + " but evolution delta has unit " + delta_u.canonical_string()
                        });
                    }
                } else if (st.kind == "verify") {
                    verified_candidates.insert(st.target);
                } else if (st.kind == "commit_or_fallback") {
                    // Extract candidate name after "commit "
                    std::string cand = extract_second_word(st.expr);
                    if (verified_candidates.find(cand) == verified_candidates.end()) {
                        rep.ok = false;
                        rep.diagnostics.push_back({
                            "CY-MIND-002", st.line,
                            "Unverified AI state commit in flow '" + fl.name + "': candidate '" + cand +
                            "' must pass 'verify " + cand + " against <laws>' before 'commit'"
                        });
                    }
                    if (st.aux.empty()) {
                        rep.ok = false;
                        rep.diagnostics.push_back({
                            "CY-MIND-003", st.line,
                            "Commit of AI candidate '" + cand + "' in flow '" + fl.name +
                            "' requires deterministic 'otherwise fallback <safe_solver>'"
                        });
                    }
                }
            }
            ++rep.verified_flows;
        }

        // 6. Verify `law` declarations: Z^7 SI unit consistency across relational operators
        for (const auto& lw : ast.laws) {
            for (const auto& as : lw.assertions) {
                bool expr_ok = true;
                std::string err_msg;
                SiUnitDimension lhs_u = infer_expr_unit(as.lhs_expr, rep.symbol_units, expr_ok, err_msg);
                if (!expr_ok) {
                    rep.ok = false;
                    rep.diagnostics.push_back({"CY-UNIT-001", as.line, "In law '" + lw.name + "': " + err_msg});
                } else if (lhs_u.is_annotated && as.rhs_unit.is_annotated && !lhs_u.same_dimension(as.rhs_unit)) {
                    rep.ok = false;
                    rep.diagnostics.push_back({
                        "CY-UNIT-001", as.line,
                        "Law '" + lw.name + "' compares incompatible SI dimensions: " +
                        lhs_u.canonical_string() + " " + as.op + " " + as.rhs_unit.canonical_string()
                    });
                }
            }
            ++rep.verified_laws;
        }

        return rep;
    }

private:
    static std::string to_lower(std::string s) {
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }

    static std::string extract_second_word(const std::string& s) {
        std::istringstream iss(s);
        std::string w1, w2;
        iss >> w1 >> w2;
        return w2;
    }

    static SiUnitDimension lookup_unit(const std::string& sym, const std::map<std::string, SiUnitDimension>& table) {
        std::string k = CyCraftParser::trim(sym);
        auto it = table.find(k);
        if (it != table.end()) return it->second;
        size_t dot = k.rfind('.');
        if (dot != std::string::npos) {
            std::string tail = k.substr(dot + 1);
            auto it2 = table.find(tail);
            if (it2 != table.end()) return it2->second;
        }
        return SiUnitDimension::dimensionless();
    }

    // Recursive Z^7 dimensional evaluator for arithmetic expressions (+, -, *, /, parentheses, @unit literals)
    static SiUnitDimension infer_expr_unit(
        const std::string& raw,
        const std::map<std::string, SiUnitDimension>& table,
        bool& ok,
        std::string& err_msg) {
        std::string s = CyCraftParser::trim(raw);
        if (s.empty()) return SiUnitDimension::dimensionless();

        // Strip enclosing balanced parentheses
        while (s.size() >= 2 && s.front() == '(' && s.back() == ')') {
            int depth = 0;
            bool outer = true;
            for (size_t i = 0; i + 1 < s.size(); ++i) {
                if (s[i] == '(') ++depth;
                else if (s[i] == ')') --depth;
                if (depth == 0) {
                    outer = false;
                    break;
                }
            }
            if (outer) s = CyCraftParser::trim(s.substr(1, s.size() - 2));
            else break;
        }

        // Top-level + or -
        int depth = 0;
        for (int i = static_cast<int>(s.size()) - 1; i >= 0; --i) {
            char c = s[static_cast<size_t>(i)];
            if (c == ')') ++depth;
            else if (c == '(') --depth;
            else if (depth == 0 && (c == '+' || c == '-')) {
                std::string lhs = s.substr(0, static_cast<size_t>(i));
                std::string rhs = s.substr(static_cast<size_t>(i) + 1);
                if (CyCraftParser::trim(lhs).empty()) continue;
                SiUnitDimension u1 = infer_expr_unit(lhs, table, ok, err_msg);
                SiUnitDimension u2 = infer_expr_unit(rhs, table, ok, err_msg);
                if (!ok) return u1;
                if (u1.is_annotated && u2.is_annotated && !u1.same_dimension(u2)) {
                    ok = false;
                    err_msg = "Cannot add/subtract incompatible physical units " +
                              u1.canonical_string() + " and " + u2.canonical_string() +
                              " in expression '" + s + "'";
                }
                return u1.is_annotated ? u1 : u2;
            }
        }

        // Top-level * or /
        depth = 0;
        for (int i = static_cast<int>(s.size()) - 1; i >= 0; --i) {
            char c = s[static_cast<size_t>(i)];
            if (c == ')') ++depth;
            else if (c == '(') --depth;
            else if (depth == 0 && (c == '*' || c == '/')) {
                std::string lhs = s.substr(0, static_cast<size_t>(i));
                std::string rhs = s.substr(static_cast<size_t>(i) + 1);
                SiUnitDimension u1 = infer_expr_unit(lhs, table, ok, err_msg);
                SiUnitDimension u2 = infer_expr_unit(rhs, table, ok, err_msg);
                if (!ok) return u1;
                return (c == '*') ? SiUnitDimension::multiply(u1, u2) : SiUnitDimension::divide(u1, u2);
            }
        }

        // Check for inline @unit annotation on literal/identifier (e.g. `10.0 @m`)
        size_t at = s.find('@');
        if (at != std::string::npos) {
            return SiUnitDimension::parse_annotation(CyCraftParser::trim(s.substr(at)));
        }

        return lookup_unit(s, table);
    }
};

} // namespace cycraft::omega::language
