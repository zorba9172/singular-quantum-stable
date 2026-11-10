// SINGULAR Ω v24 — Formal SystemVerilog Properties (`omega_formal_properties.sv`)
module omega_formal_properties (
    input logic        clk_i,
    input logic        rst_ni,
    input logic        issue_valid_i,
    input logic [31:0] issue_instr_i,
    input logic [63:0] issue_rs1_i,
    input logic [63:0] result_data_i,
    input logic        result_exc_i
);
`ifdef FORMAL
    // Property 1: Out-of-bounds state ID (rs1 >= 1024) on valid custom-3 returns ERR_INVALID_STATE_ID (1)
    always_ff @(posedge clk_i) begin
        if (rst_ni && issue_valid_i && (issue_instr_i[6:0] == 7'h7B) &&
            (issue_instr_i[31:25] == 7'h01) && (issue_instr_i[14:12] <= 3'd3) && (issue_rs1_i >= 64'd1024)) begin
            assert(result_data_i == 64'd1);
        end
    end
    // Property 2: Illegal funct3 (> 3) raises exception
    always_ff @(posedge clk_i) begin
        if (rst_ni && issue_valid_i && (issue_instr_i[6:0] == 7'h7B) && (issue_instr_i[14:12] > 3'd3)) begin
            assert(result_exc_i == 1'b1);
        end
    end
`endif
endmodule
