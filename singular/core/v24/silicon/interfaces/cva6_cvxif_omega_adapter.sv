// SINGULAR Ω v24 — CVA6 CORE-V-XIF Coprocessor Adapter for `omega_dirty_unit`
module cva6_cvxif_omega_adapter (
    input  logic        clk_i,
    input  logic        rst_ni,
    input  logic        x_issue_valid_i,
    output logic        x_issue_ready_o,
    input  logic [31:0] x_issue_instr_i,
    input  logic [63:0] x_issue_rs1_i,
    input  logic [63:0] x_issue_rs2_i,
    input  logic [15:0] x_issue_cap_i,
    output logic        x_result_valid_o,
    output logic [4:0]  x_result_rd_o,
    output logic [63:0] x_result_data_o,
    output logic        x_result_exc_o
);
    omega_dirty_unit u_omega_dirty_unit (
        .clk_i            (clk_i),
        .rst_ni           (rst_ni),
        .x_issue_valid_i  (x_issue_valid_i),
        .x_issue_ready_o  (x_issue_ready_o),
        .x_issue_instr_i  (x_issue_instr_i),
        .x_issue_rs1_i    (x_issue_rs1_i),
        .x_issue_rs2_i    (x_issue_rs2_i),
        .x_issue_cap_i    (x_issue_cap_i),
        .x_result_valid_o (x_result_valid_o),
        .x_result_rd_o    (x_result_rd_o),
        .x_result_data_o  (x_result_data_o),
        .x_result_exc_o   (x_result_exc_o)
    );
endmodule
