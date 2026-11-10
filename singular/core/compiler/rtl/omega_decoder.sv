// ============================================================================
// SINGULAR OMEGA v24.0.0 — Synthesizable Singular Arc Machine Instruction Decoder
// Generated from Single Source of Truth: `omega_isa.yaml`
// Opcode: RISC-V custom-3 (0x7B = 7'b1111011), Format: R-Type (32-bit)
// Instructions (funct7 = 7'h01):
//   funct3 = 3'b000 : omega.track rd, rs1, rs2
//   funct3 = 3'b001 : omega.dirty rd, rs1, rs2
//   funct3 = 3'b010 : omega.prop  rd, rs1, rs2
//   funct3 = 3'b011 : omega.valid rd, rs1, rs2
// ============================================================================
module omega_decoder (
    input  logic [31:0] insn_i,
    output logic        valid_omega_v24_o,
    output logic        illegal_omega_o,
    output logic        is_track_o,
    output logic        is_dirty_o,
    output logic        is_prop_o,
    output logic        is_valid_o,
    output logic [4:0]  rd_o,
    output logic [4:0]  rs1_o,
    output logic [4:0]  rs2_o,
    output logic [2:0]  funct3_o,
    output logic [6:0]  funct7_o
);
    assign opcode_match = (insn_i[6:0] == 7'b1111011);
    assign rd_o         = insn_i[11:7];
    assign funct3_o     = insn_i[14:12];
    assign rs1_o        = insn_i[19:15];
    assign rs2_o        = insn_i[24:20];
    assign funct7_o     = insn_i[31:25];

    logic opcode_match;
    logic valid_f7;
    logic valid_f3;

    assign valid_f7          = (funct7_o == 7'h01) || (funct7_o == 7'h00);
    assign valid_f3          = (funct3_o <= 3'b011);
    assign valid_omega_v24_o = opcode_match && valid_f7 && valid_f3;
    assign illegal_omega_o   = opcode_match && !(valid_f7 && valid_f3);

    assign is_track_o = valid_omega_v24_o && (funct3_o == 3'b000);
    assign is_dirty_o = valid_omega_v24_o && (funct3_o == 3'b001);
    assign is_prop_o  = valid_omega_v24_o && (funct3_o == 3'b010);
    assign is_valid_o = valid_omega_v24_o && (funct3_o == 3'b011);
endmodule
