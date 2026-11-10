#pragma once
// ============================================================================
// SINGULAR Ω v13.0 — General Quantum Scientific Compiler (`Quantum Ω-05`)
// Gate 1: Typed Pauli/Hamiltonian IR (`Ω-QIR`) | Gate 2: Auto Algorithm Selection
// Gate 3: Noncommuting 2D Benchmarks | Gate 4: Incremental Krylov-SIAC (`Δ⁺/Δ⁻`)
// Gate 5: `.qcy -> AST -> Ω-QIR -> Kernel` | Gate 6: Verification | Gate 7: Perf
// @layer(core) @owner("singular.v13.quantum_compiler") @contract("CY-V13-QIR-01-07")
// Enforces Constitutional Rule `SG-ARCH-400` (<= 400 physical lines).
// ============================================================================
#include "cycraft/quantum_syntax/omega_cir_lowering.hpp"
#include "cycraft/quantum_syntax/omega_frustrated_lattice_cyclotomic_solver.hpp"
#include "cycraft/quantum_syntax/omega_symbolic_parser.hpp"
#if defined(__APPLE__)
#ifndef ACCELERATE_NEW_LAPACK
#define ACCELERATE_NEW_LAPACK 1
#endif
#include <Accelerate/Accelerate.h>
#endif
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <string>
#include <vector>

namespace cycraft::omega::quantum_syntax {

enum class OmegaBoundaryKind : uint8_t { OPEN_BOUNDARY = 0, PERIODIC_TORUS = 1, CYLINDRICAL = 2 };

enum class OmegaQuantumAlgorithmKind : uint8_t {
    ALG_EXACT_CYCLOTOMIC_BUTTERFLY_TM = 0,
    ALG_SYMPLECTIC_STABILIZER_TABLEAU = 1,
    ALG_DENSE_EXACT_DIAGONALIZATION = 2,
    ALG_MATRIX_FREE_SPARSE_LANCZOS = 3,
    ALG_APPROXIMATE_MPS_TT_SVD = 4
};

struct OmegaPauliTerm {
    uint64_t x_mask = 0, z_mask = 0;
    double coeff_re = 0.0, coeff_im = 0.0;
    int locality_weight = 0, site_u = -1, site_v = -1;
    std::string term_kind;
};

struct OmegaGeneralHamiltonianIr {
    std::string source_file, hamiltonian_symbol;
    int L = 3, W = 3, num_qubits = 9;
    OmegaBoundaryKind boundary = OmegaBoundaryKind::PERIODIC_TORUS;
    double Jh0 = 1.0, Jv0 = -0.8, Jd0 = -0.6, hX0 = 0.75, gZ0 = 0.35;
    std::vector<double> site_hX, site_gZ;
    std::vector<OmegaPauliTerm> terms;
    bool parameters_from_qcy_source = false;
};

struct OmegaHamiltonianStructureProfile {
    int num_qubits = 0, max_locality_k = 0, graph_treewidth = 0;
    size_t total_terms = 0, diagonal_z_terms = 0, offdiag_x_terms = 0, commuting_pairs = 0, noncommuting_pairs = 0;
    bool is_hermitian = true, is_diagonal_z = false, is_commuting = false, is_stabilizer_compatible = false;
    bool z2_spin_flip_conserved = false, u1_magnetization_conserved = false, integer_couplings = false;
    double hermiticity_frobenius_err = 0.0, commutator_norm_normalized = 0.0, commutator_frobenius_norm = 0.0;
    double sparsity_ratio = 0.0, selection_latency_us = 0.0;
    OmegaQuantumAlgorithmKind selected_algorithm = OmegaQuantumAlgorithmKind::ALG_MATRIX_FREE_SPARSE_LANCZOS;
    std::string selected_algorithm_name, selection_rationale;
};

struct OmegaMpsTruncationRow {
    int bond_dim_chi = 0;
    double svd_truncation_err = 0.0, fidelity_err = 0.0, mps_variational_energy = 0.0, abs_energy_err = 0.0, residual_norm = 0.0;
    bool is_exact = false;
};

struct OmegaQuantumEigenResult {
    int L = 0, W = 0, num_qubits = 0, krylov_iters = 0, matvecs = 0;
    OmegaQuantumAlgorithmKind algorithm_used = OmegaQuantumAlgorithmKind::ALG_DENSE_EXACT_DIAGONALIZATION;
    std::string algorithm_name;
    bool is_exact_method = true, used_warm_start = false, fallback_to_cold_triggered = false;
    double E0 = 0.0, E1 = 0.0, spectral_gap = 0.0, residual_norm_r0 = 0.0, residual_norm_r1 = 0.0;
    double temple_kato_lower_bound = 0.0, bauer_fike_error_bound = 0.0, norm_err = 0.0, hermiticity_err = 0.0;
    double mag_X = 0.0, mag_Z = 0.0, corr_ZZ_horiz = 0.0, entanglement_svn_bits = 0.0;
    double elapsed_ms = 0.0, diag_build_ms = 0.0;
    size_t peak_memory_bytes = 0;
    std::vector<double> psi0, psi1, diag_Zz, convergence_r0_history;
    std::vector<OmegaMpsTruncationRow> mps_sweep;
};

class OmegaV13GeneralQuantumCompilerEngine {
public:
    static inline int popcnt64(uint64_t x) { return __builtin_popcountll(x); }
    static inline bool pauli_commutes(const OmegaPauliTerm& a, const OmegaPauliTerm& b) {
        return ((popcnt64(a.x_mask & b.z_mask) + popcnt64(a.z_mask & b.x_mask)) & 1) == 0;
    }
    static const char* algorithm_mnemonic(OmegaQuantumAlgorithmKind k) {
        switch (k) {
            case OmegaQuantumAlgorithmKind::ALG_EXACT_CYCLOTOMIC_BUTTERFLY_TM: return "EXACT_CYCLOTOMIC_BUTTERFLY_TM";
            case OmegaQuantumAlgorithmKind::ALG_SYMPLECTIC_STABILIZER_TABLEAU: return "SYMPLECTIC_STABILIZER_TABLEAU";
            case OmegaQuantumAlgorithmKind::ALG_DENSE_EXACT_DIAGONALIZATION: return "DENSE_EXACT_DIAGONALIZATION";
            case OmegaQuantumAlgorithmKind::ALG_MATRIX_FREE_SPARSE_LANCZOS: return "MATRIX_FREE_SPARSE_LANCZOS";
            default: return "APPROXIMATE_MPS_TT_SVD";
        }
    }

    static OmegaGeneralHamiltonianIr build_hamiltonian_ir(int L, int W, double Jh0, double Jv0, double Jd0,
                                                          double hX0, double gZ0, OmegaBoundaryKind bc = OmegaBoundaryKind::PERIODIC_TORUS,
                                                          bool from_qcy = false, const std::string& src_path = "<memory.qcy>",
                                                          int mut_site = -1, double dJh = 0.0, double dhX = 0.0, double dgZ = 0.0) {
        OmegaGeneralHamiltonianIr ir{};
        ir.source_file = src_path; ir.hamiltonian_symbol = "ℋ_0";
        ir.L = L; ir.W = W; ir.num_qubits = L * W; ir.boundary = bc;
        ir.Jh0 = Jh0; ir.Jv0 = Jv0; ir.Jd0 = Jd0; ir.hX0 = hX0; ir.gZ0 = gZ0;
        ir.parameters_from_qcy_source = from_qcy;
        ir.site_hX.resize(ir.num_qubits, 0.0); ir.site_gZ.resize(ir.num_qubits, 0.0);
        for (int r = 0; r < L; ++r) {
            for (int c = 0; c < W; ++c) {
                int u = r * W + c;
                if (c + 1 < W || bc != OmegaBoundaryKind::OPEN_BOUNDARY) {
                    int v = r * W + ((c + 1) % W);
                    double jh = Jh0 + (u == mut_site ? dJh : 0.0);
                    if (std::abs(jh) > 1e-15 && u != v) ir.terms.push_back({0ULL, (1ULL << u) | (1ULL << v), -jh, 0.0, 2, u, v, "ZZ_horiz"});
                }
                if (r + 1 < L || bc == OmegaBoundaryKind::PERIODIC_TORUS) {
                    int v = ((r + 1) % L) * W + c;
                    if (std::abs(Jv0) > 1e-15 && u != v) ir.terms.push_back({0ULL, (1ULL << u) | (1ULL << v), -Jv0, 0.0, 2, u, v, "ZZ_vert"});
                }
                if ((r + 1 < L || bc == OmegaBoundaryKind::PERIODIC_TORUS) && (c + 1 < W || bc != OmegaBoundaryKind::OPEN_BOUNDARY)) {
                    int v = ((r + 1) % L) * W + ((c + 1) % W);
                    if (std::abs(Jd0) > 1e-15 && u != v) ir.terms.push_back({0ULL, (1ULL << u) | (1ULL << v), -Jd0, 0.0, 2, u, v, "ZZ_diag"});
                }
                double h_rc = hX0 * (1.0 + 0.25 * ((r + 2 * c) % 3)) + (u == mut_site ? dhX : 0.0);
                ir.site_hX[u] = h_rc;
                if (std::abs(h_rc) > 1e-15) ir.terms.push_back({1ULL << u, 0ULL, -h_rc, 0.0, 1, u, -1, "X_trans"});
                double g_rc = gZ0 * (((r + c) & 1) ? -1.0 : 1.0) * (1.0 + 0.5 * ((r * c) % 3)) + (u == mut_site ? dgZ : 0.0);
                ir.site_gZ[u] = g_rc;
                if (std::abs(g_rc) > 1e-15) ir.terms.push_back({0ULL, 1ULL << u, +g_rc, 0.0, 1, u, -1, "Z_long"});
            }
        }
        return ir;
    }

    static OmegaGeneralHamiltonianIr lower_qcy_to_hamiltonian_ir(const OmegaSymbolicAst& ast, const OmegaCirModule& cir) {
        int L = 4, W = 4, bc_int = 1;
        double Jh = 1.0, Jv = -0.8, Jd = -0.6, hX = 0.75, gZ = 0.35;
        for (const auto& d : ast.state_decls) {
            if (d.raw_type_expr.find("𝕋[") != std::string::npos && d.dimensions.size() >= 2) {
                L = static_cast<int>(d.dimensions[0]); W = static_cast<int>(d.dimensions[1]);
            }
        }
        bool found_hq = false;
        for (const auto& n : cir.nodes) {
            if (n.op_mnemonic == "quantum.noncommuting_hamiltonian_hq" && n.numeric_params.size() >= 5) {
                Jh = n.numeric_params[0]; Jv = n.numeric_params[1]; Jd = n.numeric_params[2];
                hX = n.numeric_params[3]; gZ = n.numeric_params[4];
                if (n.numeric_params.size() >= 6) bc_int = static_cast<int>(n.numeric_params[5]);
                found_hq = true;
            }
        }
        return build_hamiltonian_ir(L, W, Jh, Jv, Jd, hX, gZ, static_cast<OmegaBoundaryKind>(bc_int), found_hq, ast.source_path);
    }

    static OmegaHamiltonianStructureProfile analyze_structure_and_select(const OmegaGeneralHamiltonianIr& ir, bool force_approx_mps = false) {
        auto t0 = std::chrono::steady_clock::now();
        OmegaHamiltonianStructureProfile p{};
        p.num_qubits = ir.num_qubits; p.total_terms = ir.terms.size();
        p.graph_treewidth = (ir.boundary == OmegaBoundaryKind::PERIODIC_TORUS) ? (2 * std::min(ir.L, ir.W)) : std::min(ir.L, ir.W);
        p.z2_spin_flip_conserved = true; p.u1_magnetization_conserved = true; p.integer_couplings = true;
        for (const auto& t : ir.terms) {
            p.max_locality_k = std::max(p.max_locality_k, t.locality_weight);
            p.hermiticity_frobenius_err += t.coeff_im * t.coeff_im;
            if (t.x_mask == 0) ++p.diagonal_z_terms; else { ++p.offdiag_x_terms; p.u1_magnetization_conserved = false; }
            if (t.z_mask != 0 && (popcnt64(t.z_mask) & 1)) p.z2_spin_flip_conserved = false;
            if (std::abs(t.coeff_re - std::round(t.coeff_re)) > 1e-9) p.integer_couplings = false;
        }
        p.hermiticity_frobenius_err = std::sqrt(p.hermiticity_frobenius_err);
        p.is_hermitian = (p.hermiticity_frobenius_err < 1e-14);
        p.is_diagonal_z = (p.offdiag_x_terms == 0);
        std::vector<OmegaPauliTerm> merged;
        for (const auto& t : ir.terms) {
            bool found = false;
            for (auto& m : merged) if (m.x_mask == t.x_mask && m.z_mask == t.z_mask) { m.coeff_re += t.coeff_re; found = true; break; }
            if (!found) merged.push_back(t);
        }
        double comm_sq = 0.0;
        for (size_t i = 0; i < merged.size(); ++i)
            for (size_t j = i + 1; j < merged.size(); ++j)
                if (pauli_commutes(merged[i], merged[j])) ++p.commuting_pairs;
                else { ++p.noncommuting_pairs; double c = 2.0 * merged[i].coeff_re * merged[j].coeff_re; comm_sq += c * c; }
        p.is_commuting = (p.noncommuting_pairs == 0);
        p.commutator_norm_normalized = std::sqrt(comm_sq);
        p.commutator_frobenius_norm = p.commutator_norm_normalized * std::pow(2.0, 0.5 * ir.num_qubits);
        p.sparsity_ratio = static_cast<double>(ir.num_qubits + 1) / std::pow(2.0, ir.num_qubits);
        if (!force_approx_mps && p.is_diagonal_z) {
            p.selected_algorithm = OmegaQuantumAlgorithmKind::ALG_EXACT_CYCLOTOMIC_BUTTERFLY_TM;
            p.selection_rationale = "Diagonal Z-basis Hamiltonian ([H_Z, H_X]=0): Exact Cyclotomic Z[zeta_16] Butterfly Transfer-Matrix.";
        } else if (!force_approx_mps && p.is_commuting && !p.is_diagonal_z) {
            p.selected_algorithm = OmegaQuantumAlgorithmKind::ALG_SYMPLECTIC_STABILIZER_TABLEAU;
            p.selection_rationale = "Mutually commuting non-diagonal Pauli generators: F2 Symplectic Stabilizer Tableau.";
        } else if (force_approx_mps || ir.num_qubits > 20) {
            p.selected_algorithm = OmegaQuantumAlgorithmKind::ALG_APPROXIMATE_MPS_TT_SVD;
            p.selection_rationale = "Noncommuting 2D system with N>20 (or MPS mode): Controlled Approximate TT-SVD/MPS with explicit truncation error bound.";
        } else if (ir.num_qubits <= 11) {
            p.selected_algorithm = OmegaQuantumAlgorithmKind::ALG_DENSE_EXACT_DIAGONALIZATION;
            p.selection_rationale = "Noncommuting Hamiltonian ([H_Z,H_X]!=0) with small Hilbert space (N<=11): Dense Exact Diagonalization (LAPACK dsyev).";
        } else {
            p.selected_algorithm = OmegaQuantumAlgorithmKind::ALG_MATRIX_FREE_SPARSE_LANCZOS;
            p.selection_rationale = "Noncommuting Hamiltonian ([H_Z,H_X]!=0) with 12<=N<=20: Matrix-Free Bitwise-Pauli Reorthogonalized Lanczos.";
        }
        p.selected_algorithm_name = algorithm_mnemonic(p.selected_algorithm);
        p.selection_latency_us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
        return p;
    }

    static void build_diagonal_z_table(const OmegaGeneralHamiltonianIr& ir, std::vector<double>& D) {
        const size_t dim = 1ULL << ir.num_qubits; D.assign(dim, 0.0);
        for (const auto& t : ir.terms) {
            if (t.x_mask != 0) continue;
            const uint64_t zm = t.z_mask; const double c = t.coeff_re;
            for (size_t z = 0; z < dim; ++z) D[z] += ((popcnt64(z & zm) & 1) ? -c : c);
        }
    }

    static void apply_hamiltonian_matvec(const OmegaGeneralHamiltonianIr& ir, const std::vector<double>& D,
                                         const std::vector<double>& v, std::vector<double>& out) {
        const size_t dim = D.size();
        for (size_t z = 0; z < dim; ++z) out[z] = D[z] * v[z];
        for (int q = 0; q < ir.num_qubits; ++q) {
            const double hq = ir.site_hX[q]; if (std::abs(hq) <= 1e-15) continue;
            const size_t bit = 1ULL << q;
            for (size_t z = 0; z < dim; ++z) out[z] -= hq * v[z ^ bit];
        }
    }

    static void evaluate_observables_and_verification(const OmegaGeneralHamiltonianIr& ir, const std::vector<double>& D,
                                                      OmegaQuantumEigenResult& res, bool compute_mps_sweep = false) {
        const size_t dim = res.psi0.size();
        double norm_sq = 0.0; for (double x : res.psi0) norm_sq += x * x;
        res.norm_err = std::abs(1.0 - norm_sq);
        std::vector<double> Hpsi(dim, 0.0); apply_hamiltonian_matvec(ir, D, res.psi0, Hpsi);
        double r_sq = 0.0;
        for (size_t z = 0; z < dim; ++z) { double d = Hpsi[z] - res.E0 * res.psi0[z]; r_sq += d * d; }
        res.residual_norm_r0 = std::sqrt(r_sq); res.bauer_fike_error_bound = res.residual_norm_r0;
        res.temple_kato_lower_bound = res.E0 - (res.residual_norm_r0 * res.residual_norm_r0) / std::max(1e-6, res.E1 - res.E0);
        double mx = 0.0, mz = 0.0, czz = 0.0; int n_horiz = 0;
        for (int q = 0; q < ir.num_qubits; ++q) {
            const size_t bit = 1ULL << q; double ex = 0.0, ez = 0.0;
            for (size_t z = 0; z < dim; ++z) { ex += res.psi0[z] * res.psi0[z ^ bit]; ez += res.psi0[z] * res.psi0[z] * ((z & bit) ? -1.0 : 1.0); }
            mx += ex; mz += ez;
        }
        for (const auto& t : ir.terms) if (t.term_kind == "ZZ_horiz") {
            double ezz = 0.0; for (size_t z = 0; z < dim; ++z) ezz += res.psi0[z] * res.psi0[z] * ((popcnt64(z & t.z_mask) & 1) ? -1.0 : 1.0);
            czz += ezz; ++n_horiz;
        }
        res.mag_X = mx / ir.num_qubits; res.mag_Z = mz / ir.num_qubits; res.corr_ZZ_horiz = (n_horiz > 0) ? (czz / n_horiz) : 0.0;
        const int nA = ir.num_qubits / 2, nB = ir.num_qubits - nA, dimA = 1 << nA, dimB = 1 << nB, kRank = std::min(dimA, dimB);
        std::vector<double> mat(res.psi0), svals(kRank, 0.0), U(dimA * kRank, 0.0), VT(kRank * dimB, 0.0);
#if defined(__APPLE__)
        int m = dimA, n = dimB, lda = dimA, ldu = dimA, ldvt = kRank, info = 0, lwork = -1; double wkopt = 0.0;
        char job = compute_mps_sweep ? 'S' : 'N';
        dgesvd_(&job, &job, &m, &n, mat.data(), &lda, svals.data(), U.data(), &ldu, VT.data(), &ldvt, &wkopt, &lwork, &info);
        lwork = static_cast<int>(wkopt); std::vector<double> work(std::max(1, lwork));
        dgesvd_(&job, &job, &m, &n, mat.data(), &lda, svals.data(), U.data(), &ldu, VT.data(), &ldvt, work.data(), &lwork, &info);
#endif
        double svn = 0.0; for (double sv : svals) { double p = sv * sv; if (p > 1e-15) svn -= p * std::log2(p); }
        res.entanglement_svn_bits = svn;
        if (compute_mps_sweep) {
            for (int chi : {2, 4, 8, 16, 32}) {
                if (chi > kRank) continue;
                double trunc_err = 0.0, kept_norm_sq = 0.0;
                for (int k = 0; k < kRank; ++k) if (k < chi) kept_norm_sq += svals[k] * svals[k]; else trunc_err += svals[k] * svals[k];
                std::vector<double> psi_chi(dim, 0.0), Hpsi_chi(dim, 0.0);
                double inv_norm = 1.0 / std::sqrt(std::max(1e-15, kept_norm_sq));
                for (int k = 0; k < chi; ++k) {
                    double sk = svals[k] * inv_norm;
                    for (int j = 0; j < dimB; ++j) {
                        double skj = sk * VT[k + j * kRank];
                        for (int i = 0; i < dimA; ++i) psi_chi[i + j * dimA] += U[i + k * dimA] * skj;
                    }
                }
                apply_hamiltonian_matvec(ir, D, psi_chi, Hpsi_chi);
                double e_mps = 0.0, r_mps_sq = 0.0;
                for (size_t z = 0; z < dim; ++z) e_mps += psi_chi[z] * Hpsi_chi[z];
                for (size_t z = 0; z < dim; ++z) { double d = Hpsi_chi[z] - e_mps * psi_chi[z]; r_mps_sq += d * d; }
                res.mps_sweep.push_back({chi, trunc_err, 1.0 - kept_norm_sq, e_mps, std::abs(e_mps - res.E0), std::sqrt(r_mps_sq), (chi == kRank)});
            }
        }
    }

    static OmegaQuantumEigenResult solve_dense_ed(const OmegaGeneralHamiltonianIr& ir, bool compute_mps_sweep = false) {
        auto t0 = std::chrono::steady_clock::now();
        OmegaQuantumEigenResult res{}; res.L = ir.L; res.W = ir.W; res.num_qubits = ir.num_qubits;
        res.algorithm_used = OmegaQuantumAlgorithmKind::ALG_DENSE_EXACT_DIAGONALIZATION;
        res.algorithm_name = "DENSE_EXACT_DIAGONALIZATION"; res.is_exact_method = true;
        const size_t dim = 1ULL << ir.num_qubits; build_diagonal_z_table(ir, res.diag_Zz);
        std::vector<double> H(dim * dim, 0.0), evals(dim, 0.0);
        for (size_t z = 0; z < dim; ++z) {
            H[z * dim + z] = res.diag_Zz[z];
            for (int q = 0; q < ir.num_qubits; ++q) if (std::abs(ir.site_hX[q]) > 1e-15) H[z * dim + (z ^ (1ULL << q))] -= ir.site_hX[q];
        }
        double herm_sq = 0.0;
        for (size_t i = 0; i < dim; ++i) for (size_t j = i + 1; j < dim; ++j) { double d = H[i * dim + j] - H[j * dim + i]; herm_sq += 2.0 * d * d; }
        res.hermiticity_err = std::sqrt(herm_sq);
#if defined(__APPLE__)
        char jobz = 'V', uplo = 'U'; int n = static_cast<int>(dim), lda = n, info = 0, lwork = -1; double wkopt = 0.0;
        dsyev_(&jobz, &uplo, &n, H.data(), &lda, evals.data(), &wkopt, &lwork, &info);
        lwork = static_cast<int>(wkopt); std::vector<double> work(std::max(1, lwork));
        dsyev_(&jobz, &uplo, &n, H.data(), &lda, evals.data(), work.data(), &lwork, &info);
#endif
        res.E0 = evals[0]; res.E1 = evals[1]; res.spectral_gap = res.E1 - res.E0;
        res.psi0.assign(H.begin(), H.begin() + dim); res.krylov_iters = 1; res.matvecs = 1;
        evaluate_observables_and_verification(ir, res.diag_Zz, res, compute_mps_sweep);
        res.elapsed_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        res.peak_memory_bytes = (dim * dim + 3 * dim) * sizeof(double);
        return res;
    }

    static OmegaQuantumEigenResult solve_matrix_free_lanczos(const OmegaGeneralHamiltonianIr& ir, int max_iters = 76,
                                                             double tol = 1e-12, const std::vector<double>* warm_psi0 = nullptr,
                                                             const std::vector<double>* prebuilt_D = nullptr, bool compute_mps_sweep = false,
                                                             const std::vector<double>* warm_psi1 = nullptr, bool skip_svd = false) {
        auto t0 = std::chrono::steady_clock::now();
        OmegaQuantumEigenResult res{}; res.L = ir.L; res.W = ir.W; res.num_qubits = ir.num_qubits;
        res.algorithm_used = OmegaQuantumAlgorithmKind::ALG_MATRIX_FREE_SPARSE_LANCZOS;
        res.algorithm_name = warm_psi0 ? "INCREMENTAL_WARM_KRYLOV_SIAC_LANCZOS" : "MATRIX_FREE_SPARSE_LANCZOS";
        res.is_exact_method = true; res.used_warm_start = (warm_psi0 != nullptr);
        const size_t dim = 1ULL << ir.num_qubits;
        if (prebuilt_D && prebuilt_D->size() == dim) res.diag_Zz = *prebuilt_D; else build_diagonal_z_table(ir, res.diag_Zz);
        res.diag_build_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        const int m_max = std::min<int>(max_iters, static_cast<int>(dim));
        std::vector<std::vector<double>> V; V.reserve(m_max + 1); std::vector<double> v0(dim, 0.0);
        if (warm_psi0 && warm_psi0->size() == dim) {
            if (warm_psi1 && warm_psi1->size() == dim) {
                std::vector<double> h0(dim, 0.0), h1(dim, 0.0);
                apply_hamiltonian_matvec(ir, res.diag_Zz, *warm_psi0, h0);
                apply_hamiltonian_matvec(ir, res.diag_Zz, *warm_psi1, h1);
                double m00 = 0.0, m01 = 0.0, m11 = 0.0;
                for (size_t z = 0; z < dim; ++z) { m00 += (*warm_psi0)[z] * h0[z]; m01 += (*warm_psi0)[z] * h1[z]; m11 += (*warm_psi1)[z] * h1[z]; }
                double tr = m00 + m11, det = m00 * m11 - m01 * m01, lam = 0.5 * (tr - std::sqrt(std::max(0.0, tr * tr - 4.0 * det)));
                double c0 = (std::abs(m01) > 1e-14) ? m01 : ((m00 <= m11) ? 1.0 : 0.0);
                double c1 = (std::abs(m01) > 1e-14) ? (lam - m00) : ((m00 <= m11) ? 0.0 : 1.0);
                for (size_t z = 0; z < dim; ++z) v0[z] = c0 * (*warm_psi0)[z] + c1 * (*warm_psi1)[z];
            } else {
                v0 = *warm_psi0;
            }
        } else {
            for (size_t z = 0; z < dim; ++z) v0[z] = 1.0 + 0.15 * std::sin(0.37 * static_cast<double>(z + 1));
        }
        auto normalize_vec = [](std::vector<double>& v) -> double {
            double s = 0.0; for (double x : v) s += x * x;
            double nrm = std::sqrt(s); if (nrm > 1e-15) for (double& x : v) x /= nrm;
            return nrm;
        };
        normalize_vec(v0); V.push_back(v0);
        std::vector<double> alpha, beta, w(dim, 0.0), y0_best, y1_best; double best_E0 = 0.0, best_E1 = 0.0, best_res0 = 1.0;
        for (int j = 0; j < m_max; ++j) {
            apply_hamiltonian_matvec(ir, res.diag_Zz, V[j], w); ++res.matvecs;
            double a_j = 0.0; for (size_t z = 0; z < dim; ++z) a_j += V[j][z] * w[z];
            alpha.push_back(a_j);
            for (int pass = 0; pass < 2; ++pass)
                for (int k = 0; k <= j; ++k) {
                    double dot = 0.0; for (size_t z = 0; z < dim; ++z) dot += V[k][z] * w[z];
                    for (size_t z = 0; z < dim; ++z) w[z] -= dot * V[k][z];
                }
            double b_j = normalize_vec(w); beta.push_back(b_j); V.push_back(w);
            const int cur_m = j + 1;
            if (cur_m >= 2) {
                std::vector<double> T(cur_m * cur_m, 0.0), tevals(cur_m, 0.0);
                for (int i = 0; i < cur_m; ++i) { T[i * cur_m + i] = alpha[i]; if (i + 1 < cur_m) { T[i * cur_m + i + 1] = beta[i]; T[(i + 1) * cur_m + i] = beta[i]; } }
#if defined(__APPLE__)
                char jobz = 'V', uplo = 'U'; int n = cur_m, lda = cur_m, info = 0, lwork = std::max(1, 3 * cur_m); std::vector<double> work(lwork);
                dsyev_(&jobz, &uplo, &n, T.data(), &lda, tevals.data(), work.data(), &lwork, &info);
#endif
                best_E0 = tevals[0]; best_E1 = tevals[1]; best_res0 = std::abs(b_j * T[cur_m - 1]);
                res.convergence_r0_history.push_back(best_res0);
                y0_best.assign(T.begin(), T.begin() + cur_m);
                if (!warm_psi0) y1_best.assign(T.begin() + cur_m, T.begin() + 2 * cur_m);
                if ((best_res0 < tol && cur_m >= 4) || b_j < 1e-13) { res.krylov_iters = cur_m; break; }
            }
            res.krylov_iters = cur_m;
        }
        res.E0 = best_E0; res.E1 = best_E1; res.spectral_gap = res.E1 - res.E0; res.psi0.assign(dim, 0.0);
        for (size_t k = 0; k < y0_best.size(); ++k) { const double yk = y0_best[k]; for (size_t z = 0; z < dim; ++z) res.psi0[z] += yk * V[k][z]; }
        normalize_vec(res.psi0);
        if (!y1_best.empty()) {
            res.psi1.assign(dim, 0.0);
            for (size_t k = 0; k < y1_best.size(); ++k) { const double yk = y1_best[k]; for (size_t z = 0; z < dim; ++z) res.psi1[z] += yk * V[k][z]; }
            normalize_vec(res.psi1);
        }
        if (!skip_svd) evaluate_observables_and_verification(ir, res.diag_Zz, res, compute_mps_sweep);
        else res.residual_norm_r0 = best_res0;
        res.elapsed_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        res.peak_memory_bytes = (V.size() + 2) * dim * sizeof(double);
        return res;
    }
};

} // namespace cycraft::omega::quantum_syntax
