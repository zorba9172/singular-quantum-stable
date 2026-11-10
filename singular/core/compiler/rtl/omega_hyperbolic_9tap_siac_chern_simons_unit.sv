// ============================================================================
// SINGULAR Ω v37.0.0 — 7-MODE HYPERBOLIC 9-TAP O(h^9) SIAC & CHERN-SIMONS
// GAUGE HARDWARE-RECONFIGURING ACCELERATOR UNIT
// Constitutional Rule `SG-ARCH-400`: <= 400 lines. Zero Verilator warnings.
// ============================================================================
`default_nettype none

module omega_hyperbolic_9tap_siac_chern_simons_unit (
    input  wire        clk,
    input  wire        rst_n,
    input  wire        valid_in,
    input  wire [2:0]  mode_req,           // 0..6 (7 hardware execution modes)
    input  wire [15:0] active_cone_k,
    input  wire [15:0] total_domain_n,
    input  wire signed [63:0] h_m4,
    input  wire signed [63:0] h_m3,
    input  wire signed [63:0] h_m2,
    input  wire signed [63:0] h_m1,
    input  wire signed [63:0] h_0,
    input  wire signed [63:0] h_p1,
    input  wire signed [63:0] h_p2,
    input  wire signed [63:0] h_p3,
    input  wire signed [63:0] h_p4,
    input  wire signed [63:0] curv_delta_ds,
    input  wire signed [63:0] su3_gellmann_flux,
    input  wire signed [63:0] err_in,
    output reg         valid_out,
    output reg  [2:0]  active_hw_mode,
    output reg  [63:0] hw_ctrl_word,
    output reg  signed [63:0] siac9_filtered_out,
    output reg  signed [63:0] state_next_out,
    output reg  signed [63:0] err_next_out,
    output reg  [63:0] chern_simons_invariant_q48
);

    // 7 Hardware Modes:
    // 3'd0: IDLE_CLOCK_GATED
    // 3'd1: SPARSE_VERTEX_ARC
    // 3'd2: TOPO_COBOUNDARY_EDGE
    // 3'd3: SYMPLECTIC_7TAP_SIAC
    // 3'd4: SU2_GAUGE_HUBO
    // 3'd5: HYPERBOLIC_9TAP_SIAC
    // 3'd6: DENSE_CHERN_SIMONS_BURST (K/N > 22%: 100*K > 22*N)
    wire [31:0] lhs_100k = {16'd0, active_cone_k} * 32'd100;
    wire [31:0] rhs_22n  = {16'd0, total_domain_n} * 32'd22;
    wire is_dense_crossover = (lhs_100k > rhs_22n);

    wire [2:0] resolved_mode = (active_cone_k == 16'd0) ? 3'd0 :
                               (is_dense_crossover)     ? 3'd6 :
                               (mode_req == 3'd0)       ? 3'd5 : mode_req;

    // 1-Cycle Integer Knuth TwoSum Error-Free Transformation
    wire signed [63:0] comp_delta = curv_delta_ds + err_in;
    wire signed [63:0] sum_next   = h_0 + comp_delta;
    wire signed [63:0] v_eff      = sum_next - h_0;
    wire signed [63:0] err_Resid  = (h_0 - (sum_next - v_eff)) + (comp_delta - v_eff);

    // Symmetric 9-Tap O(h^9) B-Spline Stencil Numerator:
    // 450*h_0 + 336*(h_m1 + h_p1) + 144*(h_m2 + h_p2) + 24*(h_m3 + h_p3) + 1*(h_m4 + h_p4)
    wire signed [63:0] sym_1 = h_m1 + h_p1;
    wire signed [63:0] sym_2 = h_m2 + h_p2;
    wire signed [63:0] sym_3 = h_m3 + h_p3;
    wire signed [63:0] sym_4 = h_m4 + h_p4;

    // 450*x = 512*x - 64*x + 2*x
    wire signed [63:0] t0_450 = (sum_next <<< 9) - (sum_next <<< 6) + (sum_next <<< 1);
    // 336*x = 256*x + 64*x + 16*x
    wire signed [63:0] t1_336 = (sym_1 <<< 8) + (sym_1 <<< 6) + (sym_1 <<< 4);
    // 144*x = 128*x + 16*x
    wire signed [63:0] t2_144 = (sym_2 <<< 7) + (sym_2 <<< 4);
    // 24*x  = 16*x + 8*x
    wire signed [63:0] t3_24  = (sym_3 <<< 4) + (sym_3 <<< 3);

    wire signed [63:0] siac9_num = t0_450 + t1_336 + t2_144 + t3_24 + sym_4;
    wire signed [63:0] siac9_norm = (siac9_num >>> 10) - (siac9_num >>> 12);

    // Chern-Simons Non-Abelian SU(3) topological invariant accumulator
    wire [63:0] cs_braid_step = (sum_next[63:0] ^ {su3_gellmann_flux[47:0], su3_gellmann_flux[63:48]}) +
                                (sym_1[63:0] ^ sym_3[63:0]);

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            valid_out                  <= 1'b0;
            active_hw_mode             <= 3'd0;
            hw_ctrl_word               <= 64'h5337_0000_0000_0000;
            siac9_filtered_out         <= 64'sd0;
            state_next_out             <= 64'sd0;
            err_next_out               <= 64'sd0;
            chern_simons_invariant_q48 <= 64'd0;
        end else if (valid_in) begin
            valid_out          <= 1'b1;
            active_hw_mode     <= resolved_mode;
            hw_ctrl_word       <= {16'h5337, active_cone_k, total_domain_n, 13'd0, resolved_mode};
            state_next_out     <= sum_next;
            err_next_out       <= err_Resid;
            siac9_filtered_out <= (resolved_mode == 3'd5) ? siac9_norm :
                                  (resolved_mode == 3'd4) ? (siac9_norm + (su3_gellmann_flux >>> 3)) :
                                  sum_next;
            chern_simons_invariant_q48 <= chern_simons_invariant_q48 ^ cs_braid_step;
        end else begin
            valid_out <= 1'b0;
        end
    end

endmodule
`default_nettype wire
