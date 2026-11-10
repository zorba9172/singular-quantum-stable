/* GENERATED from omega_isa.yaml by omega_isagen. Do not edit. */
#ifndef OMEGA_ISA_GEN_H
#define OMEGA_ISA_GEN_H
#include <stdint.h>
#define OMEGA_ISA_NAME "Xomegacausal"
#define OMEGA_ISA_VERSION "24.0.0"
#define OMEGA_OPCODE 0x0000007Bu
#define OMEGA_FUNCT7 0x00000001u
#define OMEGA_ENTRIES 64u
#define OMEGA_VERSION_BITS 32u
#define OMEGA_CSR_MOMEGAEN 0x000007C0u
#define OMEGA_CSR_S_BIT 0u
#define OMEGA_CSR_U_BIT 1u
#define OMEGA_ERR_FLAG (1ull << 63)
#define OMEGA_ERR_INVALID_ID 1u
#define OMEGA_ERR_VERSION_MISMATCH 2u
#define OMEGA_ERR_SELF_DEP 3u
#define OMEGA_F3_TRACK 0u
#define MATCH_OMEGA_TRACK 0x0200007Bu
#define MASK_OMEGA_TRACK 0xFE00707Fu
#define OMEGA_F3_DIRTY 1u
#define MATCH_OMEGA_DIRTY 0x0200107Bu
#define MASK_OMEGA_DIRTY 0xFE00707Fu
#define OMEGA_F3_PROP 2u
#define MATCH_OMEGA_PROP 0x0200207Bu
#define MASK_OMEGA_PROP 0xFFF0707Fu
#define OMEGA_F3_VALID 3u
#define MATCH_OMEGA_VALID 0x0200307Bu
#define MASK_OMEGA_VALID 0xFE00707Fu
#define OMEGA_F3_CLEAN 4u
#define MATCH_OMEGA_CLEAN 0x0200407Bu
#define MASK_OMEGA_CLEAN 0xFFF0707Fu
#define OMEGA_INSN_COUNT 5
typedef struct { const char* mnemonic; const char* symbol; uint32_t funct3; uint32_t match; uint32_t mask; uint32_t rs2_zero; } omega_insn_desc_t;
static const omega_insn_desc_t OMEGA_INSNS[OMEGA_INSN_COUNT] = {
  {"omega.track", "⊙", 0u, 0x0200007Bu, 0xFE00707Fu, 0u},
  {"omega.dirty", "Δ⁺", 1u, 0x0200107Bu, 0xFE00707Fu, 0u},
  {"omega.prop", "⇝", 2u, 0x0200207Bu, 0xFFF0707Fu, 1u},
  {"omega.valid", "✓", 3u, 0x0200307Bu, 0xFE00707Fu, 0u},
  {"omega.clean", "⟲", 4u, 0x0200407Bu, 0xFFF0707Fu, 1u},
};
#endif
