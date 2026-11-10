#pragma once
// SINGULAR Ω v24 — `.sga` Type & Effect System (`Ω.MEM[1024]`, `VersionedSlot`, `Capability`)
#include <cstdint>

namespace singular::v24::language {

enum class SgaValueKind : uint8_t {
    SCALAR_F64,
    STATE_SLOT_ID,
    VERSION_COUNTER,
    CONE_BITMASK64,
    VALIDITY_CERT,
    CAPABILITY_TOKEN
};

} // namespace singular::v24::language
