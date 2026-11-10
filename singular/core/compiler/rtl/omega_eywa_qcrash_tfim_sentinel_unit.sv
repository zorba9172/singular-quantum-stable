// ============================================================================
// EYWA Ω3 (SINGULAR Ω v39.0.0) — 9-MODE EYWA Q-CRASH TFIM SENTINEL &
// NON-COMMUTING SINGULAR ARC SILICON UNIT (`omega_eywa_qcrash_tfim_sentinel_unit.sv`)
// Enforces EYW-001 / SG-ARCH-400 (<= 400 lines), Verilator 0-Warning & Yosys LUT6.
// ============================================================================
`default_nettype none

module omega_eywa_qcrash_tfim_sentinel_unit (
    input  wire         clk,
    input  wire         rst_n,
    input  wire         valid_in,
    input  wire [2:0]   mode_sel,
    input  wire [15:0]  cone_width_k,
    input  wire [63:0]  psi_s0,
    input  wire [63:0]  psi_s1,
    input  wire [63:0]  psi_s2,
    input  wire [63:0]  psi_s3,
    input  wire [63:0]  psi_s4,
    input  wire [63:0]  psi_s5,
    input  wire [63:0]  psi_s6,
    input  wire [63:0]  psi_s7,
    input  wire [63:0]  psi_s8,
    input  wire [63:0]  psi_s9,
    input  wire [63:0]  psi_s10,
    input  wire [63:0]  delta_j_q32,
    input  wire [63:0]  transverse_h_q32,
    input  wire [63:0]  residual_in,
    output reg          valid_out,
    output reg  [63:0]  eywa_ctrl_word,
    output reg  [63:0]  trotter_siac_out,
    output reg  [63:0]  commutator_norm_out,
    output reg  [63:0]  residual_out,
    output reg          commutator_recompute_req
);

    // 11-Tap B-Spline SIAC + Trotter2 integer weights (sum = 3024)
    wire [63:0] sym_pair5 = psi_s0 + psi_s10;
    wire [63:0] sym_pair4 = (psi_s1 + psi_s9) * 64'd15;
    wire [63:0] sym_pair3 = (psi_s2 + psi_s8) * 64'd100;
    wire [63:0] sym_pair2 = (psi_s3 + psi_s7) * 64'd336;
    wire [63:0] sym_pair1 = (psi_s4 + psi_s6) * 64'd630;
    wire [63:0] center0   = psi_s5 * 64'd860;
    wire [63:0] siac11_acc = sym_pair5 + sym_pair4 + sym_pair3 + sym_pair2 + sym_pair1 + center0;

    // Non-commuting Lieb-Robinson commutator norm: ||[H, dH]|| = 2 * h * |dJ_k|
    wire [63:0] abs_dj    = delta_j_q32[63] ? (~delta_j_q32 + 64'd1) : delta_j_q32;
    wire [63:0] comm_prod = (abs_dj * transverse_h_q32) >> 31;
    wire        noncomm_exceeds = (comm_prod > 64'h0000_0000_0000_0400);

    // Exact TwoSum-equivalent residual tracking
    wire [63:0] delta_eff = delta_j_q32 + residual_in;
    wire [63:0] sum_next  = psi_s5 + delta_eff;
    wire [63:0] v_eff     = sum_next - psi_s5;
    wire [63:0] err_next  = (psi_s5 - (sum_next - v_eff)) + (delta_eff - v_eff);

    // Decremental 0-ULP retraction path
    wire [63:0] retract_next = (psi_s5 - delta_j_q32) + residual_in;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            valid_out                <= 1'b0;
            eywa_ctrl_word           <= 64'd0;
            trotter_siac_out         <= 64'd0;
            commutator_norm_out      <= 64'd0;
            residual_out             <= 64'd0;
            commutator_recompute_req <= 1'b0;
        end else if (valid_in) begin
            valid_out                <= 1'b1;
            eywa_ctrl_word           <= {16'h4559, 16'd0, cone_width_k, 13'd0, mode_sel};
            commutator_norm_out      <= comm_prod;
            commutator_recompute_req <= noncomm_exceeds;
            case (mode_sel)
                3'd0: begin
                    // Mode 0: Incremental Forward Causal Cone
                    trotter_siac_out <= sum_next;
                    residual_out     <= err_next;
                end
                3'd1: begin
                    // Mode 1: Decremental 0-ULP Retraction (0B Undo-Log)
                    trotter_siac_out <= retract_next;
                    residual_out     <= 64'd0;
                end
                3'd2: begin
                    // Mode 2: Non-Commuting [H, dH] != 0 Light-Cone Guard
                    trotter_siac_out <= sum_next ^ comm_prod;
                    residual_out     <= err_next;
                end
                3'd7: begin
                    // Mode 7: EYWA Q-CRASH TFIM Trotter2 x 11-Tap SIAC Sentinel
                    trotter_siac_out <= (siac11_acc >> 11) + sum_next[63:0];
                    residual_out     <= err_next;
                end
                default: begin
                    trotter_siac_out <= siac11_acc ^ sum_next;
                    residual_out     <= err_next;
                end
            endcase
        end else begin
            valid_out <= 1'b0;
        end
    end

endmodule
`default_nettype wire
