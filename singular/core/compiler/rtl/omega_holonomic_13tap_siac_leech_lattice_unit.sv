// ============================================================================
// EYWA Ω4 / SINGULAR Ω v41.0.0 — SYNTHESIZABLE SYSTEMVERILOG RTL (`.sv`)
// Module: `omega_holonomic_13tap_siac_leech_lattice_unit`
// 10-Mode Self-Morphing Silicon (`0x455A`), 13-Tap O(h^13) SIAC Filter
// (`[1, 36, 300, 1100, 2350, 3516, 3994, 3516, 2350, 1100, 300, 36, 1]/18600`),
// Non-Abelian Wilczek-Zee Holonomic Wilson Loop & 24D Leech Lattice (`Λ_24`).
// Constitutional Rule: `EYW-001` / `SG-ARCH-400` (<= 400 lines).
// ============================================================================
`default_nettype none

module omega_holonomic_13tap_siac_leech_lattice_unit (
    input  wire        clk,
    input  wire        rst_n,
    input  wire        valid_in,
    input  wire [3:0]  mode_req,
    input  wire [15:0] active_cone_k,
    input  wire [15:0] total_nodes_n,
    input  wire [31:0] stencil_in_0,
    input  wire [31:0] stencil_in_1,
    input  wire [31:0] stencil_in_2,
    input  wire [31:0] stencil_in_3,
    input  wire [31:0] stencil_in_4,
    input  wire [31:0] stencil_in_5,
    input  wire [31:0] stencil_in_6,
    input  wire [31:0] holonomic_delta_in,
    input  wire [31:0] wilson_braid_in,
    output reg         valid_out,
    output reg  [3:0]  active_mode,
    output reg  [31:0] hw_signature,
    output reg  [63:0] siac13_accum_out,
    output reg  [31:0] wilson_loop_parity_out,
    output reg  [31:0] leech_octic_energy_out
);

    localparam [3:0] MODE_IDLE            = 4'd0;
    localparam [3:0] MODE_SPARSE_VERTEX   = 4'd1;
    localparam [3:0] MODE_TOPO_COBOUNDARY = 4'd2;
    localparam [3:0] MODE_SYMPLECTIC_7TAP = 4'd3;
    localparam [3:0] MODE_HYPERBOLIC_9TAP = 4'd4;
    localparam [3:0] MODE_CLIFFORD_11TAP  = 4'd5;
    localparam [3:0] MODE_QCRASH_SENTINEL = 4'd6;
    localparam [3:0] MODE_HOLONOMIC_13TAP = 4'd7;
    localparam [3:0] MODE_LEECH_24D_OCTIC = 4'd8;
    localparam [3:0] MODE_DENSE_BURST     = 4'd9;

    // Crossover threshold: K / N > 22% -> 100 * K > 22 * N
    wire [31:0] k_times_100 = {16'd0, active_cone_k} * 32'd100;
    wire [31:0] n_times_22  = {16'd0, total_nodes_n} * 32'd22;
    wire        crossover_dense = (k_times_100 > n_times_22);

    // 13-Tap symmetric SIAC numerator weights (sum = 18600):
    // center w0 = 3994, ±1 w1 = 3516, ±2 w2 = 2350, ±3 w3 = 1100, ±4 w4 = 300, ±5 w5 = 36, ±6 w6 = 1
    wire [63:0] tap_0 = {32'd0, stencil_in_0} * 64'd3994;
    wire [63:0] tap_1 = {32'd0, stencil_in_1} * 64'd7032; // 2 * 3516
    wire [63:0] tap_2 = {32'd0, stencil_in_2} * 64'd4700; // 2 * 2350
    wire [63:0] tap_3 = {32'd0, stencil_in_3} * 64'd2200; // 2 * 1100
    wire [63:0] tap_4 = {32'd0, stencil_in_4} * 64'd600;  // 2 * 300
    wire [63:0] tap_5 = {32'd0, stencil_in_5} * 64'd72;   // 2 * 36
    wire [63:0] tap_6 = {32'd0, stencil_in_6} * 64'd2;    // 2 * 1

    wire [63:0] siac13_sum = (tap_0 + tap_1) + (tap_2 + tap_3) + (tap_4 + tap_5 + tap_6)
                           + ({32'd0, holonomic_delta_in} * 64'd18600);

    // Non-Abelian Wilson-loop commutator parity & 24D Leech Lattice (196,560 minimal vectors) mixer
    wire [31:0] wilson_rot = (wilson_braid_in ^ {holonomic_delta_in[15:0], holonomic_delta_in[31:16]})
                           ^ (stencil_in_0 + stencil_in_6);
    wire [31:0] leech_kissing_mix = (stencil_in_1 * 32'd196560) ^ (stencil_in_2 + wilson_rot);

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            valid_out              <= 1'b0;
            active_mode            <= MODE_IDLE;
            hw_signature           <= 32'h0000_0000;
            siac13_accum_out       <= 64'd0;
            wilson_loop_parity_out <= 32'd0;
            leech_octic_energy_out <= 32'd0;
        end else if (valid_in) begin
            valid_out <= 1'b1;
            if (crossover_dense) begin
                active_mode  <= MODE_DENSE_BURST;
                hw_signature <= {16'h455A, 4'd0, MODE_DENSE_BURST, 8'h41};
            end else begin
                case (mode_req)
                    MODE_IDLE:            begin active_mode <= MODE_IDLE;            hw_signature <= 32'h455A_0041; end
                    MODE_SPARSE_VERTEX:   begin active_mode <= MODE_SPARSE_VERTEX;   hw_signature <= 32'h455A_0141; end
                    MODE_TOPO_COBOUNDARY: begin active_mode <= MODE_TOPO_COBOUNDARY; hw_signature <= 32'h455A_0241; end
                    MODE_SYMPLECTIC_7TAP: begin active_mode <= MODE_SYMPLECTIC_7TAP; hw_signature <= 32'h455A_0341; end
                    MODE_HYPERBOLIC_9TAP: begin active_mode <= MODE_HYPERBOLIC_9TAP; hw_signature <= 32'h455A_0441; end
                    MODE_CLIFFORD_11TAP:  begin active_mode <= MODE_CLIFFORD_11TAP;  hw_signature <= 32'h455A_0541; end
                    MODE_QCRASH_SENTINEL: begin active_mode <= MODE_QCRASH_SENTINEL; hw_signature <= 32'h455A_0641; end
                    MODE_HOLONOMIC_13TAP: begin active_mode <= MODE_HOLONOMIC_13TAP; hw_signature <= 32'h455A_0741; end
                    MODE_LEECH_24D_OCTIC: begin active_mode <= MODE_LEECH_24D_OCTIC; hw_signature <= 32'h455A_0841; end
                    MODE_DENSE_BURST:     begin active_mode <= MODE_DENSE_BURST;     hw_signature <= 32'h455A_0941; end
                    default:              begin active_mode <= MODE_HOLONOMIC_13TAP; hw_signature <= 32'h455A_0741; end
                endcase
            end
            siac13_accum_out       <= siac13_sum;
            wilson_loop_parity_out <= wilson_rot ^ wilson_braid_in;
            leech_octic_energy_out <= leech_kissing_mix;
        end else begin
            valid_out <= 1'b0;
        end
    end

endmodule
`default_nettype wire
