// SINGULAR OMEGA v18.0 — Synthesizable RISC-V custom-3 (0x7B) Instruction Decoder
module omega_riscv_custom3_decoder (
    input  logic [31:0] insn_i, output logic valid_omega_o, output logic [2:0] funct3_o,
    output logic [4:0]  rd_o, rs1_o, rs2_o, output logic [6:0] funct7_o
);
    assign {funct7_o, rs2_o, rs1_o, funct3_o, rd_o} = insn_i[31:7];
    assign valid_omega_o = (insn_i[6:0] == 7'b1111011) && (insn_i[14:12] <= 3'b101);
endmodule
