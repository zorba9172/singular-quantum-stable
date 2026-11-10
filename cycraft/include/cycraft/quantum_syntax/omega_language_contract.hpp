#pragma once
#include <string>
namespace cycraft::omega::quantum_syntax {
inline bool valid_canonical_layer(const std::string& layer) {
    for (const char* v : {"contracts", "shared", "plugins", "domain", "application", "api", "tests", "core", "modules", "discoveries", "verification", "workspace"})
        if (layer == v) return true;
    return false;
}
}
