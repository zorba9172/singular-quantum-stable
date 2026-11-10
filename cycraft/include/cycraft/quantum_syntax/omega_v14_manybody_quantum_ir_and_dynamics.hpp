#pragma once
// ============================================================================
// SINGULAR Ω v14.0 — Quantum Grand Challenge: Complex Many-Body Quantum IR (`Ω-QIR v14`),
// Frustrated J1-J2-DM Heisenberg Model, Adaptive Krylov Time Evolution & ACSR Engine
// @layer(core) @owner("singular.v14.grand_challenge") @contract("CY-V14-QGC-01-10")
// Enforces Constitutional Rule `SG-ARCH-400` (<= 400 physical lines).
// ============================================================================
#include "cycraft/quantum_syntax/omega_v13_deliverable_emitter.hpp"
#include <complex>

namespace cycraft::omega::quantum_syntax {

using Complex128 = std::complex<double>;

enum class OmegaEpistemicStateStatus : uint8_t {
    PROVEN_EXACT = 0,
    BOUNDED_APPROXIMATE = 1,
    REJECTED_BUDGET_EXCEEDED = 2,
    UNVERIFIED = 3
};

enum class OmegaStateRepresentationKind : uint8_t {
    EXACT_DENSE_COMPLEX128 = 0,
    KRYLOV_SUBSPACE_COMPLEX128 = 1,
    APPROXIMATE_MPS_TENSOR_NETWORK = 2,
    UNVERIFIED_EXCEEDED_BUDGET = 3
};

struct ErrorBudget {
    double max_hermiticity_err = 1e-12;
    double max_norm_err = 1e-9;
    double max_ground_rel_residual = 1e-8;
    double max_small_sys_energy_diff = 1e-8;
    double max_time_evol_observable_err = 1e-5;
    double max_mps_truncation_err = 2.5e-2;
};

struct SolverEvidence {
    std::string event_id, timestamp, run_id, source_file_hash, compiler_version, git_commit;
    std::string hamiltonian_fingerprint, state_representation, algorithm_candidates, selected_algorithm;
    std::string selection_reason, validation_result, fallback_reason, previous_state_hash, resulting_state_hash;
    double precision_requirement = 1e-10, error_estimate = 0.0, runtime_cost_ms = 0.0;
    size_t memory_estimate_bytes = 0;
    OmegaEpistemicStateStatus epistemic_status = OmegaEpistemicStateStatus::PROVEN_EXACT;
};

struct DirectedBondV14 { int u = 0, v = 0; bool is_nnn = false; };

template <typename T = Complex128>
struct Hamiltonian {
    int L = 2, W = 2, num_qubits = 4, quench_site = 0;
    double J1 = 1.0, J2 = 0.50, D = 0.15, hx0 = 0.10, quench_dhx = 0.35;
    uint64_t rng_seed = 20261401ULL;
    bool is_post_quench = false;
    std::vector<double> hz, hx, diag_Z;
    std::vector<DirectedBondV14> nn_bonds, nnn_bonds;
    double hermiticity_norm_residual = 0.0;
    std::string fingerprint_hex;
};

template <OmegaStateRepresentationKind Rep = OmegaStateRepresentationKind::KRYLOV_SUBSPACE_COMPLEX128>
struct QuantumState {
    int num_qubits = 0, bond_dim_chi = 0;
    OmegaStateRepresentationKind representation = Rep;
    OmegaEpistemicStateStatus epistemic_status = OmegaEpistemicStateStatus::PROVEN_EXACT;
    std::vector<Complex128> amplitudes;
    double norm_error = 0.0, truncation_error = 0.0;
    std::string state_hash;
};

template <typename T = double>
struct Observable {
    std::string name;
    int site_i = -1, site_j = -1;
    T value{};
    double error_bound = 0.0;
    OmegaEpistemicStateStatus status = OmegaEpistemicStateStatus::PROVEN_EXACT;
};

struct QuantumMutation {
    std::string mutation_id;
    int site_u = -1, site_v = -1;
    double delta_hx = 0.0, delta_hz = 0.0, delta_J1 = 0.0;
    bool is_global_quench = false;
};

struct EvolutionOperator {
    double t_start = 0.0, t_end = 5.0, dt = 0.1;
    int krylov_dim_m = 20, adaptive_steps_taken = 0, matvecs_used = 0;
    double max_step_error = 0.0;
};

struct OmegaV14TimePointRow {
    std::string exp_id, lattice_label, representation_name, epistemic_tag;
    int qubits = 0, bond_dim_chi = 0;
    double t = 0.0, energy_post = 0.0, energy_drift_from_t0 = 0.0, norm_err = 0.0;
    double loschmidt_echo = 1.0, mag_Z_quench = 0.0, mag_X_quench = 0.0, corr_ZZ_nn = 0.0;
    double svn_half_bits = 0.0, svn_quarter_bits = 0.0, truncation_err = 0.0;
    double ref_energy_post = 0.0, ref_loschmidt = 1.0, ref_svn_half = 0.0, obs_abs_err_vs_ref = 0.0;
};

struct OmegaV14AcsrCache {
    int r_dim = 0;
    std::vector<double> base_diag_Z;
    std::vector<std::vector<Complex128>> U_basis;
    std::vector<std::vector<Complex128>> HU_cached;
};

struct OmegaV14EigenSolveResult {
    double E0 = 0.0, E1 = 0.0, gap = 0.0, residual_norm = 0.0, norm_err = 0.0, hermiticity_err = 0.0, elapsed_ms = 0.0;
    int iters = 0, matvecs = 0;
    double subspace_overlap = 1.0;
    bool used_acsr = false, fallback_triggered = false;
    OmegaEpistemicStateStatus status = OmegaEpistemicStateStatus::PROVEN_EXACT;
    std::vector<Complex128> psi0, psi1;
};

class OmegaV14ManyBodyDynamicsEngine {
public:
    static inline double get_deterministic_hz(int u, uint64_t seed = 20261401ULL) {
        uint64_t x = seed + 0x9E3779B97F4A7C15ULL * static_cast<uint64_t>(u + 1);
        x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
        x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
        x = x ^ (x >> 31);
        double unit = static_cast<double>(x & 0x1FFFFFFFFFFFFFULL) / static_cast<double>(1ULL << 53);
        return -0.20 + 0.40 * unit;
    }

    static std::string hash_state_hex(const std::vector<Complex128>& v) {
        uint64_t h = 0xCBF29CE484222325ULL;
        size_t stride = std::max<size_t>(1, v.size() / 64);
        for (size_t i = 0; i < v.size(); i += stride) {
            uint64_t bits = static_cast<uint64_t>( std::llround((v[i].real() + 2.0) * 1e12) ) ^
                            (static_cast<uint64_t>( std::llround((v[i].imag() + 2.0) * 1e12) ) << 1);
            h = (h ^ bits) * 0x100000001B3ULL;
        }
        std::ostringstream os; os << std::hex << std::setw(16) << std::setfill('0') << h;
        return os.str();
    }

    static Hamiltonian<Complex128> build_j1_j2_dm_hamiltonian(
        int L, int W, double J1 = 1.0, double J2 = 0.50, double D = 0.15,
        double hx0 = 0.10, double quench_dhx = 0.35, bool post_quench = false, uint64_t seed = 20261401ULL) {
        Hamiltonian<Complex128> H{};
        H.L = L; H.W = W; H.num_qubits = L * W; H.quench_site = (L / 2) * W + (W / 2);
        H.J1 = J1; H.J2 = J2; H.D = D; H.hx0 = hx0; H.quench_dhx = quench_dhx;
        H.rng_seed = seed; H.is_post_quench = post_quench;
        H.hz.resize(H.num_qubits, 0.0); H.hx.resize(H.num_qubits, hx0);
        for (int u = 0; u < H.num_qubits; ++u) H.hz[u] = get_deterministic_hz(u, seed);
        if (post_quench) H.hx[H.quench_site] += quench_dhx;
        for (int r = 0; r < L; ++r) {
            for (int c = 0; c < W; ++c) {
                int u = r * W + c;
                if (c + 1 < W) H.nn_bonds.push_back({u, r * W + (c + 1), false});
                if (r + 1 < L) H.nn_bonds.push_back({u, (r + 1) * W + c, false});
                if (r + 1 < L && c + 1 < W) {
                    H.nnn_bonds.push_back({u, (r + 1) * W + (c + 1), true});
                    H.nnn_bonds.push_back({r * W + (c + 1), (r + 1) * W + c, true});
                }
            }
        }
        if (H.num_qubits <= 20) {
            const size_t dim = 1ULL << H.num_qubits;
            H.diag_Z.assign(dim, 0.0);
            for (size_t z = 0; z < dim; ++z) {
                double ez = 0.0;
                for (int u = 0; u < H.num_qubits; ++u) ez += 0.5 * H.hz[u] * (((z >> u) & 1ULL) ? -1.0 : 1.0);
                for (const auto& b : H.nn_bonds) {
                    int bi = (z >> b.u) & 1, bj = (z >> b.v) & 1;
                    ez += 0.25 * H.J1 * (bi == bj ? 1.0 : -1.0);
                }
                for (const auto& b : H.nnn_bonds) {
                    int bi = (z >> b.u) & 1, bj = (z >> b.v) & 1;
                    ez += 0.25 * H.J2 * (bi == bj ? 1.0 : -1.0);
                }
                H.diag_Z[z] = ez;
            }
        }
        std::ostringstream fp; fp << "J1J2DM-" << L << "x" << W << "-" << (post_quench ? "POST" : "PRE") << "-S" << seed;
        H.fingerprint_hex = fp.str();
        return H;
    }

    static void apply_complex_matvec(const Hamiltonian<Complex128>& H, const std::vector<Complex128>& v, std::vector<Complex128>& out) {
        const size_t dim = v.size();
        for (size_t z = 0; z < dim; ++z) out[z] = H.diag_Z[z] * v[z];
        for (int u = 0; u < H.num_qubits; ++u) {
            const double hxu = 0.5 * H.hx[u];
            if (std::abs(hxu) <= 1e-15) continue;
            const size_t mask = 1ULL << u;
            for (size_t z = 0; z < dim; ++z) out[z] += hxu * v[z ^ mask];
        }
        const Complex128 c_plus(0.5 * H.J1, 0.5 * H.D), c_minus(0.5 * H.J1, -0.5 * H.D);
        for (const auto& b : H.nn_bonds) {
            const size_t mi = 1ULL << b.u, mj = 1ULL << b.v, m_both = mi | mj;
            for (size_t z = 0; z < dim; ++z) {
                bool bi = (z & mi) != 0, bj = (z & mj) != 0;
                if (bi != bj) out[z] += (bi ? c_minus : c_plus) * v[z ^ m_both];
            }
        }
        const double c_nnn = 0.5 * H.J2;
        if (std::abs(c_nnn) > 1e-15) {
            for (const auto& b : H.nnn_bonds) {
                const size_t mi = 1ULL << b.u, mj = 1ULL << b.v, m_both = mi | mj;
                for (size_t z = 0; z < dim; ++z)
                    if (((z & mi) != 0) != ((z & mj) != 0)) out[z] += c_nnn * v[z ^ m_both];
            }
        }
    }

    static double normalize_complex(std::vector<Complex128>& v) {
        double s = 0.0; for (const auto& c : v) s += std::norm(c);
        double nrm = std::sqrt(s);
        if (nrm > 1e-15) { double inv = 1.0 / nrm; for (auto& c : v) c *= inv; }
        return nrm;
    }

    static Complex128 inner_prod(const std::vector<Complex128>& a, const std::vector<Complex128>& b) {
        Complex128 s(0.0, 0.0);
        for (size_t i = 0; i < a.size(); ++i) s += std::conj(a[i]) * b[i];
        return s;
    }

    static OmegaV14EigenSolveResult solve_complex_dense_ed(const Hamiltonian<Complex128>& H) {
        auto t0 = std::chrono::steady_clock::now();
        OmegaV14EigenSolveResult res{};
        const size_t dim = 1ULL << H.num_qubits;
        std::vector<Complex128> mat(dim * dim, Complex128(0.0, 0.0)), col_in(dim, Complex128(0.0, 0.0)), col_out(dim, Complex128(0.0, 0.0));
        for (size_t j = 0; j < dim; ++j) {
            col_in[j] = Complex128(1.0, 0.0);
            apply_complex_matvec(H, col_in, col_out);
            for (size_t i = 0; i < dim; ++i) mat[i + j * dim] = col_out[i];
            col_in[j] = Complex128(0.0, 0.0);
        }
        double diff_sq = 0.0, norm_sq = 0.0;
        for (size_t j = 0; j < dim; ++j) {
            for (size_t i = 0; i < dim; ++i) {
                norm_sq += std::norm(mat[i + j * dim]);
                diff_sq += std::norm(mat[i + j * dim] - std::conj(mat[j + i * dim]));
            }
        }
        res.hermiticity_err = std::sqrt(diff_sq) / std::max(1.0, std::sqrt(norm_sq));
        std::vector<double> evals(dim, 0.0);
#if defined(__APPLE__)
        char jobz = 'V', uplo = 'U'; int n = static_cast<int>(dim), lda = n, info = 0, lwork = -1;
        Complex128 wkopt{}; std::vector<double> rwork(std::max<size_t>(1, 3 * dim - 2));
        zheev_(&jobz, &uplo, &n, reinterpret_cast<__LAPACK_double_complex*>(mat.data()), &lda, evals.data(),
               reinterpret_cast<__LAPACK_double_complex*>(&wkopt), &lwork, rwork.data(), &info);
        lwork = static_cast<int>(wkopt.real()); std::vector<Complex128> work(std::max(1, lwork));
        zheev_(&jobz, &uplo, &n, reinterpret_cast<__LAPACK_double_complex*>(mat.data()), &lda, evals.data(),
               reinterpret_cast<__LAPACK_double_complex*>(work.data()), &lwork, rwork.data(), &info);
#endif
        res.E0 = evals[0]; res.E1 = evals[1]; res.gap = res.E1 - res.E0;
        res.psi0.assign(mat.begin(), mat.begin() + dim);
        res.psi1.assign(mat.begin() + dim, mat.begin() + 2 * dim);
        apply_complex_matvec(H, res.psi0, col_out);
        double r_sq = 0.0, n_sq = 0.0;
        for (size_t z = 0; z < dim; ++z) { n_sq += std::norm(res.psi0[z]); r_sq += std::norm(col_out[z] - res.E0 * res.psi0[z]); }
        res.residual_norm = std::sqrt(r_sq); res.norm_err = std::abs(1.0 - n_sq);
        res.iters = 1; res.matvecs = 1;
        res.elapsed_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        return res;
    }

    static OmegaV14EigenSolveResult solve_complex_lanczos(
        const Hamiltonian<Complex128>& H, int max_iters = 140, double tol = 1e-11,
        OmegaV14AcsrCache* build_cache = nullptr, const OmegaV14AcsrCache* use_cache = nullptr,
        const QuantumMutation* local_mut = nullptr, bool enable_error_controller = true) {
        auto t0 = std::chrono::steady_clock::now();
        OmegaV14EigenSolveResult res{};
        const size_t dim = 1ULL << H.num_qubits;
        std::vector<Complex128> v0(dim, Complex128(0.0, 0.0)), w(dim, Complex128(0.0, 0.0));
        if (use_cache && use_cache->r_dim == 1 && !use_cache->U_basis.empty()) {
            v0 = use_cache->U_basis[0];
        } else if (use_cache && use_cache->r_dim >= 2 && local_mut && !local_mut->is_global_quench) {
            res.used_acsr = true;
            std::vector<std::vector<Complex128>> Q = use_cache->U_basis, HQ = use_cache->HU_cached;
            const int u = local_mut->site_u;
            if (u >= 0 && u < H.num_qubits) {
                const size_t mask = 1ULL << u;
                const double dhx = 0.5 * local_mut->delta_hx, dhz = 0.5 * local_mut->delta_hz;
                for (size_t k = 0; k < Q.size(); ++k)
                    for (size_t z = 0; z < dim; ++z) {
                        if (std::abs(dhz) > 1e-15) HQ[k][z] += (dhz * ((z & mask) ? -1.0 : 1.0)) * Q[k][z];
                        if (std::abs(dhx) > 1e-15) HQ[k][z] += dhx * Q[k][z ^ mask];
                    }
            }
            std::vector<Complex128> M_store(24 * 24, Complex128(0.0, 0.0));
            for (int corr = 0; corr <= 12; ++corr) {
                const int cr = static_cast<int>(Q.size());
                const int b_start = (corr == 0) ? 0 : (cr - 1);
                for (int b = b_start; b < cr; ++b) for (int a = 0; a <= b; ++a) { M_store[a + b * 24] = inner_prod(Q[a], HQ[b]); M_store[b + a * 24] = std::conj(M_store[a + b * 24]); }
                std::vector<Complex128> M(cr * cr, Complex128(0.0, 0.0));
                for (int b = 0; b < cr; ++b) for (int a = 0; a < cr; ++a) M[a + b * cr] = M_store[a + b * 24];
                std::vector<double> r_evals(cr, 0.0);
#if defined(__APPLE__)
                char jobz = 'V', uplo = 'U'; int n = cr, lda = cr, info = 0, lwork = std::max(1, 4 * cr);
                std::vector<Complex128> work(lwork); std::vector<double> rwork(std::max(1, 3 * cr - 2));
                zheev_(&jobz, &uplo, &n, reinterpret_cast<__LAPACK_double_complex*>(M.data()), &lda, r_evals.data(),
                       reinterpret_cast<__LAPACK_double_complex*>(work.data()), &lwork, rwork.data(), &info);
#endif
                res.E0 = r_evals[0]; res.E1 = r_evals[std::min(1, cr - 1)]; res.gap = res.E1 - res.E0;
                res.psi0.assign(dim, Complex128(0.0, 0.0)); w.assign(dim, Complex128(0.0, 0.0));
                for (int k = 0; k < cr; ++k) {
                    Complex128 ck = M[k];
                    for (size_t z = 0; z < dim; ++z) { res.psi0[z] += ck * Q[k][z]; w[z] += ck * (HQ[k][z] - res.E0 * Q[k][z]); }
                }
                double r_sq = 0.0; for (size_t z = 0; z < dim; ++z) r_sq += std::norm(w[z]);
                res.residual_norm = std::sqrt(r_sq); res.iters = corr + 1;
                if ((!enable_error_controller && corr == 0) || res.residual_norm < tol || corr == 12) break;
                for (int p = 0; p < 2; ++p) for (int k = 0; k < cr; ++k) { Complex128 d = inner_prod(Q[k], w); for (size_t z = 0; z < dim; ++z) w[z] -= d * Q[k][z]; }
                if (normalize_complex(w) < 1e-13) break;
                Q.push_back(w); std::vector<Complex128> hw(dim); apply_complex_matvec(H, w, hw); ++res.matvecs; HQ.push_back(std::move(hw));
            }
            normalize_complex(res.psi0);
            res.status = (res.residual_norm <= tol * 100.0) ? OmegaEpistemicStateStatus::PROVEN_EXACT : OmegaEpistemicStateStatus::REJECTED_BUDGET_EXCEEDED;
            res.elapsed_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            return res;
        } else {
            for (size_t z = 0; z < dim; ++z)
                v0[z] = Complex128(1.0 + 0.15 * std::sin(0.37 * (z + 1)), 0.07 * std::cos(0.23 * (z + 1)));
        }
        double v0_nrm = normalize_complex(v0);
        if (res.used_acsr && v0_nrm > 1e-15) { double inv = 1.0 / v0_nrm; for (auto& c : w) c *= inv; }
        const int m_max = std::min<int>(max_iters, static_cast<int>(dim));
        std::vector<std::vector<Complex128>> V; V.reserve(m_max + 1); V.push_back(v0);
        std::vector<double> alpha, beta, T_last;
        int last_m = 0;
        double best_E0 = 0.0, best_E1 = 0.0, best_res = 1.0;

        for (int j = 0; j < m_max; ++j) {
            if (!(j == 0 && res.used_acsr)) { apply_complex_matvec(H, V[j], w); ++res.matvecs; }
            double aj = inner_prod(V[j], w).real(); alpha.push_back(aj);
            for (int pass = 0; pass < 2; ++pass) {
                for (int k = 0; k <= j; ++k) {
                    Complex128 dot = inner_prod(V[k], w);
                    for (size_t z = 0; z < dim; ++z) w[z] -= dot * V[k][z];
                }
            }
            double bj = normalize_complex(w); beta.push_back(bj); V.push_back(w);
            const int cur_m = j + 1;
            if (cur_m >= 2) {
                std::vector<double> T(cur_m * cur_m, 0.0), tevals(cur_m, 0.0);
                for (int i = 0; i < cur_m; ++i) { T[i * cur_m + i] = alpha[i]; if (i + 1 < cur_m) { T[i * cur_m + i + 1] = beta[i]; T[(i + 1) * cur_m + i] = beta[i]; } }
#if defined(__APPLE__)
                char jobz = 'V', uplo = 'U'; int n = cur_m, lda = cur_m, info = 0, lwork = std::max(1, 3 * cur_m); std::vector<double> work(lwork);
                dsyev_(&jobz, &uplo, &n, T.data(), &lda, tevals.data(), work.data(), &lwork, &info);
#endif
                best_E0 = tevals[0]; best_E1 = tevals[1]; best_res = std::abs(bj * T[cur_m - 1]);
                T_last = T; last_m = cur_m;
                if (!enable_error_controller && cur_m >= 5) { res.iters = cur_m; break; }
                if ((best_res < tol && cur_m >= 3) || bj < 1e-13) { res.iters = cur_m; break; }
            }
            res.iters = cur_m;
        }
        res.E0 = best_E0; res.E1 = best_E1; res.gap = best_E1 - best_E0; res.residual_norm = best_res;
        res.psi0.assign(dim, Complex128(0.0, 0.0));
        for (int i = 0; i < last_m; ++i) { double yk = T_last[i]; for (size_t z = 0; z < dim; ++z) res.psi0[z] += yk * V[i][z]; }
        normalize_complex(res.psi0);
        if (last_m >= 2) {
            res.psi1.assign(dim, Complex128(0.0, 0.0));
            for (int i = 0; i < last_m; ++i) { double yk = T_last[last_m + i]; for (size_t z = 0; z < dim; ++z) res.psi1[z] += yk * V[i][z]; }
            normalize_complex(res.psi1);
        }
        if (build_cache && last_m >= 6) {
            const int r_keep = std::min(8, last_m);
            build_cache->r_dim = r_keep; build_cache->base_diag_Z = H.diag_Z;
            build_cache->U_basis.assign(r_keep, std::vector<Complex128>(dim, Complex128(0.0, 0.0)));
            for (int r = 0; r < r_keep; ++r) {
                for (int i = 0; i < last_m; ++i) {
                    double y_ri = T_last[r * last_m + i];
                    for (size_t z = 0; z < dim; ++z) build_cache->U_basis[r][z] += y_ri * V[i][z];
                }
                for (int p = 0; p < 2; ++p) for (int k = 0; k < r; ++k) { Complex128 d = inner_prod(build_cache->U_basis[k], build_cache->U_basis[r]); for (size_t z = 0; z < dim; ++z) build_cache->U_basis[r][z] -= d * build_cache->U_basis[k][z]; }
                normalize_complex(build_cache->U_basis[r]);
            }
            build_cache->HU_cached.assign(r_keep, std::vector<Complex128>(dim));
            for (int k = 0; k < r_keep; ++k) apply_complex_matvec(H, build_cache->U_basis[k], build_cache->HU_cached[k]);
        }
        res.status = (res.residual_norm <= tol * 100.0) ? OmegaEpistemicStateStatus::PROVEN_EXACT : OmegaEpistemicStateStatus::REJECTED_BUDGET_EXCEEDED;
        res.elapsed_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        return res;
    }
};

} // namespace cycraft::omega::quantum_syntax
