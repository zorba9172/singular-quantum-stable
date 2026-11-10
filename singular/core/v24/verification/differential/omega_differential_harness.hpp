#pragma once
// SINGULAR Ω v24 — 3-Way Differential Harness (`Reference == QBE == Spike`)
#include "../../machine/interpreter/omega_interpreter.hpp"

namespace singular::v24::verification {

inline bool run_differential_check() noexcept {
    singular::v24::machine::ReferenceMachineInterpreter ref{};
    ref.x[11] = 42; ref.x[12] = 7;
    ref.step_word(0x02c5857bu); // omega.track x10, x11, x12
    ref.x[12] = 19;
    ref.step_word(0x02c5857bu); // omega.track x10, x11, x12
    ref.x[12] = 0;
    ref.step_word(0x02c5957bu); // omega.dirty x10, x11, x12
    ref.step_word(0x02c5a57bu); // omega.prop  x10, x11, x12
    return ref.x[10] == ((1ULL << 42) | (1ULL << 19) | (1ULL << 7));
}

} // namespace singular::v24::verification
