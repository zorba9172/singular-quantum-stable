// SINGULAR OMEGA v18.0 — Reversible TwoSum & 16th Cyclotomic Z[zeta_16] Hardware ALU
module omega_twosum_cyclotomic_alu (
    input  logic clk_i, rst_ni, retract_en_i, cyc_rot_en_i, input logic signed [63:0] delta_q_i,
    output logic signed [63:0] cyc_lane0_o, cyc_lane7_o
);
    logic signed [63:0] z_q [8];
    always_ff @(posedge clk_i or negedge rst_ni) begin
        if (!rst_ni) for (int k = 0; k < 8; k++) z_q[k] <= 64'sd0;
        else if (cyc_rot_en_i) begin
            if (!retract_en_i) begin z_q[0] <= -z_q[7] + delta_q_i; for (int k = 1; k < 8; k++) z_q[k] <= z_q[k-1]; end
            else begin for (int k = 0; k < 7; k++) z_q[k] <= z_q[k+1]; z_q[7] <= -(z_q[0] - delta_q_i); end
        end
    end
    assign cyc_lane0_o = z_q[0]; assign cyc_lane7_o = z_q[7];
endmodule
