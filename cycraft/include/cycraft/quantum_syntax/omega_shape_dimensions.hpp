#pragma once
#include <charconv>
#include <cctype>
#include <string>
#include <vector>

namespace cycraft::omega::quantum_syntax {
// A failed dimension must never become a partial shape or a thrown conversion.
inline std::vector<size_t> parse_omega_shape_dimensions(const std::string& type, bool& valid) {
    valid = true;
    std::vector<size_t> dims;
    const auto begin = type.find('['), end = type.find(']');
    if (begin == std::string::npos && end == std::string::npos) return dims;
    if (begin == std::string::npos || end == std::string::npos || end <= begin + 1) {
        valid = false; return {};
    }
    size_t cursor = begin + 1;
    while (cursor <= end) {
        const auto comma = type.find(',', cursor);
        size_t last = comma == std::string::npos || comma > end ? end : comma;
        const size_t next = last + 1;
        while (cursor < last && std::isspace(static_cast<unsigned char>(type[cursor]))) ++cursor;
        while (last > cursor && std::isspace(static_cast<unsigned char>(type[last - 1]))) --last;
        const std::string token = type.substr(cursor, last - cursor);
        if (token == "B" || token == "N" || token == "D" || token == "∞") {
            dims.push_back(16); // Preserve the existing symbolic-shape convention.
        } else {
            const bool exponent = token.rfind("2^", 0) == 0;
            const char* first = token.data() + (exponent ? 2 : 0);
            const char* finish = token.data() + token.size();
            size_t value = 0;
            const auto result = std::from_chars(first, finish, value);
            if (result.ec != std::errc{} || result.ptr != finish || (!exponent && value == 0)) {
                valid = false; return {};
            }
            // The IR stores a qubit exponent, not the statevector length.
            dims.push_back(value);
        }
        cursor = next;
    }
    return dims;
}
} // namespace cycraft::omega::quantum_syntax
