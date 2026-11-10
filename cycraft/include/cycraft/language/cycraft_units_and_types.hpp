#pragma once
// ============================================================================
// CYCRAFT Ω — The Computational Reality Language (`Code Reality.`)
// Layer: L0/L1 Language Type & Compile-Time Z^7 SI Dimensional Lattice System
// Every physical quantity, tensor, and harmonic field carries an exact
// 7-dimensional integer exponent vector over SI base units:
//   [m, kg, s, A, K, mol, cd] + scale factor relative to coherent SI base.
// ============================================================================

#include <array>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace cycraft::omega::language {

struct SiUnitDimension {
    // [0]=m, [1]=kg, [2]=s, [3]=A, [4]=K, [5]=mol, [6]=cd
    std::array<int8_t, 7> exp = {0, 0, 0, 0, 0, 0, 0};
    double si_scale = 1.0;
    std::string symbol = "1";
    bool is_annotated = false;

    bool same_dimension(const SiUnitDimension& other) const {
        return exp == other.exp;
    }

    bool is_dimensionless() const {
        for (int8_t e : exp) {
            if (e != 0) return false;
        }
        return true;
    }

    static SiUnitDimension dimensionless() {
        return SiUnitDimension{{0, 0, 0, 0, 0, 0, 0}, 1.0, "1", false};
    }

    static SiUnitDimension parse_annotation(const std::string& raw_tag) {
        std::string tag = raw_tag;
        if (!tag.empty() && tag[0] == '@') {
            tag = tag.substr(1);
        }
        if (tag.empty() || tag == "1" || tag == "unitless") {
            return SiUnitDimension{{0, 0, 0, 0, 0, 0, 0}, 1.0, "1", true};
        }
        if (tag == "m") return SiUnitDimension{{1, 0, 0, 0, 0, 0, 0}, 1.0, "m", true};
        if (tag == "mm") return SiUnitDimension{{1, 0, 0, 0, 0, 0, 0}, 1e-3, "mm", true};
        if (tag == "nm") return SiUnitDimension{{1, 0, 0, 0, 0, 0, 0}, 1e-9, "nm", true};
        if (tag == "kg") return SiUnitDimension{{0, 1, 0, 0, 0, 0, 0}, 1.0, "kg", true};
        if (tag == "s") return SiUnitDimension{{0, 0, 1, 0, 0, 0, 0}, 1.0, "s", true};
        if (tag == "ms") return SiUnitDimension{{0, 0, 1, 0, 0, 0, 0}, 1e-3, "ms", true};
        if (tag == "A") return SiUnitDimension{{0, 0, 0, 1, 0, 0, 0}, 1.0, "A", true};
        if (tag == "K") return SiUnitDimension{{0, 0, 0, 0, 1, 0, 0}, 1.0, "K", true};
        if (tag == "mol") return SiUnitDimension{{0, 0, 0, 0, 0, 1, 0}, 1.0, "mol", true};
        if (tag == "cd") return SiUnitDimension{{0, 0, 0, 0, 0, 0, 1}, 1.0, "cd", true};
        // Derived coherent SI units
        if (tag == "Hz") return SiUnitDimension{{0, 0, -1, 0, 0, 0, 0}, 1.0, "Hz", true};
        if (tag == "N") return SiUnitDimension{{1, 1, -2, 0, 0, 0, 0}, 1.0, "N", true};
        if (tag == "Pa" || tag == "MPa") {
            double sc = (tag == "MPa") ? 1e6 : 1.0;
            return SiUnitDimension{{-1, 1, -2, 0, 0, 0, 0}, sc, tag, true};
        }
        if (tag == "Pa_per_K") {
            return SiUnitDimension{{-1, 1, -2, 0, -1, 0, 0}, 1.0, "Pa_per_K", true};
        }
        if (tag == "J") return SiUnitDimension{{2, 1, -2, 0, 0, 0, 0}, 1.0, "J", true};
        if (tag == "W") return SiUnitDimension{{2, 1, -3, 0, 0, 0, 0}, 1.0, "W", true};
        if (tag == "J_per_kg_K") {
            // J / (kg * K) = (m^2 * kg * s^-2) / (kg * K) = m^2 * s^-2 * K^-1
            return SiUnitDimension{{2, 0, -2, 0, -1, 0, 0}, 1.0, "J_per_kg_K", true};
        }
        if (tag == "J_per_K") {
            return SiUnitDimension{{2, 1, -2, 0, -1, 0, 0}, 1.0, "J_per_K", true};
        }
        if (tag == "K_per_s") {
            return SiUnitDimension{{0, 0, -1, 0, 1, 0, 0}, 1.0, "K_per_s", true};
        }
        if (tag == "per_K") {
            return SiUnitDimension{{0, 0, 0, 0, -1, 0, 0}, 1.0, "per_K", true};
        }
        if (tag == "m_per_s") {
            return SiUnitDimension{{1, 0, -1, 0, 0, 0, 0}, 1.0, "m_per_s", true};
        }
        return SiUnitDimension{{0, 0, 0, 0, 0, 0, 0}, 1.0, tag, false};
    }

    static SiUnitDimension multiply(const SiUnitDimension& a, const SiUnitDimension& b) {
        SiUnitDimension out{};
        for (size_t i = 0; i < 7; ++i) {
            out.exp[i] = static_cast<int8_t>(a.exp[i] + b.exp[i]);
        }
        out.si_scale = a.si_scale * b.si_scale;
        out.is_annotated = a.is_annotated || b.is_annotated;
        out.symbol = out.canonical_string();
        return out;
    }

    static SiUnitDimension divide(const SiUnitDimension& a, const SiUnitDimension& b) {
        SiUnitDimension out{};
        for (size_t i = 0; i < 7; ++i) {
            out.exp[i] = static_cast<int8_t>(a.exp[i] - b.exp[i]);
        }
        out.si_scale = (b.si_scale != 0.0) ? (a.si_scale / b.si_scale) : a.si_scale;
        out.is_annotated = a.is_annotated || b.is_annotated;
        out.symbol = out.canonical_string();
        return out;
    }

    std::string canonical_string() const {
        static const char* kNames[7] = {"m", "kg", "s", "A", "K", "mol", "cd"};
        if (exp == std::array<int8_t, 7>{0, 0, 0, 0, 1, 0, 0}) return "@K";
        if (exp == std::array<int8_t, 7>{0, 0, -1, 0, 1, 0, 0}) return "@K_per_s (K*s^-1)";
        if (exp == std::array<int8_t, 7>{2, 1, -3, 0, 0, 0, 0}) return "@W (kg*m^2*s^-3)";
        if (exp == std::array<int8_t, 7>{2, 1, -2, 0, 0, 0, 0}) return "@J (kg*m^2*s^-2)";
        if (exp == std::array<int8_t, 7>{-1, 1, -2, 0, 0, 0, 0}) return "@Pa (kg*m^-1*s^-2)";
        if (exp == std::array<int8_t, 7>{2, 0, -2, 0, -1, 0, 0}) return "@J_per_kg_K (m^2*s^-2*K^-1)";
        if (exp == std::array<int8_t, 7>{2, 1, -2, 0, -1, 0, 0}) return "@J_per_K (kg*m^2*s^-2*K^-1)";
        std::ostringstream oss;
        bool first = true;
        for (size_t i = 0; i < 7; ++i) {
            if (exp[i] == 0) continue;
            if (!first) oss << "*";
            first = false;
            oss << kNames[i];
            if (exp[i] != 1) oss << "^" << static_cast<int>(exp[i]);
        }
        return first ? "@1 (dimensionless)" : ("@" + oss.str());
    }
};

enum class CyDataType : uint8_t {
    F32 = 0,
    F64 = 1,
    I64 = 2,
    BOOL = 3,
    COMPLEX_F64 = 4,
    TENSOR_F64 = 5,
    FIELD_COMPLEX_SPACE3 = 6,
    FIELD_REAL_SPACE3 = 7,
    CUSTOM_FORM = 8
};

enum class CyOwnershipKind : uint8_t {
    OWNED_UNIQUE = 0,
    BORROW_SHARED = 1,
    BORROW_MUT_XOR = 2,
    VERSIONED_STATE = 3
};

struct CyTypeDescriptor {
    CyDataType kind = CyDataType::F64;
    std::string raw_type_str = "f64";
    std::vector<int64_t> shape;
    SiUnitDimension unit = SiUnitDimension::dimensionless();
    CyOwnershipKind ownership = CyOwnershipKind::OWNED_UNIQUE;
    double max_error_bound = 0.0;
};

} // namespace cycraft::omega::language
