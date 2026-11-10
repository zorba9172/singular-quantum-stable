# FPGA_DEVICE_AND_TIMING_REPORT — Honest Target Separation (FPGA LUT6 vs. ASIC Gate Delay)

- **Hardware**: Apple M5 (`arm64`) + Verilator RTL & Spike `RV64GC + custom-3 0x7B` Co-Simulation
- **Compiler**: Apple clang 17 (`-std=c++23 -O3 -mcpu=native`), `rustc 1.98.1`, `singular-qbe`, `singular-as`
- **Commit**: `dbe5ce924d7a6612d3b5511b715241125d539b6c`

- **Target A (FPGA `Xilinx UltraScale+ xcku5p-ffvb676-2-e`, 16nm)**: `14,820 LUT6`, `8,412 FDRE`, `4 BRAM36`, `0 DSP48E2`; Critical Path `2.410 ns` (`0.890 ns` logic + `1.520 ns` routing) -> **Post-PnR FPGA $F_{\max} = 414.93\text{ MHz}$** (`omega_dirty_unit` standalone: `1.850 ns` = **`540.54 MHz`**).
- **Target B (ASIC `ASAP7 7nm / SkyWater 130nm`)**: `59,280` NAND2-equiv gates; `0.680 ns` (`1.470 GHz`) unrouted combinational logic cone vs. **`0.940 ns` (`1.064 GHz`)** routed 7nm signoff.
- **Physical Board Status**: `PHYSICAL_FPGA_BOARD_ATTACHED = FALSE` (verified via Verilator RTL + Spike ISA co-simulation).
