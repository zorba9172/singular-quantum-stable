// ============================================================================
// SINGULAR OMEGA v24.0.0 — Synthesizable 1024-Entry Hardware State Table
// Tracks: version[31:0], dirty[0], deps_mask[63:0], valid_cert[31:0], owner_cap[15:0]
// Enforces Bounds (rs1 < 1024), Capability Isolation, Version Check & Overflow Guard
// ============================================================================
module omega_state_table #(
    parameter int ENTRIES = 1024
) (
    input  logic        clk_i,
    input  logic        rst_ni,
    input  logic        cmd_valid_i,
    input  logic [1:0]  cmd_op_i,        // 0: TRACK, 1: DIRTY, 2: PROP, 3: VALID
    input  logic [63:0] rs1_val_i,       // state_id
    input  logic [63:0] rs2_val_i,       // expected_version / dep_bit
    input  logic [15:0] caller_cap_i,    // active capability token
    output logic [63:0] result_o,        // rd return value / error code
    output logic [2:0]  err_code_o       // 0=OK, 1=INVALID_ID, 2=VER_MISMATCH, 3=CAP_DENIED, 4=OVERFLOW
);
    logic [31:0] version_q    [ENTRIES];
    logic        dirty_q      [ENTRIES];
    logic [63:0] deps_mask_q  [ENTRIES];
    logic [31:0] valid_cert_q [ENTRIES];
    logic [15:0] owner_cap_q  [ENTRIES];

    logic        id_in_bounds;
    logic [9:0]  idx;
    logic        cap_ok;
    logic        ver_match;
    logic        ver_overflow;

    assign id_in_bounds = (rs1_val_i < 64'd1024);
    assign idx          = rs1_val_i[9:0];
    assign cap_ok       = id_in_bounds && (caller_cap_i == owner_cap_q[idx]);
    assign ver_match    = id_in_bounds && (version_q[idx] == rs2_val_i[31:0]);
    assign ver_overflow = id_in_bounds && (version_q[idx] == 32'hFFFF_FFFF);

    always_comb begin
        err_code_o = 3'd0;
        result_o   = 64'd0;
        if (cmd_valid_i) begin
            if (!id_in_bounds || ((cmd_op_i == 2'd0) && (rs2_val_i >= 64'd64))) begin
                err_code_o = 3'd1; // ERR_INVALID_STATE_ID
                result_o   = 64'd1;
            end else if (!cap_ok) begin
                err_code_o = 3'd3; // ERR_CAPABILITY_DENIED
                result_o   = 64'd3;
            end else if (cmd_op_i == 2'd1) begin // omega.dirty
                if (!ver_match) begin
                    err_code_o = 3'd2; // ERR_VERSION_MISMATCH
                    result_o   = 64'd2;
                end else if (ver_overflow) begin
                    err_code_o = 3'd4; // ERR_VERSION_OVERFLOW
                    result_o   = 64'd4;
                end else begin
                    err_code_o = 3'd0;
                    result_o   = 64'd0;
                end
            end else if (cmd_op_i == 2'd2) begin // omega.prop
                result_o = dirty_q[idx] ? (deps_mask_q[idx] | (64'h1 << (idx & 10'd63))) : 64'd0;
            end else if (cmd_op_i == 2'd3) begin // omega.valid
                result_o = (!dirty_q[idx] && ver_match) ? 64'd1 : 64'd0;
            end
        end
    end

    always_ff @(posedge clk_i or negedge rst_ni) begin
        if (!rst_ni) begin
            for (int i = 0; i < ENTRIES; i++) begin
                version_q[i]    <= 32'd0;
                dirty_q[i]      <= 1'b0;
                deps_mask_q[i]  <= 64'd0;
                valid_cert_q[i] <= 32'd0;
                owner_cap_q[i]  <= 16'h0001;
            end
        end else if (cmd_valid_i && (err_code_o == 3'd0)) begin
            case (cmd_op_i)
                2'd0: deps_mask_q[idx][rs2_val_i[5:0]] <= 1'b1;
                2'd1: begin
                    version_q[idx] <= version_q[idx] + 32'd1;
                    dirty_q[idx]   <= 1'b1;
                end
                2'd2: begin
                    if (dirty_q[idx]) begin
                        for (int b = 0; b < 64; b++) begin
                            if (deps_mask_q[idx][b]) dirty_q[b] <= 1'b1;
                        end
                    end
                end
                2'd3: begin
                    if (dirty_q[idx] || !ver_match) begin
                        dirty_q[idx]      <= 1'b0;
                        valid_cert_q[idx] <= version_q[idx];
                    end
                end
            endcase
        end
    end
endmodule
