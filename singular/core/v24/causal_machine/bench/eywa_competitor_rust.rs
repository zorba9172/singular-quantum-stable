// ============================================================================
// EYWA Ω4 / SINGULAR Ω v39 — External Rust Competitor (C-ABI cdylib)
// Compiled with: rustc -C opt-level=3 -C target-cpu=native --crate-type cdylib
// Implements the exact same mathematical kernels as C++23 and EYWA ARM64 ASM:
//   1. rust_equal_work_v39_cone: TwoSum EFT + 11-tap SIAC (weights / 3024.0)
//   2. rust_incremental_cone_e2e: Causal-cone gather + 11-tap SIAC + UndoLog
// ============================================================================

#[no_mangle]
pub unsafe extern "C" fn rust_equal_work_v39_cone(
    s_ptr: *mut f64,
    eh_ptr: *mut f64,
    zz_ptr: *const f64,
    mid_ptr: *mut f64,
    out_ptr: *mut f64,
    n_cone: usize,
    dt: f64,
) -> f64 {
    let sp = std::slice::from_raw_parts_mut(s_ptr, n_cone + 10);
    let ehp = std::slice::from_raw_parts_mut(eh_ptr, n_cone + 10);
    let zp = std::slice::from_raw_parts(zz_ptr, n_cone + 10);
    let mp = std::slice::from_raw_parts_mut(mid_ptr, n_cone);
    let op = std::slice::from_raw_parts_mut(out_ptr, n_cone);

    for i in 0..(n_cone + 10) {
        let a = sp[i];
        let d = zp[i] * dt + ehp[i];
        let sm = a + d;
        let v = sm - a;
        sp[i] = sm;
        ehp[i] = (a - (sm - v)) + (d - v);
    }
    std::sync::atomic::compiler_fence(std::sync::atomic::Ordering::SeqCst);
    for i in 0..n_cone {
        mp[i] = 860.0 * sp[i + 5]
            + 630.0 * (sp[i + 4] + sp[i + 6])
            + 336.0 * (sp[i + 3] + sp[i + 7])
            + 100.0 * (sp[i + 2] + sp[i + 8])
            + 15.0 * (sp[i + 1] + sp[i + 9])
            + (sp[i] + sp[i + 10]);
    }
    std::sync::atomic::compiler_fence(std::sync::atomic::Ordering::SeqCst);
    for i in 0..n_cone {
        op[i] = mp[i] / 3024.0;
    }
    op[0]
}

#[no_mangle]
pub unsafe extern "C" fn rust_incremental_cone_e2e(
    state: *mut f64,
    zz_ptr: *const f64,
    cone_indices: *const u32,
    undo_idx: *mut u32,
    undo_val: *mut f64,
    cone_len: usize,
    n_total: usize,
    dt: f64,
) -> f64 {
    let st = std::slice::from_raw_parts_mut(state, n_total + 10);
    let zp = std::slice::from_raw_parts(zz_ptr, n_total + 10);
    let ci = std::slice::from_raw_parts(cone_indices, cone_len);
    let ui = std::slice::from_raw_parts_mut(undo_idx, cone_len);
    let uv = std::slice::from_raw_parts_mut(undo_val, cone_len);

    let mut energy = 0.0f64;
    for k in 0..cone_len {
        let idx = ci[k] as usize;
        ui[k] = ci[k];
        uv[k] = st[idx];
        let stencil = (860.0 * st[idx + 5]
            + 630.0 * (st[idx + 4] + st[idx + 6])
            + 336.0 * (st[idx + 3] + st[idx + 7])
            + 100.0 * (st[idx + 2] + st[idx + 8])
            + 15.0 * (st[idx + 1] + st[idx + 9])
            + (st[idx] + st[idx + 10]))
            / 3024.0;
        let upd = st[idx] + zp[idx] * dt * stencil;
        st[idx] = upd;
        energy += upd * upd;
    }
    for k in (0..cone_len).rev() {
        st[ui[k] as usize] = uv[k];
    }
    energy
}
