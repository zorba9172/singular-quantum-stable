#pragma once
// SINGULAR Ω v24 — `.sga` Symbolic Assembly Lexer
#include <sstream>
#include <string>
#include <vector>

namespace singular::v24::language {

inline std::vector<std::string> tokenize_sga(const std::string& src) {
    std::vector<std::string> tokens;
    std::istringstream iss(src);
    std::string tok;
    while (iss >> tok) tokens.push_back(tok);
    return tokens;
}

} // namespace singular::v24::language
