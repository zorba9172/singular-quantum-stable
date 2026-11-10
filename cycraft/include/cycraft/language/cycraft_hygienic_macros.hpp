#pragma once
// ============================================================================
// SINGULAR Ω++ × CYCRAFT Ω+ — Hygienic AST Macros & Compile-Time Reflection
// Layer: L1 Language Metaprogramming (`cycraft_hygienic_macros.hpp` — `Gate P15`)
// Enforces Constitutional Rule `SG-ARCH-400` (<= 400 physical lines).
//   * Supports `macro <name>(params...) { quote { ... splice(x) ... } }`
//   * Enforces deterministic α-renaming (`__hyg_<macro>_<epoch>_<binder>`)
//   * Rejects accidental caller identifier capture with `SG-MAC-001`
//   * Enforces bounded compile-time macro expansion depth (`max_depth = 32`)
// ============================================================================

#include <cctype>
#include <cstdint>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace cycraft::omega::macros {

struct HygienicMacroDef {
    std::string name;
    std::vector<std::string> params;
    std::string quote_body;
    std::set<std::string> local_binders; // variables introduced via `let <id> =` inside `quote`
    bool allow_unhygienic_raw = false;
};

struct MacroExpansionResult {
    bool ok = true;
    std::string diagnostic_code; // "" or "SG-MAC-001" or "SG-MAC-002"
    std::string diagnostic_message;
    std::string expanded_source;
    size_t alpha_renamed_binders = 0;
};

class HygienicMacroExpander {
public:
    static constexpr size_t kMaxExpansionDepth = 32;

    static MacroExpansionResult expand_macro(
        const HygienicMacroDef& def,
        const std::vector<std::string>& call_args,
        const std::set<std::string>& caller_scope_symbols,
        uint64_t hygiene_epoch = 1,
        size_t current_depth = 0) {
        MacroExpansionResult res{};
        if (current_depth > kMaxExpansionDepth) {
            res.ok = false;
            res.diagnostic_code = "SG-MAC-002";
            res.diagnostic_message = "Compile-time macro expansion depth exceeded limit (32)";
            return res;
        }
        if (call_args.size() != def.params.size()) {
            res.ok = false;
            res.diagnostic_code = "SG-MAC-001";
            res.diagnostic_message = "Macro '" + def.name + "' arity mismatch";
            return res;
        }

        // Check if an unhygienic macro attempts to capture a caller variable directly
        if (def.allow_unhygienic_raw) {
            for (const auto& binder : def.local_binders) {
                if (caller_scope_symbols.count(binder) > 0) {
                    res.ok = false;
                    res.diagnostic_code = "SG-MAC-001";
                    res.diagnostic_message =
                        "Macro hygiene violation [SG-MAC-001]: macro '" + def.name +
                        "' captures caller symbol '" + binder + "' without α-renaming";
                    return res;
                }
            }
        }

        std::string work = def.quote_body;

        // 1. Alpha-rename every macro-local binder `x` -> `__hyg_<macro>_<epoch>_<x>`
        for (const auto& binder : def.local_binders) {
            std::string hygienic_name =
                "__hyg_" + def.name + "_" + std::to_string(hygiene_epoch) + "_" + binder;
            replace_whole_identifier(work, binder, hygienic_name);
            ++res.alpha_renamed_binders;
        }

        // 2. Substitute `splice(<param>)` with caller expression
        for (size_t i = 0; i < def.params.size(); ++i) {
            std::string pattern = "splice(" + def.params[i] + ")";
            size_t pos = 0;
            while ((pos = work.find(pattern, pos)) != std::string::npos) {
                work.replace(pos, pattern.size(), "(" + call_args[i] + ")");
                pos += call_args[i].size() + 2;
            }
        }

        res.expanded_source = work;
        return res;
    }

    static bool verify_p15_hygiene_gate(std::string& summary_out) {
        // 1. Valid `unless(cond, body)` hygienic macro with local binder `tmp_flag`
        HygienicMacroDef unless_macro{
            "unless",
            {"cond", "body"},
            "let tmp_flag = !(splice(cond)); if (tmp_flag) { splice(body) }",
            {"tmp_flag"},
            false
        };
        std::set<std::string> caller_syms = {"tmp_flag", "temperature", "dt"};
        auto valid_exp = expand_macro(unless_macro, {"temperature > 400.0", "evolve temperature by dt"}, caller_syms, 1);

        // Verify that caller's `tmp_flag` was NOT captured because macro's `tmp_flag` became `__hyg_unless_1_tmp_flag`
        bool hygiene_preserved =
            valid_exp.ok &&
            valid_exp.alpha_renamed_binders == 1 &&
            valid_exp.expanded_source.find("__hyg_unless_1_tmp_flag") != std::string::npos;

        // 2. Negative Test (`SG-MAC-001`): Unhygienic macro attempting to capture caller's `tmp_flag`
        HygienicMacroDef bad_capture_macro{
            "bad_swap",
            {"a", "b"},
            "let tmp_flag = splice(a); splice(a) = splice(b); splice(b) = tmp_flag;",
            {"tmp_flag"},
            true // attempts raw unrenamed capture
        };
        auto bad_exp = expand_macro(bad_capture_macro, {"tmp_flag", "dt"}, caller_syms, 2);
        bool capture_rejected = !bad_exp.ok && bad_exp.diagnostic_code == "SG-MAC-001";

        bool all_ok = hygiene_preserved && capture_rejected;
        summary_out = all_ok
            ? "PASS (α-Renamed `__hyg_unless_1_tmp_flag`, 0 Capture Violations, SG-MAC-001 Verified)"
            : "FAIL";
        return all_ok;
    }

private:
    static bool is_ident_char(char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
    }

    static void replace_whole_identifier(std::string& text, const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = text.find(from, pos)) != std::string::npos) {
            bool left_ok = (pos == 0) || !is_ident_char(text[pos - 1]);
            bool right_ok = (pos + from.size() >= text.size()) || !is_ident_char(text[pos + from.size()]);
            if (left_ok && right_ok) {
                text.replace(pos, from.size(), to);
                pos += to.size();
            } else {
                pos += from.size();
            }
        }
    }
};

} // namespace cycraft::omega::macros
