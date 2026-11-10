// SINGULAR Ω v24 — formal harness for omega_causal_unit (SymbiYosys, N = 4).
// Instruction-level functional specification written independently of the RTL:
// at accept time the harness snapshots the table and operands; when the result is
// presented it asserts rd, result value and the complete next table against the spec.
// With RESET_TABLE = 0 the table starts arbitrary, so every check covers all table states.
module omega_causal_formal #(parameter bit RESET_TABLE = 1'b0) (
    input logic        clk,
    input logic        rst_n,
    input logic        issue_valid,
    input logic [31:0] issue_insn,
    input logic [63:0] issue_rs1,
    input logic [63:0] issue_rs2,
    input logic [1:0]  issue_priv,
    input logic [1:0]  momegaen,
    input logic        result_ready
);
    localparam int unsigned N = 4;
    localparam logic [63:0] ERR = 64'h8000_0000_0000_0000;

    logic issue_ready, issue_accept, issue_illegal, result_valid;
    logic [4:0] result_rd;
    logic [63:0] result_data;
    logic [N*32-1:0] ver;
    logic [N-1:0] dirty;
    logic [N*N-1:0] deps;

    omega_causal_unit #(.N(N), .RESET_TABLE(RESET_TABLE)) dut (
        .clk, .rst_n, .issue_valid, .issue_ready, .issue_insn, .issue_rs1, .issue_rs2, .issue_priv, .momegaen,
        .issue_accept, .issue_illegal, .result_valid, .result_ready, .result_rd, .result_data,
        .version_q(ver), .dirty_q(dirty), .deps_q(deps)
    );

`ifdef FORMAL
    // ---- environment: reset asserted in the first cycle only
    logic init = 1'b1;
    always @(posedge clk) init <= 1'b0;
    always @(*) assume(rst_n == !init);

    // ---- independent legality formula
    logic [2:0] f3;
    logic spec_legal, spec_permit, spec_ours;
    assign f3 = issue_insn[14:12];
    assign spec_ours = issue_insn[6:0] == 7'h7B;
    assign spec_legal = spec_ours && issue_insn[31:25] == 7'h01 &&
                        (f3 == 3'd0 || f3 == 3'd1 || f3 == 3'd3 || ((f3 == 3'd2 || f3 == 3'd4) && issue_insn[24:20] == 5'd0));
    assign spec_permit = issue_priv == 2'd3 || (issue_priv == 2'd1 && momegaen[0]) || (issue_priv == 2'd0 && momegaen[1]);

    // ---- snapshot at accept
    logic pend = 1'b0;
    logic [2:0] s_f3; logic [4:0] s_rd; logic [63:0] s_a, s_b;
    logic [N*32-1:0] s_ver; logic [N-1:0] s_dirty; logic [N*N-1:0] s_deps;
    logic [7:0] busy = 8'd0;
    always @(posedge clk) begin
        if (!rst_n) begin
            pend <= 1'b0;
            busy <= 8'd0;
        end else if (issue_accept) begin
            pend <= 1'b1; busy <= 8'd0;
            s_f3 <= f3; s_rd <= issue_insn[11:7]; s_a <= issue_rs1; s_b <= issue_rs2;
            s_ver <= ver; s_dirty <= dirty; s_deps <= deps;
        end else if (pend && !result_valid) begin
            busy <= busy + 8'd1;
        end else if (result_valid && result_ready) begin
            pend <= 1'b0;
        end
    end

    // ---- specification of one instruction applied to the snapshot
    logic [63:0] e_res; logic [N*32-1:0] e_ver; logic [N-1:0] e_dirty; logic [N*N-1:0] e_deps;
    logic [N-1:0] reach;
    logic [1:0] ia, ib;
    assign ia = s_a[1:0];
    assign ib = s_b[1:0];
    always @(*) begin
        e_ver = s_ver; e_dirty = s_dirty; e_deps = s_deps; e_res = 64'd0;
        reach = s_deps[ia*N +: N];
        for (int k = 0; k < N; k++)
            for (int j = 0; j < N; j++)
                if (reach[j]) reach = reach | s_deps[j*N +: N];
        case (s_f3)
            3'd0: if (s_a >= N || s_b >= N) e_res = ERR | 64'd1;
                  else if (s_a == s_b) e_res = ERR | 64'd3;
                  else begin e_deps[ia*N + ib] = 1'b1; e_res = {60'd0, e_deps[ia*N +: N]}; end
            3'd1: if (s_a >= N) e_res = ERR | 64'd1;
                  else if (s_b != {32'd0, s_ver[ia*32 +: 32]}) e_res = ERR | 64'd2;
                  else begin e_ver[ia*32 +: 32] = s_ver[ia*32 +: 32] + 32'd1; e_dirty[ia] = 1'b1; e_res = {32'd0, e_ver[ia*32 +: 32]}; end
            3'd2: if (s_a >= N) e_res = ERR | 64'd1;
                  else if (!s_dirty[ia]) e_res = 64'd0;
                  else begin
                      for (int j = 0; j < N; j++)
                          if (reach[j] && !s_dirty[j]) begin e_dirty[j] = 1'b1; e_ver[j*32 +: 32] = s_ver[j*32 +: 32] + 32'd1; end
                      e_res = {60'd0, reach & ~(4'b1 << ia)};
                  end
            3'd3: if (s_a >= N) e_res = ERR | 64'd1;
                  else e_res = (!s_dirty[ia] && s_b == {32'd0, s_ver[ia*32 +: 32]}) ? 64'd1 : 64'd0;
            default: if (s_a >= N) e_res = ERR | 64'd1;
                  else begin e_dirty[ia] = 1'b0; e_res = {32'd0, s_ver[ia*32 +: 32]}; end
        endcase
    end

    logic first_result, prev_result_valid = 1'b0;
    always @(posedge clk) prev_result_valid <= result_valid;
    assign first_result = pend && result_valid && !prev_result_valid;

    always @(posedge clk) begin
        if (!init && rst_n && !$past(init)) begin
            // P1 decoder: accept iff legal and permitted; illegal iff ours and not (legal and permitted)
            if (issue_valid && issue_ready) begin
                assert(issue_accept == (spec_legal && spec_permit));
                assert(issue_illegal == (spec_ours && !(spec_legal && spec_permit)));
            end
            // P2 no accept/illegal response while busy
            if (!issue_ready) assert(!issue_accept && !issue_illegal);
            // P3 functional correctness of every instruction (result, rd, complete next table)
            if (first_result) begin
                assert(result_data == e_res);
                assert(result_rd == s_rd);
                assert(ver == e_ver && dirty == e_dirty && deps == e_deps);
            end
            // P4 atomicity: no table change while an instruction is in flight
            if (pend && !result_valid && $past(pend)) assert(ver == s_ver && dirty == s_dirty && deps == s_deps);
            // P5 rejected issue or idle cycle leaves the table unchanged
            if ($past(!issue_accept) && !$past(pend)) assert(ver == $past(ver) && dirty == $past(dirty) && deps == $past(deps));
            // P6 bounded latency: closure needs at most N + 1 cycles
            assert(busy <= N + 1);
            // P7 result held stable under back-pressure
            if ($past(result_valid && !result_ready)) assert(result_valid && result_data == $past(result_data) && result_rd == $past(result_rd));
            // P8 structural invariant reachable states keep: never a self-dependency
            if (RESET_TABLE) for (int i = 0; i < N; i++) assert(!deps[i*N + i]);
        end
        // P9 reset clears the table (only meaningful with RESET_TABLE = 1)
        if (RESET_TABLE && !init && $past(init)) assert(ver == '0 && dirty == '0 && deps == '0);
        // Non-vacuity: every instruction kind, an error result, an illegal issue and a
        // two-hop closure must be reachable (checked with the cover task).
        if (!init) begin
            for (int k = 0; k < 5; k++) cover(first_result && s_f3 == k[2:0] && !result_data[63]);
            cover(first_result && result_data[63]);
            cover(issue_illegal);
            cover(first_result && s_f3 == 3'd2 && busy >= 8'd2 && result_data[3:0] != 4'd0);
        end
    end
`endif
endmodule
