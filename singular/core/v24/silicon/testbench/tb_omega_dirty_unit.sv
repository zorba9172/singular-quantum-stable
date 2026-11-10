// SINGULAR Ω v24 — SystemVerilog Self-Checking Testbench for `omega_dirty_unit`
module tb_omega_dirty_unit;
    logic        clk = 0;
    logic        rst_n = 0;
    logic        issue_valid = 0;
    logic        issue_ready;
    logic [31:0] issue_instr = 32'h0;
    logic [63:0] issue_rs1 = 64'd0;
    logic [63:0] issue_rs2 = 64'd0;
    logic [15:0] issue_cap = 16'h0001;
    logic        res_valid;
    logic [4:0]  res_rd;
    logic [63:0] res_data;
    logic        res_exc;

    always #5 clk = ~clk;

    omega_dirty_unit dut (
        .clk_i(clk), .rst_ni(rst_n),
        .x_issue_valid_i(issue_valid), .x_issue_ready_o(issue_ready),
        .x_issue_instr_i(issue_instr), .x_issue_rs1_i(issue_rs1),
        .x_issue_rs2_i(issue_rs2), .x_issue_cap_i(issue_cap),
        .x_result_valid_o(res_valid), .x_result_rd_o(res_rd),
        .x_result_data_o(res_data), .x_result_exc_o(res_exc)
    );

    initial begin
        rst_n = 0; #20; rst_n = 1; #10;
        // 1. omega.track x10, x11(42), x12(7) -> 0x02c5857b
        issue_valid = 1; issue_instr = 32'h02c5857b; issue_rs1 = 64'd42; issue_rs2 = 64'd7; #10;
        assert(res_valid && res_data == 64'd0 && !res_exc);
        // 2. omega.dirty x10, x11(42), x12(0) -> 0x02c5957b
        issue_instr = 32'h02c5957b; issue_rs1 = 64'd42; issue_rs2 = 64'd0; #10;
        assert(res_valid && res_data == 64'd0 && !res_exc);
        // 3. Negative bounds check: rs1 = 2048 >= 1024 -> ERR_INVALID_STATE_ID (1)
        issue_instr = 32'h02c5957b; issue_rs1 = 64'd2048; issue_rs2 = 64'd0; #10;
        assert(res_valid && res_data == 64'd1);
        $finish;
    end
endmodule
