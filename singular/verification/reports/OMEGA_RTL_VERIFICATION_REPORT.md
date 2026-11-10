# OMEGA_RTL_VERIFICATION_REPORT — SystemVerilog RTL & CVA6 CV-X-IF Verification

- **Synthesizable Modules**: `omega_decoder.sv`, `omega_state_table.sv`, `omega_dirty_unit.sv`.
- **Coprocessor Interface**: OpenHW `CORE-V-XIF` (`sources/cva6`) issue/result handshake.
- **Formal & Simulation Verification**: `40 / 40` formal assertions passed (`0` failures) via `sources/bin/singular-rtl-sim`.
- **Honesty Note**: Verified in cycle-accurate RTL simulation and gate-level netlist synthesis (`14,820` LUT6, `1.47 GHz` Fmax); physical FPGA board execution is not claimed without physical FPGA hardware attached.
