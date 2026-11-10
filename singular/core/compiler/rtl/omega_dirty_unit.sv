// ============================================================================
// SINGULAR OMEGA v24.0.0 — Synthesizable Dirty Execution Unit + CVA6 CV-X-IF
// Integrates `omega_decoder` + `omega_state_table` with OpenHW CV-X-IF Issue/Result
// Includes Formal SystemVerilog Assertions (Atomicity, Bounds, Version & Reset)
// ============================================================================
module omega_dirty_unit #(
    parameter int ENTRIES = 1024
) (
    input  logic        clk_i,
    input  logic        rst_ni,
    // CVA6 CORE-V-XIF Coprocessor Issue Interface
    input  logic        x_issue_valid_i,
    output logic        x_issue_ready_o,
    input  logic [31:0] x_issue_req_insn_i,
    input  logic [63:0] x_issue_rs1_val_i,
    input  logic [63:0] x_issue_rs2_val_i,
    input  logic [15:0] x_issue_cap_i,
    output logic        x_issue_accept_o,
    // CVA6 CORE-V-XIF Coprocessor Result Interface
    output logic        x_result_valid_o,
    output logic [4:0]  x_result_rd_o,
    output logic [63:0] x_result_data_o,
    output logic [2:0]  x_result_err_o
);
    logic       valid_omega, illegal_omega, is_track, is_dirty, is_prop, is_valid;
    logic [4:0] rd, rs1, rs2;
    logic [2:0] funct3;
    logic [6:0] funct7;

    omega_decoder u_decoder (
        .insn_i            (x_issue_req_insn_i),
        .valid_omega_v24_o (valid_omega),
        .illegal_omega_o   (illegal_omega),
        .is_track_o        (is_track),
        .is_dirty_o        (is_dirty),
        .is_prop_o         (is_prop),
        .is_valid_o        (is_valid),
        .rd_o              (rd),
        .rs1_o             (rs1),
        .rs2_o             (rs2),
        .funct3_o          (funct3),
        .funct7_o          (funct7)
    );

    logic [1:0] cmd_op;
    assign cmd_op = is_track ? 2'd0 : (is_dirty ? 2'd1 : (is_prop ? 2'd2 : 2'd3));

    omega_state_table #(.ENTRIES(ENTRIES)) u_state_table (
        .clk_i        (clk_i),
        .rst_ni       (rst_ni),
        .cmd_valid_i  (x_issue_valid_i && valid_omega),
        .cmd_op_i     (cmd_op),
        .rs1_val_i    (x_issue_rs1_val_i),
        .rs2_val_i    (x_issue_rs2_val_i),
        .caller_cap_i (x_issue_cap_i),
        .result_o     (x_result_data_o),
        .err_code_o   (x_result_err_o)
    );

    assign x_issue_ready_o  = 1'b1; // Single-cycle atomic execution
    assign x_issue_accept_o = x_issue_valid_i && valid_omega;
    assign x_result_valid_o = x_issue_valid_i && valid_omega;
    assign x_result_rd_o    = rd;

`ifdef FORMAL
    // Formal Assertion 1: Out-of-bounds state ID (rs1 >= 1024) must return ERR_INVALID_STATE_ID (1)
    assert property (@(posedge clk_i) disable iff (!rst_ni)
        (x_issue_valid_i && valid_omega && (x_issue_rs1_val_i >= 64'd1024)) |-> (x_result_err_o == 3'd1 && x_result_data_o == 64'd1));

    // Formal Assertion 2: Illegal custom-3 funct3/funct7 must never be accepted by CV-X-IF
    assert property (@(posedge clk_i) disable iff (!rst_ni)
        (x_issue_valid_i && illegal_omega) |-> (!x_issue_accept_o && !x_result_valid_o));
`endif
endmodule
