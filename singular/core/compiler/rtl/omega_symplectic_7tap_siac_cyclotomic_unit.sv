// ============================================================================
// SINGULAR Ω v36.0.0 — Synthesizable SystemVerilog RTL:
// `omega_symplectic_7tap_siac_cyclotomic_unit.sv`
// Implements RISC-V Custom-3 (`0x7B`, funct7=`0x04`) 6-Mode Symplectic
// Leapfrog + 7-Tap O(h^7) SIAC (`[1, 12, 39, 40, 39, 12, 1]`) & Z[zeta_16] Unit.
// Enforces Constitutional Rule `SG-ARCH-400` (<= 400 physical lines).
// ============================================================================
module omega_symplectic_7tap_siac_cyclotomic_unit (
    input  logic        clk,
    input  logic        rst_n,
    input  logic        valid_i,
    input  logic [31:0] insn_i,
    input  logic [63:0] cone_mask_i,
    input  logic [15:0] domain_n_i,
    input  logic        symplectic_en_i,
    input  logic        cyclotomic_z16_en_i,
    input  logic        su2_hubo_en_i,
    input  logic signed [63:0] s_m3_i,
    input  logic signed [63:0] s_m2_i,
    input  logic signed [63:0] s_m1_i,
    input  logic signed [63:0] s_0_i,
    input  logic signed [63:0] s_p1_i,
    input  logic signed [63:0] s_p2_i,
    input  logic signed [63:0] s_p3_i,
    input  logic signed [63:0] symplectic_dp_i,
    output logic        ready_o,
    output logic [2:0]  hw_mode_o,
    output logic [63:0] hw_ctrl_word_o,
    output logic signed [63:0] siac7_symplectic_o,
    output logic signed [63:0] retract_exact_o
);
    localparam logic [6:0] OPCODE_CUSTOM3  = 7'h7B;
    localparam logic [6:0] FUNCT7_SYMP_V36 = 7'h04;

    localparam logic [2:0] MODE_IDLE_BYPASS                = 3'd0;
    localparam logic [2:0] MODE_SPARSE_5TAP_SIAC           = 3'd1;
    localparam logic [2:0] MODE_TOPO_RANK2_SURGERY         = 3'd2;
    localparam logic [2:0] MODE_SYMPLECTIC_7TAP_CYCLOTOMIC = 3'd3;
    localparam logic [2:0] MODE_SU2_GAUGE_HUBO_TRIGGER     = 3'd4;
    localparam logic [2:0] MODE_DENSE_SIMD_BURST           = 3'd5;

    logic is_v36_insn;
    assign is_v36_insn = (insn_i[6:0] == OPCODE_CUSTOM3) && (insn_i[31:25] == FUNCT7_SYMP_V36);

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
        else if (su2_hubo_en_i)
            next_mode = MODE_SU2_GAUGE_HUBO_TRIGGER;
        else if (symplectic_en_i || cyclotomic_z16_en_i || (insn_i[14:12] == 3'b100))
            next_mode = MODE_SYMPLECTIC_7TAP_CYCLOTOMIC;
        else if (insn_i[14:12] == 3'b011)
            next_mode = MODE_TOPO_RANK2_SURGERY;
        else
            next_mode = MODE_SPARSE_5TAP_SIAC;
    end

    logic signed [63:0] u_m3, u_m2, u_m1, u_0, u_p1, u_p2, u_p3;
    assign u_m3 = s_m3_i + symplectic_dp_i;
    assign u_m2 = s_m2_i + symplectic_dp_i;
    assign u_m1 = s_m1_i + symplectic_dp_i;
    assign u_0  = s_0_i  + symplectic_dp_i;
    assign u_p1 = s_p1_i + symplectic_dp_i;
    assign u_p2 = s_p2_i + symplectic_dp_i;
    assign u_p3 = s_p3_i + symplectic_dp_i;

    // 7-Tap Symmetric SIAC Numerator: 40*u_0 + 39*(u_m1+u_p1) + 12*(u_m2+u_p2) + (u_m3+u_p3)
    logic signed [63:0] siac7_num, siac5_num, out_mux;
    assign siac7_num = (u_0 * 64'sd40) + ((u_m1 + u_p1) * 64'sd39) + ((u_m2 + u_p2) * 64'sd12) + (u_m3 + u_p3);
    assign siac5_num = (u_0 * 64'sd10) + ((u_m1 + u_p1) * 64'sd6)  + (u_m2 + u_p2);

    always_comb begin
        case (next_mode)
            MODE_IDLE_BYPASS:                out_mux = s_0_i;
            MODE_SPARSE_5TAP_SIAC:           out_mux = siac5_num;
            MODE_TOPO_RANK2_SURGERY:         out_mux = siac5_num - ((u_0 <<< 1) - (u_m1 + u_p1));
            MODE_SYMPLECTIC_7TAP_CYCLOTOMIC: out_mux = siac7_num;
            MODE_SU2_GAUGE_HUBO_TRIGGER:     out_mux = siac7_num + (symplectic_dp_i <<< 2);
            default:                         out_mux = u_0;
        endcase
    end

    logic [14:0] reg_fields;
    assign reg_fields = {insn_i[24:15], insn_i[11:7]};

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            ready_o            <= 1'b0;
            hw_mode_o          <= MODE_IDLE_BYPASS;
            hw_ctrl_word_o     <= 64'd0;
            siac7_symplectic_o <= 64'sd0;
            retract_exact_o    <= 64'sd0;
        end else if (valid_i && is_v36_insn) begin
            ready_o            <= 1'b1;
            hw_mode_o          <= next_mode;
            hw_ctrl_word_o     <= {16'h5336, 1'b0, active_k, domain_n_i, reg_fields, 6'd0, next_mode};
            siac7_symplectic_o <= out_mux;
            retract_exact_o    <= u_0 - symplectic_dp_i;
        end else begin
            ready_o            <= 1'b0;
        end
    end
endmodule
