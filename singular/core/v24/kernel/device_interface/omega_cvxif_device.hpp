#pragma once
// SINGULAR Ω v24 — CVA6 CORE-V-XIF Coprocessor Device Interface & MMIO/CSR Dispatch
#include <cstdint>

namespace singular::v24::kernel {

struct CvxifIssueReq {
    bool valid = false;
    uint32_t instr = 0;
    uint64_t rs1_val = 0, rs2_val = 0;
    uint16_t caller_cap = 0x0001;
};

struct CvxifResultResp {
    bool valid = false;
    uint8_t rd = 0;
    uint64_t data = 0;
    bool exc = false;
};

} // namespace singular::v24::kernel
