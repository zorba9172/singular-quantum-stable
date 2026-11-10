#pragma once
// SINGULAR Ω v24 — `.sga` Symbolic Assembly Parser (`∂` vs `Ω.PROP` Enforced)
#include "../lexer/sga_lexer.hpp"

namespace singular::v24::language {

inline bool validate_sga_program(const std::string& src) noexcept {
    bool has_prop  = (src.find("Ω.PROP") != std::string::npos);
    bool has_deriv = (src.find("∂(") != std::string::npos);
    bool no_overloaded_deriv = (src.find("∂ affected") == std::string::npos);
    return has_prop && has_deriv && no_overloaded_deriv;
}

} // namespace singular::v24::language
