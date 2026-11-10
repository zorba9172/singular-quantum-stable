// SINGULAR Ω v24 — synthesis top: omega_causal_unit with only the coprocessor interface
// exposed (the state-table observation ports are left internal) so the design fits FPGA I/O.
module omega_causal_synth_top (
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
    output logic [63:0] result_data
);
    localparam int unsigned N = omega_isa_pkg::OMEGA_ENTRIES;
    /* verilator lint_off UNUSEDSIGNAL */
    logic [N*32-1:0] version_unused;
    logic [N-1:0]    dirty_unused;
    logic [N*N-1:0]  deps_unused;
    /* verilator lint_on UNUSEDSIGNAL */
    omega_causal_unit #(.N(N)) u_unit (
        .clk, .rst_n, .issue_valid, .issue_ready, .issue_insn, .issue_rs1, .issue_rs2, .issue_priv, .momegaen,
        .issue_accept, .issue_illegal, .result_valid, .result_ready, .result_rd, .result_data,
        .version_q(version_unused), .dirty_q(dirty_unused), .deps_q(deps_unused)
    );
endmodule
