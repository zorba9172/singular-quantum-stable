// SINGULAR OMEGA v18.0 — Causal Compute Unit (OMEGA-CCU) Wavefront Priority Engine
module omega_causal_processing_unit #(parameter int NODES = 64) (
    input  logic clk_i, rst_ni, track_en_i, dirty_en_i, prop_en_i, input logic [5:0] src_idx_i, dst_idx_i,
    output logic [NODES-1:0] active_cone_o, output logic dense_burst_mode_o
);
    logic [NODES-1:0] cone_mask_q [NODES]; logic [NODES-1:0] frontier_q;
    always_ff @(posedge clk_i or negedge rst_ni) begin
        if (!rst_ni) begin frontier_q <= '0; for (int i = 0; i < NODES; i++) cone_mask_q[i] <= (64'h1 << i); end
        else begin
            if (track_en_i) cone_mask_q[src_idx_i][dst_idx_i] <= 1'b1;
            if (dirty_en_i) frontier_q[src_idx_i] <= 1'b1;
            else if (prop_en_i) frontier_q <= frontier_q | cone_mask_q[src_idx_i];
        end
    end
    assign active_cone_o = frontier_q; assign dense_burst_mode_o = ($countones(frontier_q) > 14);
endmodule
