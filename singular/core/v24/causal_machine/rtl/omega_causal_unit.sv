// SINGULAR Ω v24 — Xomegacausal execution unit with private state table.
// Coprocessor-style interface (modelled on CV-X-IF issue/result handshakes, simplified):
//   * issue: in IDLE the unit is ready; same-cycle response issue_accept / issue_illegal.
//   * track/dirty/valid/clean update the table on the accepting clock edge and respond next cycle.
//   * prop runs a multi-cycle closure: reach <- reach | OR{deps[j] : reach[j]} once per clock until
//     a fixed point (at most N iterations), then dirties + version-bumps the reached clean entries.
//   * result is held until result_ready. Instructions are atomic: rejected or failing
//     instructions leave the table unchanged.
// Reset is synchronous and active low (rst_n sampled on the rising clock edge).
// RESET_TABLE = 0 is used only by the formal harness so the solver starts from arbitrary tables.
module omega_causal_unit #(
    parameter int unsigned N = omega_isa_pkg::OMEGA_ENTRIES,
    parameter bit RESET_TABLE = 1'b1
) (
    input  logic        clk,
    input  logic        rst_n,
    input  logic        issue_valid,
    output logic        issue_ready,
    input  logic [31:0] issue_insn,
    input  logic [63:0] issue_rs1,
    input  logic [63:0] issue_rs2,
    input  logic [1:0]  issue_priv,
    input  logic [1:0]  momegaen,
    output logic        issue_accept,
    output logic        issue_illegal,
    output logic        result_valid,
    input  logic        result_ready,
    output logic [4:0]  result_rd,
    output logic [63:0] result_data,
    output logic [N*32-1:0] version_q,
    output logic [N-1:0]    dirty_q,
    output logic [N*N-1:0]  deps_q
);
    localparam logic [63:0] ERR = 64'h8000_0000_0000_0000;
    typedef enum logic [1:0] { S_IDLE = 2'd0, S_PROP = 2'd1, S_RESP = 2'd2 } state_t;

    state_t      st_q;
    logic [N-1:0] reach_q;
    logic [$clog2(N)-1:0] src_q;
    logic [4:0]  rd_q;
    logic [63:0] res_q;

    logic ours, legal, permit;
    logic [2:0] f3;
    logic [4:0] rd;
    omega_causal_decoder u_dec (
        .insn(issue_insn), .priv(issue_priv), .momegaen(momegaen),
        .ours(ours), .legal(legal), .permit(permit), .funct3(f3), .rd(rd)
    );

    assign issue_ready   = (st_q == S_IDLE);
    assign issue_accept  = issue_valid && issue_ready && legal && permit;
    assign issue_illegal = issue_valid && issue_ready && ours && !(legal && permit);
    assign result_valid  = (st_q == S_RESP);
    assign result_rd     = rd_q;
    assign result_data   = res_q;

    // operand views
    logic        a_ok, b_ok;
    logic [$clog2(N)-1:0] ai, bi, si;
    assign a_ok = (issue_rs1 < 64'(N));
    assign b_ok = (issue_rs2 < 64'(N));
    assign ai   = issue_rs1[$clog2(N)-1:0];
    assign bi   = issue_rs2[$clog2(N)-1:0];
    assign si   = src_q;

    logic [31:0] ver_a;
    logic [N-1:0] deps_a;
    assign ver_a  = version_q[ai*32 +: 32];
    assign deps_a = deps_q[ai*N +: N];

    // closure step
    logic [N-1:0] reach_next;
    always_comb begin
        reach_next = reach_q;
        for (int j = 0; j < N; j++)
            if (reach_q[j]) reach_next = reach_next | deps_q[j*N +: N];
    end

    always_ff @(posedge clk) begin  // synchronous active-low reset
        if (!rst_n) begin
            st_q    <= S_IDLE;
            reach_q <= '0;
            src_q   <= '0;
            rd_q    <= '0;
            res_q   <= '0;
            if (RESET_TABLE) begin
                version_q <= '0;
                dirty_q   <= '0;
                deps_q    <= '0;
            end
        end else begin
            case (st_q)
                S_IDLE: if (issue_accept) begin
                    rd_q <= rd;
                    st_q <= S_RESP;
                    case (f3)
                        omega_isa_pkg::OMEGA_F3_TRACK: begin
                            if (!a_ok || !b_ok) res_q <= ERR | 64'(omega_isa_pkg::OMEGA_ERR_INVALID_ID);
                            else if (ai == bi)  res_q <= ERR | 64'(omega_isa_pkg::OMEGA_ERR_SELF_DEP);
                            else begin
                                deps_q[32'(ai)*N + 32'(bi)] <= 1'b1;
                                res_q <= 64'(deps_a | (N'(1) << bi));
                            end
                        end
                        omega_isa_pkg::OMEGA_F3_DIRTY: begin
                            if (!a_ok)                          res_q <= ERR | 64'(omega_isa_pkg::OMEGA_ERR_INVALID_ID);
                            else if (issue_rs2 != {32'd0, ver_a}) res_q <= ERR | 64'(omega_isa_pkg::OMEGA_ERR_VERSION_MISMATCH);
                            else begin
                                version_q[ai*32 +: 32] <= ver_a + 32'd1;
                                dirty_q[ai] <= 1'b1;
                                res_q <= {32'd0, ver_a + 32'd1};
                            end
                        end
                        omega_isa_pkg::OMEGA_F3_PROP: begin
                            if (!a_ok)              res_q <= ERR | 64'(omega_isa_pkg::OMEGA_ERR_INVALID_ID);
                            else if (!dirty_q[ai])  res_q <= 64'd0;
                            else begin
                                reach_q <= deps_a;
                                src_q   <= ai;
                                st_q    <= S_PROP;
                            end
                        end
                        omega_isa_pkg::OMEGA_F3_VALID: begin
                            if (!a_ok) res_q <= ERR | 64'(omega_isa_pkg::OMEGA_ERR_INVALID_ID);
                            else       res_q <= (!dirty_q[ai] && issue_rs2 == {32'd0, ver_a}) ? 64'd1 : 64'd0;
                        end
                        default: begin  // OMEGA_F3_CLEAN (decoder guarantees a defined funct3)
                            if (!a_ok) res_q <= ERR | 64'(omega_isa_pkg::OMEGA_ERR_INVALID_ID);
                            else begin
                                dirty_q[ai] <= 1'b0;
                                res_q <= {32'd0, ver_a};
                            end
                        end
                    endcase
                end
                S_PROP: begin
                    if (reach_next == reach_q) begin
                        for (int j = 0; j < N; j++) begin
                            if (reach_q[j] && !dirty_q[j]) begin
                                dirty_q[j] <= 1'b1;
                                version_q[j*32 +: 32] <= version_q[j*32 +: 32] + 32'd1;
                            end
                        end
                        res_q <= 64'(reach_q & ~(N'(1) << si));
                        st_q  <= S_RESP;
                    end else begin
                        reach_q <= reach_next;
                    end
                end
                default: if (result_ready) st_q <= S_IDLE;  // S_RESP
            endcase
        end
    end
endmodule
