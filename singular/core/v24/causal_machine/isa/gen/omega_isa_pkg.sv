// GENERATED from omega_isa.yaml by omega_isagen. Do not edit.
package omega_isa_pkg;
  localparam logic [6:0] OMEGA_OPCODE = 7'h7b;
  localparam logic [6:0] OMEGA_FUNCT7 = 7'h1;
  localparam int unsigned OMEGA_ENTRIES = 64;
  localparam int unsigned OMEGA_CSR_S_BIT = 0;
  localparam int unsigned OMEGA_CSR_U_BIT = 1;
  localparam logic [62:0] OMEGA_ERR_INVALID_ID = 63'd1;
  localparam logic [62:0] OMEGA_ERR_VERSION_MISMATCH = 63'd2;
  localparam logic [62:0] OMEGA_ERR_SELF_DEP = 63'd3;
  localparam logic [2:0] OMEGA_F3_TRACK = 3'd0;
  localparam logic OMEGA_RS2Z_TRACK = 1'b0;
  localparam logic [2:0] OMEGA_F3_DIRTY = 3'd1;
  localparam logic OMEGA_RS2Z_DIRTY = 1'b0;
  localparam logic [2:0] OMEGA_F3_PROP = 3'd2;
  localparam logic OMEGA_RS2Z_PROP = 1'b1;
  localparam logic [2:0] OMEGA_F3_VALID = 3'd3;
  localparam logic OMEGA_RS2Z_VALID = 1'b0;
  localparam logic [2:0] OMEGA_F3_CLEAN = 3'd4;
  localparam logic OMEGA_RS2Z_CLEAN = 1'b1;
endpackage
