// ============================================================================
// SINGULAR Ω v35.0.0 — Synthesizable SystemVerilog RTL:
// `omega_topological_self_morphing_arc_unit.sv`
// Implements RISC-V Custom-3 (`0x7B`, funct7=`0x03`) Topological Coboundary
// Rank-2 Edge Surgery, 5-Tap SIAC & Hodge 1-Form Self-Morphing Silicon Unit.
// Enforces Constitutional Rule `SG-ARCH-400` (<= 400 physical lines).
// ============================================================================
module omega_topological_self_morphing_arc_unit (
    input  logic        clk,
    input  logic        rst_n,
    input  logic        valid_i,
    input  logic [31:0] insn_i,
    input  logic [63:0] cone_mask_i,
    input  logic [15:0] domain_n_i,
    input  logic        edge_surgery_en_i,
    input  logic        sheaf_h1_repair_en_i,
    input  logic signed [63:0] s_m2_i,
    input  logic signed [63:0] s_m1_i,
    input  logic signed [63:0] s_0_i,
    input  logic signed [63:0] s_p1_i,
    input  logic signed [63:0] s_p2_i,
    input  logic signed [63:0] coboundary_delta_i,
    output logic        ready_o,
    output logic [2:0]  morph_mode_o,
    output logic [63:0] hw_ctrl_word_o,
    output logic signed [63:0] fused_topo_siac_o,
    output logic signed [63:0] retract_exact_o
);
    localparam logic [6:0] OPCODE_CUSTOM3  = 7'h7B;
    localparam logic [6:0] FUNCT7_TOPO_V35 = 7'h03;

    localparam logic [2:0] MODE_IDLE_BYPASS             = 3'd0;
    localparam logic [2:0] MODE_SPARSE_SARC_SIAC        = 3'd1;
    localparam logic [2:0] MODE_TOPO_RANK2_SURGERY      = 3'd2;
    localparam logic [2:0] MODE_HODGE_COHOMOLOGY_REPAIR = 3'd3;
    localparam logic [2:0] MODE_DENSE_SIMD_BURST        = 3'd4;

    logic is_v35_topo_insn;
    assign is_v35_topo_insn = (insn_i[6:0] == OPCODE_CUSTOM3) && (insn_i[31:25] == FUNCT7_TOPO_V35);

    logic [6:0] active_k;
    always_comb begin
        active_k = 7'd0;
        for (int b = 0; b < 64; b++) begin
            active_k = active_k + {6'd0, cone_mask_i[b]};
        end
    end

    logic [23:0] lhs_100k, rhs_22n;
    assign lhs_100k = {17'd0, active_k} * 24'd100;
    assign rhs_22n  = {8'd0, domain_n_i} * 24'd22;

    logic [2:0] next_mode;
    always_comb begin
        if (active_k == 7'd0)
            next_mode = MODE_IDLE_BYPASS;
        else if ((domain_n_i > 16'd0) && (lhs_100k > rhs_22n))
            next_mode = MODE_DENSE_SIMD_BURST;
        else if (sheaf_h1_repair_en_i)
            next_mode = MODE_HODGE_COHOMOLOGY_REPAIR;
        else if (edge_surgery_en_i || (insn_i[14:12] == 3'b011))
            next_mode = MODE_TOPO_RANK2_SURGERY;
        else
            next_mode = MODE_SPARSE_SARC_SIAC;
    end

    // Rank-2 Topological Edge Coboundary Update + Exact Inverse Retraction
    logic signed [63:0] u_m2, u_m1, u_0, u_p1, u_p2;
    assign u_m2 = s_m2_i + coboundary_delta_i;
    assign u_m1 = s_m1_i + coboundary_delta_i;
    assign u_0  = s_0_i  + coboundary_delta_i;
    assign u_p1 = s_p1_i + coboundary_delta_i;
    assign u_p2 = s_p2_i + coboundary_delta_i;

    logic signed [63:0] siac_num, hodge_lap, combined_val;
    assign siac_num  = (u_0 * 64'sd10) + ((u_m1 + u_p1) * 64'sd6) + (u_m2 + u_p2);
    assign hodge_lap = (u_0 <<< 1) - (u_m1 + u_p1);

    always_comb begin
        case (next_mode)
            MODE_IDLE_BYPASS:             combined_val = s_0_i;
            MODE_SPARSE_SARC_SIAC:        combined_val = siac_num;
            MODE_TOPO_RANK2_SURGERY:      combined_val = siac_num - hodge_lap;
            MODE_HODGE_COHOMOLOGY_REPAIR: combined_val = siac_num - (hodge_lap <<< 1);
            default:                      combined_val = u_0;
        endcase
    end

    logic [14:0] reg_fields;
    assign reg_fields = {insn_i[24:15], insn_i[11:7]};

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            ready_o           <= 1'b0;
            morph_mode_o      <= MODE_IDLE_BYPASS;
            hw_ctrl_word_o    <= 64'd0;
            fused_topo_siac_o <= 64'sd0;
            retract_exact_o   <= 64'sd0;
        end else if (valid_i && is_v35_topo_insn) begin
            ready_o           <= 1'b1;
            morph_mode_o      <= next_mode;
            hw_ctrl_word_o    <= {16'h5335, 1'b0, active_k, domain_n_i, reg_fields, 6'd0, next_mode};
            fused_topo_siac_o <= combined_val;
            retract_exact_o   <= u_0 - coboundary_delta_i;
        end else begin
            ready_o           <= 1'b0;
        end
    end
endmodule
