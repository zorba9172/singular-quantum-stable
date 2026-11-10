#pragma once
// SINGULAR Ω v24 — QBE SSA Backend Emitter (`singular-qbe` ARM64 & RV64)
#include <sstream>
#include <string>

namespace singular::v24::compiler {

inline std::string emit_qbe_ssa_function(const std::string& fn_name, const std::string& op1, const std::string& op2) {
    std::ostringstream os;
    os << "export function d $" << fn_name << "(d %a, d %b) {\n@start\n"
       << "    %t1 =d " << op1 << " %a, %b\n"
       << "    %t2 =d " << op2 << " %t1, %b\n"
       << "    ret %t2\n}\n";
    return os.str();
}

} // namespace singular::v24::compiler
