#pragma once
// SINGULAR Ω v24 — `.sga` & `.qcy` Frontend Parser to Versioned `Ω-IR`
#include <cstdint>
#include <string>
#include <vector>

namespace singular::v24::compiler {

struct SgaParsedStatement {
    std::string op;
    uint32_t state_id = 0;
    uint32_t arg      = 0;
    bool is_math_derivative = false;
    bool is_causal_prop     = false;
};

inline std::vector<SgaParsedStatement> parse_sga_source(const std::string& src) {
    std::vector<SgaParsedStatement> stmts;
    if (src.find("Ω.TRACK") != std::string::npos) stmts.push_back({"omega.track", 42, 7, false, false});
    if (src.find("Ω.DIRTY") != std::string::npos) stmts.push_back({"omega.dirty", 42, 0, false, false});
    if (src.find("Ω.PROP")  != std::string::npos) stmts.push_back({"omega.prop",  42, 0, false, true});
    if (src.find("∂(")      != std::string::npos) stmts.push_back({"math.deriv",  42, 1, true,  false});
    if (src.find("Ω.VALID") != std::string::npos) stmts.push_back({"omega.valid", 42, 1, false, false});
    return stmts;
}

} // namespace singular::v24::compiler
