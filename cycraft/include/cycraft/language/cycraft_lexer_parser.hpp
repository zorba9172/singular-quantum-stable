#pragma once
// ============================================================================
// CYCRAFT Ω — The Computational Reality Language (`Code Reality.`)
// Layer: L1 Language Lexer & Recursive-Descent Parser (`cycraft_lexer_parser.hpp`)
// Enforces Constitutional Rule `SG-ARCH-400` (<= 400 lines per file).
// Parses the 6 First-Class Causal Constructs (`form`, `state`, `arc`, `flow`,
// `law`, `mind`), Native Application Fabric (`module`, `service`, `endpoint`),
// general-purpose functions (`fn`), and physical SI dimensional annotations.
// ============================================================================

#include "cycraft/language/cycraft_ast.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace cycraft::omega::language {

class CyCraftParser {
public:
    static std::string trim(const std::string& s) {
        size_t a = 0, b = s.size();
        while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
        while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
        return s.substr(a, b - a);
    }

    static std::string strip_comment(const std::string& line) {
        size_t p = line.find("//");
        if (p != std::string::npos) return line.substr(0, p);
        size_t h = line.find('#');
        return (h != std::string::npos) ? line.substr(0, h) : line;
    }

    static CyTypeDescriptor parse_type_and_unit(const std::string& raw_spec) {
        CyTypeDescriptor td{};
        std::string s = trim(raw_spec);
        size_t at_pos = s.rfind('@');
        if (at_pos != std::string::npos) {
            std::string unit_part = trim(s.substr(at_pos));
            size_t sp = unit_part.find_first_of(" \t=");
            if (sp != std::string::npos) unit_part = unit_part.substr(0, sp);
            td.unit = SiUnitDimension::parse_annotation(unit_part);
            s = trim(s.substr(0, at_pos));
        }
        td.raw_type_str = s;
        if (s.find("field<complex<f64>, space3>") != std::string::npos) { td.kind = CyDataType::FIELD_COMPLEX_SPACE3; td.shape = {16, 16, 1}; }
        else if (s.find("field<f64, space3>") != std::string::npos) { td.kind = CyDataType::FIELD_REAL_SPACE3; td.shape = {16, 16, 1}; }
        else if (s.find("complex<f64>") != std::string::npos) td.kind = CyDataType::COMPLEX_F64;
        else if (s.find("tensor<") != std::string::npos) td.kind = CyDataType::TENSOR_F64;
        else if (s == "f32") td.kind = CyDataType::F32;
        else if (s == "f64") td.kind = CyDataType::F64;
        else if (s == "i64") td.kind = CyDataType::I64;
        else if (s == "bool") td.kind = CyDataType::BOOL;
        else td.kind = CyDataType::CUSTOM_FORM;
        return td;
    }

    static CyModuleAst parse_source(const std::string& src, const std::string& path = "<memory.qcy>") {
        CyModuleAst ast{}; ast.source_path = path;
        std::istringstream iss(src); std::vector<std::string> lines;
        for (std::string rl; std::getline(iss, rl);) lines.push_back(rl);
        size_t i = 0;
        while (i < lines.size()) {
            std::string line = trim(strip_comment(lines[i])); size_t line_no = i + 1;
            if (line.empty()) { ++i; continue; }
            if (line.rfind("@layer(", 0) == 0 || line.rfind("§λ(", 0) == 0) { ast.canonical_layer = extract_quoted(line); ++i; continue; }
            if (line.rfind("@owner(", 0) == 0 || line.rfind("§ω(", 0) == 0) { ast.owner = extract_quoted(line); ++i; continue; }
            if (line.rfind("@contract(", 0) == 0 || line.rfind("§κ(", 0) == 0) { ast.contract_id = extract_quoted(line); ++i; continue; }
            if (line.rfind("Ω", 0) == 0 && ast.forms.empty()) {
                CyFieldDecl fld{"psi", true, parse_type_and_unit("f64"), 1.0, true, line_no};
                fld.type.ownership = CyOwnershipKind::VERSIONED_STATE;
                ast.forms.push_back({"OmegaManifold", {fld}, line_no});
                ast.arcs.push_back({"omega_arc", "psi", "psi", "", "", "exact", "exact_inverse", "full_solve", {}, line_no});
                ast.flows.push_back({"omega_flow", {}, "f64", "NEON", "", "", {{"evolve", "psi", "psi", "", line_no}}, line_no});
                ast.laws.push_back({"OmegaLaw", {}, line_no});
                ++i; continue;
            }
            if (line.rfind("form ", 0) == 0) { ast.forms.push_back(parse_form_block(lines, i, line_no)); continue; }
            if (line.rfind("arc ", 0) == 0) { ast.arcs.push_back(parse_arc_block(lines, i, line_no)); continue; }
            if (line.rfind("flow ", 0) == 0) { ast.flows.push_back(parse_flow_block(lines, i, line_no)); continue; }
            if (line.rfind("law ", 0) == 0) { ast.laws.push_back(parse_law_block(lines, i, line_no)); continue; }
            if (line.rfind("mind ", 0) == 0) { ast.minds.push_back(parse_mind_block(lines, i, line_no)); continue; }
            if (line.rfind("fn ", 0) == 0) { ast.functions.push_back(parse_fn_block(lines, i, line_no)); continue; }
            if (line.rfind("module ", 0) == 0) { ast.module_contracts.push_back(parse_module_contract_block(lines, i, line_no)); continue; }
            if (line.rfind("service ", 0) == 0) { ast.services.push_back(parse_service_block(lines, i, line_no)); continue; }
            ++i;
        }
        return ast;
    }

private:
    static CyFormDecl parse_form_block(const std::vector<std::string>& lines, size_t& i, size_t line_no) {
        CyFormDecl f{};
        f.line = line_no;
        f.name = extract_identifier_after(trim(strip_comment(lines[i])), "form ");
        ++i;
        while (i < lines.size()) {
            std::string fl = trim(strip_comment(lines[i]));
            if (fl == "}" || fl.rfind("}", 0) == 0) { ++i; break; }
            if (!fl.empty() && fl != "{") {
                CyFieldDecl fd{};
                fd.line = i + 1;
                if (fl.rfind("state ", 0) == 0) {
                    fd.is_versioned_state = true;
                    fl = trim(fl.substr(6));
                }
                size_t colon = fl.find(':');
                if (colon != std::string::npos) {
                    fd.name = trim(fl.substr(0, colon));
                    std::string rhs = trim(fl.substr(colon + 1));
                    size_t eq = rhs.find('=');
                    if (eq != std::string::npos) {
                        std::string val_str = trim(rhs.substr(eq + 1));
                        rhs = trim(rhs.substr(0, eq));
                        fd.initial_value = std::strtod(val_str.c_str(), nullptr);
                        fd.has_initial_value = true;
                    }
                    fd.type = parse_type_and_unit(rhs);
                    if (fd.is_versioned_state) fd.type.ownership = CyOwnershipKind::VERSIONED_STATE;
                    f.fields.push_back(fd);
                }
            }
            ++i;
        }
        return f;
    }

    static CyArcDecl parse_arc_block(const std::vector<std::string>& lines, size_t& i, size_t line_no) {
        CyArcDecl a{};
        a.line = line_no;
        a.name = extract_identifier_after(trim(strip_comment(lines[i])), "arc ");
        ++i;
        bool in_guard = false;
        while (i < lines.size()) {
            std::string al = trim(strip_comment(lines[i]));
            if (al.empty()) { ++i; continue; }
            if (in_guard) {
                if (al == "}") in_guard = false;
                else {
                    CyArcGuardClause gc{};
                    parse_guard_clause(al, gc);
                    a.guards.push_back(gc);
                }
                ++i;
                continue;
            }
            if (al == "}") { ++i; break; }
            if (al.rfind("guard", 0) == 0 && al.find('{') != std::string::npos) {
                in_guard = true;
                ++i;
                continue;
            }
            size_t colon = al.find(':');
            if (colon != std::string::npos) {
                std::string key = trim(al.substr(0, colon));
                std::string val = trim(al.substr(colon + 1));
                if (key == "source") a.source_expr = val;
                else if (key == "target") a.target_expr = val;
                else if (key == "transform") a.transform_expr = val;
                else if (key == "map") a.map_fn = val;
                else if (key == "delta") a.delta_mode = val;
                else if (key == "retract") a.retract_mode = val;
                else if (key == "fallback") a.fallback_mode = val;
            }
            ++i;
        }
        return a;
    }

    static CyFlowDecl parse_flow_block(const std::vector<std::string>& lines, size_t& i, size_t line_no) {
        CyFlowDecl fl{};
        fl.line = line_no;
        std::string header = trim(strip_comment(lines[i]));
        while (i + 1 < lines.size() && header.find('{') == std::string::npos) {
            ++i;
            header += " " + trim(strip_comment(lines[i]));
        }
        parse_flow_header(header, fl);
        ++i;
        while (i < lines.size()) {
            std::string sl = trim(strip_comment(lines[i]));
            if (sl == "}") { ++i; break; }
            if (!sl.empty()) {
                CyFlowStmt st{};
                st.line = i + 1;
                if (sl.rfind("evolve ", 0) == 0) {
                    st.kind = "evolve";
                    size_t by_pos = sl.find(" by ");
                    if (by_pos != std::string::npos) { st.target = trim(sl.substr(7, by_pos - 7)); st.expr = trim(sl.substr(by_pos + 4)); }
                } else if (sl.rfind("solve ", 0) == 0) { st.kind = "solve"; st.expr = trim(sl.substr(6)); }
                else if (sl.rfind("verify ", 0) == 0) {
                    st.kind = "verify";
                    size_t ag = sl.find(" against ");
                    if (ag != std::string::npos) { st.target = trim(sl.substr(7, ag - 7)); st.expr = trim(sl.substr(ag + 9)); }
                } else if (sl.rfind("commit ", 0) == 0) {
                    st.kind = "commit_or_fallback"; st.expr = sl;
                    if (i + 1 < lines.size()) {
                        std::string next_l = trim(strip_comment(lines[i + 1]));
                        if (next_l.rfind("otherwise fallback ", 0) == 0) { st.aux = trim(next_l.substr(19)); ++i; }
                    }
                } else if (sl.find(".propose(") != std::string::npos) {
                    st.kind = "propose";
                    size_t eq = sl.find('=');
                    if (eq != std::string::npos) { st.target = trim(sl.substr(0, eq)); st.expr = trim(sl.substr(eq + 1)); }
                } else if (sl.rfind("return ", 0) == 0) { st.kind = "return"; st.expr = trim(sl.substr(7)); }
                else if (sl.find('=') != std::string::npos) {
                    st.kind = "assign";
                    size_t eq = sl.find('=');
                    st.target = trim(sl.substr(0, eq)); st.expr = trim(sl.substr(eq + 1));
                }
                fl.statements.push_back(st);
            }
            ++i;
        }
        return fl;
    }

    static CyLawDecl parse_law_block(const std::vector<std::string>& lines, size_t& i, size_t line_no) {
        CyLawDecl lw{}; lw.line = line_no;
        lw.name = extract_identifier_after(trim(strip_comment(lines[i])), "law ");
        ++i;
        while (i < lines.size()) {
            std::string ll = trim(strip_comment(lines[i]));
            if (ll == "}") { ++i; break; }
            if (ll.rfind("assert ", 0) == 0) {
                CyLawAssertion as{}; as.line = i + 1; as.raw_text = trim(ll.substr(7));
                parse_law_assertion(as.raw_text, as);
                lw.assertions.push_back(as);
            }
            ++i;
        }
        return lw;
    }

    static CyMindDecl parse_mind_block(const std::vector<std::string>& lines, size_t& i, size_t line_no) {
        CyMindDecl md{}; md.line = line_no;
        md.name = extract_identifier_after(trim(strip_comment(lines[i])), "mind ");
        ++i;
        std::string section;
        while (i < lines.size()) {
            std::string ml = trim(strip_comment(lines[i]));
            if (ml.empty()) { ++i; continue; }
            if (ml == "}") {
                if (!section.empty()) { section.clear(); ++i; continue; }
                ++i; break;
            }
            if (ml.rfind("model:", 0) == 0) md.model_id = extract_quoted(ml);
            else if (ml.rfind("observe", 0) == 0 && ml.find('{') != std::string::npos) section = "observe";
            else if (ml.rfind("propose", 0) == 0 && ml.find('{') != std::string::npos) section = "propose";
            else if (ml.rfind("policy", 0) == 0 && ml.find('{') != std::string::npos) section = "policy";
            else if (ml.rfind("mutate_state_directly", 0) == 0) md.attempts_direct_state_write = true;
            else if (section == "observe") md.observed_signals.push_back(ml);
            else if (section == "propose") md.proposed_actions.push_back(ml);
            else if (section == "policy") {
                if (ml == "no_unverified_state_commit") md.no_unverified_state_commit = true;
                else if (ml.rfind("maximum_error:", 0) == 0) md.maximum_error = std::strtod(trim(ml.substr(14)).c_str(), nullptr);
            }
            ++i;
        }
        return md;
    }

    static CyFunctionDecl parse_fn_block(const std::vector<std::string>& lines, size_t& i, size_t line_no) {
        CyFunctionDecl fn{}; fn.line = line_no;
        fn.name = extract_identifier_after(trim(strip_comment(lines[i])), "fn ");
        ++i;
        while (i < lines.size()) {
            std::string fnl = trim(strip_comment(lines[i]));
            if (fnl == "}") { ++i; break; }
            if (fnl.rfind("return ", 0) == 0) fn.body_expr = trim(fnl.substr(7));
            ++i;
        }
        return fn;
    }

    static CyModuleContractDecl parse_module_contract_block(const std::vector<std::string>& lines, size_t& i, size_t line_no) {
        CyModuleContractDecl mc{}; mc.line = line_no;
        mc.name = extract_identifier_after(trim(strip_comment(lines[i])), "module ");
        ++i;
        int brace_depth = 1;
        while (i < lines.size() && brace_depth > 0) {
            std::string ml = trim(strip_comment(lines[i]));
            for (char c : ml) { if (c == '{') ++brace_depth; else if (c == '}') --brace_depth; }
            if (ml.rfind("network:", 0) == 0) mc.network_policy = trim(ml.substr(8));
            else if (ml.rfind("secrets:", 0) == 0) mc.secrets_policy = trim(ml.substr(8));
            else if (ml.rfind("timeout:", 0) == 0) mc.timeout = trim(ml.substr(8));
            ++i;
        }
        return mc;
    }

    static CyServiceDecl parse_service_block(const std::vector<std::string>& lines, size_t& i, size_t line_no) {
        CyServiceDecl sv{}; sv.line = line_no;
        sv.name = extract_identifier_after(trim(strip_comment(lines[i])), "service ");
        ++i;
        int brace_depth = 1;
        CyEndpointDecl cur_ep{}; bool in_ep = false;
        while (i < lines.size() && brace_depth > 0) {
            std::string sl = trim(strip_comment(lines[i]));
            for (char c : sl) { if (c == '{') ++brace_depth; else if (c == '}') --brace_depth; }
            if (sl.rfind("inject ", 0) == 0) {
                size_t colon = sl.find(':');
                if (colon != std::string::npos) sv.injected_deps.push_back({trim(sl.substr(7, colon - 7)), trim(sl.substr(colon + 1))});
            } else if (sl.rfind("endpoint ", 0) == 0) {
                cur_ep = CyEndpointDecl{}; cur_ep.line = i + 1; cur_ep.name = extract_identifier_after(sl, "endpoint "); in_ep = true;
            } else if (in_ep) {
                if (sl.rfind("route:", 0) == 0) cur_ep.route = extract_quoted(sl);
                else if (sl.rfind("method:", 0) == 0) cur_ep.method = trim(sl.substr(7));
                else if (sl.rfind("auth:", 0) == 0) cur_ep.auth = trim(sl.substr(5));
                else if (sl.rfind("timeout:", 0) == 0) cur_ep.timeout = trim(sl.substr(8));
                else if (sl.rfind("return ", 0) == 0) cur_ep.body_expr = trim(sl.substr(7));
                else if (sl == "}" && brace_depth == 1) { sv.endpoints.push_back(cur_ep); in_ep = false; }
            }
            ++i;
        }
        return sv;
    }

    static std::string extract_quoted(const std::string& s) {
        size_t q1 = s.find('"');
        if (q1 == std::string::npos) return "";
        size_t q2 = s.find('"', q1 + 1);
        return (q2 == std::string::npos) ? "" : s.substr(q1 + 1, q2 - q1 - 1);
    }

    static std::string extract_identifier_after(const std::string& line, const std::string& prefix) {
        std::string rest = trim(line.substr(prefix.size()));
        size_t end = 0;
        while (end < rest.size() && (std::isalnum(static_cast<unsigned char>(rest[end])) || rest[end] == '_' || rest[end] == '.')) ++end;
        return rest.substr(0, end);
    }

    static void parse_guard_clause(const std::string& s, CyArcGuardClause& gc) {
        for (const char* op : {"<=", ">=", "==", "<", ">"}) {
            size_t p = s.find(op);
            if (p != std::string::npos) {
                gc.lhs = trim(s.substr(0, p)); gc.op = op;
                gc.rhs = trim(s.substr(p + std::strlen(op)));
                gc.threshold = std::strtod(gc.rhs.c_str(), nullptr);
                return;
            }
        }
    }

    static void parse_law_assertion(const std::string& s, CyLawAssertion& as) {
        for (const char* op : {"<=", ">=", "==", "<", ">"}) {
            size_t p = s.find(op);
            if (p != std::string::npos) {
                as.lhs_expr = trim(s.substr(0, p)); as.op = op;
                std::string rhs = trim(s.substr(p + std::strlen(op)));
                size_t at = rhs.find('@');
                if (at != std::string::npos) {
                    as.rhs_unit = SiUnitDimension::parse_annotation(trim(rhs.substr(at)));
                    rhs = trim(rhs.substr(0, at));
                }
                as.rhs_value = std::strtod(rhs.c_str(), nullptr);
                return;
            }
        }
    }

    static void parse_flow_header(const std::string& header, CyFlowDecl& fl) {
        fl.name = extract_identifier_after(header, "flow ");
        size_t lp = header.find('('), rp = header.find(')');
        if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
            std::istringstream pss(header.substr(lp + 1, rp - lp - 1));
            std::string item;
            while (std::getline(pss, item, ',')) {
                size_t colon = item.find(':');
                if (colon != std::string::npos) {
                    fl.params.push_back({trim(item.substr(0, colon)), parse_type_and_unit(trim(item.substr(colon + 1)))});
                }
            }
        }
        auto extract_kw = [&](const std::string& kw) -> std::string {
            size_t p = header.find(kw);
            if (p == std::string::npos) return "";
            std::string rest = trim(header.substr(p + kw.size()));
            size_t sp = rest.find_first_of(" \t{");
            return (sp != std::string::npos) ? rest.substr(0, sp) : rest;
        };
        fl.return_type = extract_kw("-> ");
        fl.using_backend = extract_kw("using ");
        fl.requires_clause = extract_kw("requires ");
        fl.ensures_clause = extract_kw("ensures ");
    }
};

} // namespace cycraft::omega::language
