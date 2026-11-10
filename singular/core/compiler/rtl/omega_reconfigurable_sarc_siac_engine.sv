// ============================================================================
// SINGULAR Ω v34.0.0 — Synthesizable SystemVerilog RTL:
// Assembly-Governed Reconfigurable Singular-Arc + SIAC + Hodge + ERA Engine
// Supports RISC-V custom-3 (0x7B, funct7=0x02) hardware reconfiguration.
// Enforces Constitutional Rule `SG-ARCH-400` (<= 400 physical lines).
// ============================================================================
`default_nettype none

module omega_reconfigurable_sarc_siac_engine (
    input  wire        clk,
    input  wire        rst_n,
    input  wire [31:0] insn_i,
    input  wire [63:0] cone_mask_i,
    input  wire [15:0] total_nodes_i,
    input  wire        retract_en_i,
    input  wire signed [63:0] delta_q32_i,
    input  wire signed [63:0] s0_i,
    input  wire signed [63:0] s1_i,
    input  wire signed [63:0] s2_i,
    input  wire signed [63:0] s3_i,
    input  wire signed [63:0] s4_i,
    output reg  [1:0]  hw_mode_o,
    output reg  [63:0] hw_ctrl_word_o,
    output reg  signed [63:0] era_state_o,
    output reg  signed [63:0] fused_siac_hodge_o,
    output reg         ai_trigger_fire_o
);
    wire is_custom3_v34 = (insn_i[6:0] == 7'h7B) && (insn_i[31:25] == 7'h02);
    wire [2:0]  funct3  = insn_i[14:12];
    wire [14:0] reg_ids = {insn_i[24:15], insn_i[11:7]};

    // Combinational 64-bit population count of active causal cone
    integer b;
    reg [6:0] active_cone_popcnt;
    always @(*) begin
        active_cone_popcnt = 7'd0;
        for (b = 0; b < 64; b = b + 1) begin
            active_cone_popcnt = active_cone_popcnt + {6'd0, cone_mask_i[b]};
        end
    end

    // Hardware Crossover Law: 100 * |ΔC| > 22 * N -> Mode 3 (DENSE_SIMD_BURST)
    wire [23:0] lhs_100k = {17'd0, active_cone_popcnt} * 24'd100;
    wire [23:0] rhs_22n  = {8'd0, total_nodes_i} * 24'd22;
    wire [1:0]  next_mode = (active_cone_popcnt == 7'd0) ? 2'b00 :
                            (lhs_100k > rhs_22n)         ? 2'b11 :
                            (funct3 == 3'b001)           ? 2'b10 : 2'b01;

    // 1-Pass Fused 5-Tap SIAC Numerator: s0 + 6*s1 + 10*s2 + 6*s3 + s4
    wire signed [63:0] outer_sum = s0_i + s4_i;
    wire signed [63:0] inner_sum = s1_i + s3_i;
    wire signed [63:0] siac_num  = outer_sum + (inner_sum * 64'sd6) + (s2_i * 64'sd10);

    // 3-Tap Discrete Exterior Hodge Laplacian: L_H = 2*s2 - (s1 + s3)
    wire signed [63:0] hodge_lap = (s2_i <<< 1) - inner_sum;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            hw_mode_o          <= 2'b00;
            hw_ctrl_word_o     <= 64'd0;
            era_state_o        <= 64'sd0;
            fused_siac_hodge_o <= 64'sd0;
            ai_trigger_fire_o  <= 1'b0;
        end else begin
            hw_mode_o      <= next_mode;
            hw_ctrl_word_o <= {16'h5334, reg_ids, 10'd0, active_cone_popcnt, 14'd4, next_mode};
            if (is_custom3_v34 || retract_en_i) begin
                era_state_o <= retract_en_i ? (era_state_o - delta_q32_i)
                                            : (era_state_o + delta_q32_i);
            end
            fused_siac_hodge_o <= (next_mode == 2'b10) ? (siac_num - hodge_lap) : siac_num;
            ai_trigger_fire_o  <= (s2_i > 64'sd2147483648) || (s2_i < -64'sd2147483648);
        end
    end
endmodule
`default_nettype wire
