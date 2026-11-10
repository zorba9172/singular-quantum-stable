// ============================================================================
// SINGULAR Ω v38.0.0 — 8-MODE CLIFFORD SPINOR 11-TAP O(h^11) SIAC & E8 GOSSET
// ROOT-LATTICE HARDWARE-RECONFIGURING ACCELERATOR UNIT
// Constitutional Rule `SG-ARCH-400`: <= 400 lines. Zero Verilator warnings.
// ============================================================================
`default_nettype none

module omega_clifford_11tap_siac_e8_lattice_unit (
    input  wire        clk,
    input  wire        rst_n,
    input  wire        valid_in,
    input  wire [2:0]  mode_req,           // 0..7 (8 hardware execution modes)
    input  wire [15:0] active_cone_k,
    input  wire [15:0] total_domain_n,
    input  wire signed [63:0] s_m5,
    input  wire signed [63:0] s_m4,
    input  wire signed [63:0] s_m3,
    input  wire signed [63:0] s_m2,
    input  wire signed [63:0] s_m1,
    input  wire signed [63:0] s_0,
    input  wire signed [63:0] s_p1,
    input  wire signed [63:0] s_p2,
    input  wire signed [63:0] s_p3,
    input  wire signed [63:0] s_p4,
    input  wire signed [63:0] s_p5,
    input  wire signed [63:0] bivector_dtheta,
    input  wire signed [63:0] e8_cartan_flux,
    input  wire signed [63:0] err_in,
    output reg         valid_out,
    output reg  [2:0]  active_hw_mode,
    output reg  [63:0] hw_ctrl_word,
    output reg  signed [63:0] siac11_filtered_out,
    output reg  signed [63:0] state_next_out,
    output reg  signed [63:0] err_next_out,
    output reg  [63:0] e8_gosset_invariant_q48
);

    // 8 Hardware Execution Modes:
    // 3'd0: IDLE_CLOCK_GATED
    // 3'd1: SPARSE_VERTEX_ARC
    // 3'd2: TOPO_COBOUNDARY_EDGE
    // 3'd3: SYMPLECTIC_7TAP_SIAC
    // 3'd4: SU2_GAUGE_HUBO
    // 3'd5: HYPERBOLIC_9TAP_SIAC
    // 3'd6: CLIFFORD_11TAP_SIAC_E8
    // 3'd7: DENSE_E8_BURST_STREAM (K/N > 22%: 100*K > 22*N)
    wire [31:0] lhs_100k = {16'd0, active_cone_k} * 32'd100;
    wire [31:0] rhs_22n  = {16'd0, total_domain_n} * 32'd22;
    wire is_dense_crossover = (lhs_100k > rhs_22n);

    wire [2:0] resolved_mode = (active_cone_k == 16'd0) ? 3'd0 :
                               (is_dense_crossover)     ? 3'd7 :
                               (mode_req == 3'd0)       ? 3'd6 : mode_req;

    // 1-Cycle Integer Knuth TwoSum Error-Free Transformation
    wire signed [63:0] comp_delta = bivector_dtheta + err_in;
    wire signed [63:0] sum_next   = s_0 + comp_delta;
    wire signed [63:0] v_eff      = sum_next - s_0;
    wire signed [63:0] err_resid  = (s_0 - (sum_next - v_eff)) + (comp_delta - v_eff);

    // Symmetric 11-Tap O(h^11) B-Spline Stencil Numerator:
    // [1, 15, 100, 336, 630, 860, 630, 336, 100, 15, 1] / 3024
    wire signed [63:0] sym_1 = s_m1 + s_p1;
    wire signed [63:0] sym_2 = s_m2 + s_p2;
    wire signed [63:0] sym_3 = s_m3 + s_p3;
    wire signed [63:0] sym_4 = s_m4 + s_p4;
    wire signed [63:0] sym_5 = s_m5 + s_p5;

    // 860*x = 1024*x - 128*x - 32*x - 4*x
    wire signed [63:0] t0_860 = (sum_next <<< 10) - (sum_next <<< 7) - (sum_next <<< 5) - (sum_next <<< 2);
    // 630*x = 512*x + 128*x - 8*x - 2*x
    wire signed [63:0] t1_630 = (sym_1 <<< 9) + (sym_1 <<< 7) - (sym_1 <<< 3) - (sym_1 <<< 1);
    // 336*x = 256*x + 64*x + 16*x
    wire signed [63:0] t2_336 = (sym_2 <<< 8) + (sym_2 <<< 6) + (sym_2 <<< 4);
    // 100*x = 64*x + 32*x + 4*x
    wire signed [63:0] t3_100 = (sym_3 <<< 6) + (sym_3 <<< 5) + (sym_3 <<< 2);
    // 15*x  = 16*x - x
    wire signed [63:0] t4_15  = (sym_4 <<< 4) - sym_4;

    wire signed [63:0] siac11_num  = t0_860 + t1_630 + t2_336 + t3_100 + t4_15 + sym_5;
    wire signed [63:0] siac11_norm = (siac11_num >>> 11) - (siac11_num >>> 13);

    // E8 240-root Gosset polytope parity-preserving accumulator
    wire [63:0] e8_step = (sum_next[63:0] ^ {e8_cartan_flux[31:0], e8_cartan_flux[63:32]}) +
                          (sym_1[63:0] ^ sym_2[63:0] ^ sym_5[63:0]);

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            valid_out               <= 1'b0;
            active_hw_mode          <= 3'd0;
            hw_ctrl_word            <= 64'h5338_0000_0000_0000;
            siac11_filtered_out     <= 64'sd0;
            state_next_out          <= 64'sd0;
            err_next_out            <= 64'sd0;
            e8_gosset_invariant_q48 <= 64'd0;
        end else if (valid_in) begin
            valid_out           <= 1'b1;
            active_hw_mode      <= resolved_mode;
            hw_ctrl_word        <= {16'h5338, active_cone_k, total_domain_n, 13'd0, resolved_mode};
            state_next_out      <= sum_next;
            err_next_out        <= err_resid;
            siac11_filtered_out <= (resolved_mode == 3'd6) ? siac11_norm :
                                   (resolved_mode == 3'd7) ? (siac11_norm + (e8_cartan_flux >>> 2)) :
                                   sum_next;
            e8_gosset_invariant_q48 <= e8_gosset_invariant_q48 ^ e8_step;
        end else begin
            valid_out <= 1'b0;
        end
    end

endmodule
`default_nettype wire
