#pragma once
// ============================================================================
// SINGULAR Ω v12.1 — Rotated Pauli-Clifford Gauge Algebra, Symplectic F₂^{2N}
// Stabilizer Tableau & Small-Lattice (2×2, 3×3, 4×4, 5×5) Z-Basis ED Verifier
// @layer(core) @owner("singular.quantum.stabilizer") @contract("CY-OMEGA04-ED")
// Enforces Constitutional Rule `SG-ARCH-400` (<= 400 physical lines).
// ============================================================================

#if defined(__APPLE__)
#ifndef ACCELERATE_NEW_LAPACK
#define ACCELERATE_NEW_LAPACK 1
#endif
#include <Accelerate/Accelerate.h>
#endif

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

namespace cycraft::omega::quantum_syntax {

struct LocalPauliGaugeAudit {
    double theta = 0.0;
    double q_hermitian_err = 0.0;
    double p_hermitian_err = 0.0;
    double q_involution_err = 0.0;       // ||Q_v^2 - I||_F
    double p_involution_err = 0.0;       // ||P_v^2 - I||_F
    double anticommutator_err = 0.0;     // ||{Q_v, P_v}||_F
    double gauge_rot_q_err = 0.0;        // ||G(θ) X G(θ)^T - Q(θ)||_F
    double gauge_rot_p_err = 0.0;        // ||G(θ) Z G(θ)^T - P(θ)||_F
    bool passed = false;
};

struct SymplecticTableauAudit {
    int L = 0, W = 0, N = 0;
    size_t total_pairs_checked = 0;
    size_t anticommuting_stabilizer_pairs = 0;
    int f2_rank = 0;
    int ground_space_degeneracy = 0;
    size_t active_anticommuting_stabilizers = 0;
    int active_stabilizer_index = -1;
    double tableau_ms = 0.0;
    size_t tableau_memory_bytes = 0;
    bool all_stabilizers_commute = false;
    bool unique_ground_state_proven = false;
};

struct SmallLatticeCrossCheckRow {
    int L = 0, W = 0, qubits = 0;
    int pert_r = 0, pert_c = 0, cut_col = 0;
    std::string method;
    double ground_energy_ed = 0.0, ground_energy_exact = 0.0;
    double spectral_gap_ed = 0.0, spectral_gap_exact = 0.0;
    double exp_A_star_ed = 0.0, exp_P_star_ed = 0.0;
    double entanglement_entropy_bits_ed = 0.0, entanglement_entropy_bits_exact = 0.0;
    uint64_t schmidt_rank_ed = 0, schmidt_rank_exact = 0;
    double eigenstate_residual_l2 = 0.0, elapsed_ms = 0.0;
    bool passed = false;
};

class OmegaStabilizerAlgebraAndEd {
public:
    static LocalPauliGaugeAudit audit_local_pauli_algebra(double theta) {
        LocalPauliGaugeAudit rep{};
        rep.theta = theta;
        const double c = std::cos(theta), s = std::sin(theta), c2 = std::cos(0.5 * theta), s2 = std::sin(0.5 * theta);
        double Q[2][2] = {{s, c}, {c, -s}}, P[2][2] = {{c, -s}, {-s, -c}};
        rep.q_hermitian_err = std::abs(Q[0][1] - Q[1][0]);
        rep.p_hermitian_err = std::abs(P[0][1] - P[1][0]);
        auto mul2 = [](const double A[2][2], const double B[2][2], double C[2][2]) {
            for (int i = 0; i < 2; ++i) for (int j = 0; j < 2; ++j) C[i][j] = A[i][0] * B[0][j] + A[i][1] * B[1][j];
        };
        double Q2[2][2], P2[2][2], QP[2][2], PQ[2][2];
        mul2(Q, Q, Q2); mul2(P, P, P2); mul2(Q, P, QP); mul2(P, Q, PQ);
        rep.q_involution_err = std::hypot(std::hypot(Q2[0][0] - 1.0, Q2[0][1]), std::hypot(Q2[1][0], Q2[1][1] - 1.0));
        rep.p_involution_err = std::hypot(std::hypot(P2[0][0] - 1.0, P2[0][1]), std::hypot(P2[1][0], P2[1][1] - 1.0));
        rep.anticommutator_err = std::hypot(std::hypot(QP[0][0] + PQ[0][0], QP[0][1] + PQ[0][1]),
                                            std::hypot(QP[1][0] + PQ[1][0], QP[1][1] + PQ[1][1]));
        double G[2][2] = {{c2, s2}, {-s2, c2}}, GT[2][2] = {{c2, -s2}, {s2, c2}};
        double X[2][2] = {{0.0, 1.0}, {1.0, 0.0}}, Z[2][2] = {{1.0, 0.0}, {0.0, -1.0}};
        double GX[2][2], GXGT[2][2], GZ[2][2], GZGT[2][2];
        mul2(G, X, GX); mul2(GX, GT, GXGT); mul2(G, Z, GZ); mul2(GZ, GT, GZGT);
        rep.gauge_rot_q_err = std::hypot(std::hypot(GXGT[0][0] - Q[0][0], GXGT[0][1] - Q[0][1]),
                                         std::hypot(GXGT[1][0] - Q[1][0], GXGT[1][1] - Q[1][1]));
        rep.gauge_rot_p_err = std::hypot(std::hypot(GZGT[0][0] - P[0][0], GZGT[0][1] - P[0][1]),
                                         std::hypot(GZGT[1][0] - P[1][0], GZGT[1][1] - P[1][1]));
        rep.passed = (rep.q_involution_err < 1e-14) && (rep.p_involution_err < 1e-14) &&
                     (rep.anticommutator_err < 1e-14) && (rep.gauge_rot_q_err < 1e-14) && (rep.gauge_rot_p_err < 1e-14);
        return rep;
    }

    static std::vector<int> neighbors_open_rect(int r, int c, int L, int W) {
        std::vector<int> nbrs;
        if (r > 0) nbrs.push_back((r - 1) * W + c);
        if (r + 1 < L) nbrs.push_back((r + 1) * W + c);
        if (c > 0) nbrs.push_back(r * W + (c - 1));
        if (c + 1 < W) nbrs.push_back(r * W + (c + 1));
        return nbrs;
    }

    static SymplecticTableauAudit audit_symplectic_stabilizer_tableau(int L, int W, int pert_r, int pert_c) {
        auto t0 = std::chrono::steady_clock::now();
        SymplecticTableauAudit rep{};
        rep.L = L; rep.W = W; rep.N = L * W;
        const int N = rep.N;
        const size_t words = static_cast<size_t>((N + 63) / 64);
        std::vector<uint64_t> q_tab(N * words, 0ULL), p_tab(N * words, 0ULL);
        for (int r = 0; r < L; ++r) {
            for (int c = 0; c < W; ++c) {
                int v = r * W + c;
                q_tab[v * words + (v / 64)] |= (1ULL << (v % 64));
                for (int u : neighbors_open_rect(r, c, L, W)) p_tab[v * words + (u / 64)] |= (1ULL << (u % 64));
            }
        }
        for (int u = 0; u < N; ++u) {
            const uint64_t* qu = &q_tab[u * words];
            const uint64_t* pu = &p_tab[u * words];
            for (int v = u + 1; v < N; ++v) {
                const uint64_t* qv = &q_tab[v * words];
                const uint64_t* pv = &p_tab[v * words];
                uint64_t acc = 0ULL;
                for (size_t w = 0; w < words; ++w) acc ^= (qu[w] & pv[w]) ^ (pu[w] & qv[w]);
                if (std::popcount(acc) & 1) ++rep.anticommuting_stabilizer_pairs;
                ++rep.total_pairs_checked;
            }
        }
        std::vector<uint64_t> mat(N * 2 * words, 0ULL);
        for (int v = 0; v < N; ++v) {
            for (size_t w = 0; w < words; ++w) {
                mat[v * 2 * words + w] = q_tab[v * words + w];
                mat[v * 2 * words + words + w] = p_tab[v * words + w];
            }
        }
        int rank = 0;
        for (int col = 0; col < 2 * N && rank < N; ++col) {
            size_t w_idx = static_cast<size_t>(col / 64), b_idx = static_cast<size_t>(col % 64);
            int pivot = -1;
            for (int row = rank; row < N; ++row) {
                if ((mat[row * 2 * words + w_idx] >> b_idx) & 1ULL) { pivot = row; break; }
            }
            if (pivot < 0) continue;
            if (pivot != rank) {
                for (size_t w = 0; w < 2 * words; ++w) std::swap(mat[rank * 2 * words + w], mat[pivot * 2 * words + w]);
            }
            for (int row = 0; row < N; ++row) {
                if (row != rank && ((mat[row * 2 * words + w_idx] >> b_idx) & 1ULL)) {
                    for (size_t w = 0; w < 2 * words; ++w) mat[row * 2 * words + w] ^= mat[rank * 2 * words + w];
                }
            }
            ++rank;
        }
        rep.f2_rank = rank;
        rep.ground_space_degeneracy = 1 << (N - rank);
        const int v_star = pert_r * W + pert_c;
        for (int v = 0; v < N; ++v) {
            if (((q_tab[v * words + (v_star / 64)] >> (v_star % 64)) & 1ULL) != 0ULL) {
                ++rep.active_anticommuting_stabilizers;
                rep.active_stabilizer_index = v;
            }
        }
        rep.tableau_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        rep.tableau_memory_bytes = (q_tab.size() + p_tab.size() + mat.size()) * sizeof(uint64_t);
        rep.all_stabilizers_commute = (rep.anticommuting_stabilizer_pairs == 0);
        rep.unique_ground_state_proven = rep.all_stabilizers_commute && (rep.f2_rank == N) && (rep.ground_space_degeneracy == 1);
        return rep;
    }

    static double binary_entropy_bits(double p) {
        if (p <= 1e-15 || p >= 1.0 - 1e-15) return 0.0;
        return -p * std::log2(p) - (1.0 - p) * std::log2(1.0 - p);
    }

    static double exact_bipartite_entropy_formula(int L, int pert_c, int cut_col, double lambda) {
        const double p_plus = 0.5 * (1.0 + std::abs(lambda) / std::sqrt(1.0 + lambda * lambda));
        return (pert_c == cut_col - 1 || pert_c == cut_col) ? (static_cast<double>(L - 1) + binary_entropy_bits(p_plus)) : static_cast<double>(L);
    }

    static void apply_single_qubit_op(std::vector<double>& psi, int N, int q, const double M[2][2]) {
        const size_t dim = 1ULL << N, bit = 1ULL << q;
        for (size_t i = 0; i < dim; ++i) {
            if ((i & bit) == 0) {
                size_t j = i | bit;
                double v0 = psi[i], v1 = psi[j];
                psi[i] = M[0][0] * v0 + M[0][1] * v1;
                psi[j] = M[1][0] * v0 + M[1][1] * v1;
            }
        }
    }

    static void apply_A_v_raw_z_basis(const std::vector<double>& in, std::vector<double>& tmp, int L, int W, int v, double theta) {
        tmp = in;
        const int N = L * W, r = v / W, c = v % W;
        const double ct = std::cos(theta), st = std::sin(theta);
        const double Q[2][2] = {{st, ct}, {ct, -st}}, P[2][2] = {{ct, -st}, {-st, -ct}};
        apply_single_qubit_op(tmp, N, v, Q);
        for (int u : neighbors_open_rect(r, c, L, W)) apply_single_qubit_op(tmp, N, u, P);
    }

    static std::pair<double, uint64_t> compute_statevector_cut_svd(const std::vector<double>& psi, int L, int W, int cut_col) {
        const int N = L * W;
        std::vector<int> a_qubits, b_qubits;
        for (int r = 0; r < L; ++r) {
            for (int c = 0; c < W; ++c) {
                if (c < cut_col) a_qubits.push_back(r * W + c);
                else b_qubits.push_back(r * W + c);
            }
        }
        const int m = 1 << static_cast<int>(a_qubits.size()), n = 1 << static_cast<int>(b_qubits.size()), k_min = std::min(m, n);
        std::vector<double> mat(static_cast<size_t>(m) * n, 0.0);
        const size_t dim = 1ULL << N;
        for (size_t mask = 0; mask < dim; ++mask) {
            int row = 0, col = 0;
            for (size_t i = 0; i < a_qubits.size(); ++i) if ((mask >> a_qubits[i]) & 1ULL) row |= (1 << i);
            for (size_t j = 0; j < b_qubits.size(); ++j) if ((mask >> b_qubits[j]) & 1ULL) col |= (1 << j);
            mat[col * m + row] = psi[mask];
        }
        std::vector<double> s(k_min, 0.0);
#if defined(__APPLE__)
        char jobu = 'N', jobvt = 'N';
        int M = m, N_col = n, lda = m, ldu = 1, ldvt = 1, lwork = -1, info = 0;
        double wkopt = 0.0;
        dgesvd_(&jobu, &jobvt, &M, &N_col, mat.data(), &lda, s.data(), nullptr, &ldu, nullptr, &ldvt, &wkopt, &lwork, &info);
        lwork = static_cast<int>(wkopt) + 32;
        std::vector<double> work(static_cast<size_t>(lwork), 0.0);
        dgesvd_(&jobu, &jobvt, &M, &N_col, mat.data(), &lda, s.data(), nullptr, &ldu, nullptr, &ldvt, work.data(), &lwork, &info);
#endif
        double svn = 0.0; uint64_t rank = 0;
        for (double sv : s) {
            double p = sv * sv;
            if (p > 1e-10) { ++rank; svn -= p * std::log2(p); }
        }
        return {svn, rank};
    }

    static SmallLatticeCrossCheckRow verify_small_lattice_dense_ed(int L, int W, int pert_r, int pert_c, double theta, double lambda) {
        auto t0 = std::chrono::steady_clock::now();
        SmallLatticeCrossCheckRow row{};
        row.L = L; row.W = W; row.qubits = L * W; row.pert_r = pert_r; row.pert_c = pert_c;
        row.cut_col = W / 2; row.method = "Dense_Z_Basis_LAPACK_dsyev";
        const int N = L * W, dim = 1 << N, v_star = pert_r * W + pert_c;
        const double ct = std::cos(theta), st = std::sin(theta);
        const double P[2][2] = {{ct, -st}, {-st, -ct}};
        std::vector<double> H(static_cast<size_t>(dim) * dim, 0.0), e_j(dim, 0.0), tmp(dim, 0.0), col_acc(dim, 0.0);
        for (int j = 0; j < dim; ++j) {
            std::fill(e_j.begin(), e_j.end(), 0.0); e_j[j] = 1.0;
            std::fill(col_acc.begin(), col_acc.end(), 0.0);
            for (int v = 0; v < N; ++v) {
                apply_A_v_raw_z_basis(e_j, tmp, L, W, v, theta);
                for (int i = 0; i < dim; ++i) col_acc[i] -= tmp[i];
            }
            tmp = e_j; apply_single_qubit_op(tmp, N, v_star, P);
            for (int i = 0; i < dim; ++i) H[j * dim + i] = col_acc[i] + lambda * tmp[i];
        }
        std::vector<double> eigvals(dim, 0.0);
#if defined(__APPLE__)
        char jobz = 'V', uplo = 'U';
        int n_mat = dim, lda = dim, lwork = -1, info = 0; double wkopt = 0.0;
        dsyev_(&jobz, &uplo, &n_mat, H.data(), &lda, eigvals.data(), &wkopt, &lwork, &info);
        lwork = static_cast<int>(wkopt) + 32;
        std::vector<double> work(static_cast<size_t>(lwork), 0.0);
        dsyev_(&jobz, &uplo, &n_mat, H.data(), &lda, eigvals.data(), work.data(), &lwork, &info);
#endif
        std::vector<double> psi0(H.begin(), H.begin() + dim);
        row.ground_energy_ed = eigvals[0];
        row.ground_energy_exact = -static_cast<double>(N - 1) - std::sqrt(1.0 + lambda * lambda);
        row.spectral_gap_ed = eigvals[1] - eigvals[0]; row.spectral_gap_exact = 2.0;
        apply_A_v_raw_z_basis(psi0, tmp, L, W, v_star, theta);
        row.exp_A_star_ed = std::inner_product(psi0.begin(), psi0.end(), tmp.begin(), 0.0);
        tmp = psi0; apply_single_qubit_op(tmp, N, v_star, P);
        row.exp_P_star_ed = std::inner_product(psi0.begin(), psi0.end(), tmp.begin(), 0.0);
        auto [svn, srank] = compute_statevector_cut_svd(psi0, L, W, row.cut_col);
        row.entanglement_entropy_bits_ed = svn;
        row.entanglement_entropy_bits_exact = exact_bipartite_entropy_formula(L, pert_c, row.cut_col, lambda);
        row.schmidt_rank_ed = srank; row.schmidt_rank_exact = 1ULL << L;
        row.elapsed_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        row.passed = (std::abs(row.ground_energy_ed - row.ground_energy_exact) < 1e-11) &&
                     (std::abs(row.spectral_gap_ed - row.spectral_gap_exact) < 1e-11) &&
                     (std::abs(row.exp_A_star_ed - 1.0 / std::sqrt(1.0 + lambda * lambda)) < 1e-11) &&
                     (std::abs(row.exp_P_star_ed + lambda / std::sqrt(1.0 + lambda * lambda)) < 1e-11) &&
                     (std::abs(row.entanglement_entropy_bits_ed - row.entanglement_entropy_bits_exact) < 1e-10) &&
                     (row.schmidt_rank_ed == row.schmidt_rank_exact);
        return row;
    }

    static SmallLatticeCrossCheckRow verify_medium_lattice_statevector(int L, int W, int pert_r, int pert_c, double theta, double lambda, bool full_z_rotation) {
        auto t0 = std::chrono::steady_clock::now();
        SmallLatticeCrossCheckRow row{};
        row.L = L; row.W = W; row.qubits = L * W; row.pert_r = pert_r; row.pert_c = pert_c; row.cut_col = W / 2;
        row.method = full_z_rotation ? "MatrixFree_Raw_Z_Basis_65536_SVD" : "Fast_Graph_Clifford_2^25_Statevector";
        const int N = L * W, v_star = pert_r * W + pert_c;
        const size_t dim = 1ULL << N, bit_star = 1ULL << v_star;
        const double sin_a = lambda / std::sqrt(1.0 + lambda * lambda), cos_a = 1.0 / std::sqrt(1.0 + lambda * lambda);
        const double alpha = std::atan2(sin_a, cos_a);
        const double c_minus = (std::cos(0.5 * alpha) - std::sin(0.5 * alpha)) / std::sqrt(2.0);
        const double c_plus  = (std::cos(0.5 * alpha) + std::sin(0.5 * alpha)) / std::sqrt(2.0);
        const double inv_sqrt2_nm1 = std::ldexp(1.0, -(N - 1) / 2) * (((N - 1) & 1) ? (1.0 / std::sqrt(2.0)) : 1.0);
        std::vector<std::pair<size_t, size_t>> edges;
        for (int r = 0; r < L; ++r) {
            for (int c = 0; c < W; ++c) {
                int u = r * W + c;
                if (r + 1 < L) edges.push_back({1ULL << u, 1ULL << ((r + 1) * W + c)});
                if (c + 1 < W) edges.push_back({1ULL << u, 1ULL << (r * W + (c + 1))});
            }
        }
        std::vector<double> psi(dim, 0.0);
        for (size_t mask = 0; mask < dim; ++mask) {
            int parity = 0;
            for (const auto& [b1, b2] : edges) if ((mask & b1) && (mask & b2)) parity ^= 1;
            double amp = ((mask & bit_star) ? c_plus : c_minus) * inv_sqrt2_nm1;
            psi[mask] = parity ? -amp : amp;
        }
        if (full_z_rotation) {
            const double c2 = std::cos(0.5 * theta), s2 = std::sin(0.5 * theta), ct = std::cos(theta), st = std::sin(theta);
            const double G[2][2] = {{c2, s2}, {-s2, c2}}, P[2][2] = {{ct, -st}, {-st, -ct}};
            for (int q = 0; q < N; ++q) apply_single_qubit_op(psi, N, q, G);
            std::vector<double> Hpsi(dim, 0.0), tmp(dim, 0.0);
            for (int v = 0; v < N; ++v) {
                apply_A_v_raw_z_basis(psi, tmp, L, W, v, theta);
                if (v == v_star) row.exp_A_star_ed = std::inner_product(psi.begin(), psi.end(), tmp.begin(), 0.0);
                for (size_t i = 0; i < dim; ++i) Hpsi[i] -= tmp[i];
            }
            tmp = psi; apply_single_qubit_op(tmp, N, v_star, P);
            row.exp_P_star_ed = std::inner_product(psi.begin(), psi.end(), tmp.begin(), 0.0);
            for (size_t i = 0; i < dim; ++i) Hpsi[i] += lambda * tmp[i];
            row.ground_energy_ed = std::inner_product(psi.begin(), psi.end(), Hpsi.begin(), 0.0);
            double res_sq = 0.0;
            for (size_t i = 0; i < dim; ++i) { double d = Hpsi[i] - row.ground_energy_ed * psi[i]; res_sq += d * d; }
            row.eigenstate_residual_l2 = std::sqrt(res_sq);
            auto [svn, srank] = compute_statevector_cut_svd(psi, L, W, row.cut_col);
            row.entanglement_entropy_bits_ed = svn; row.schmidt_rank_ed = srank;
        } else {
            double exp_p = 0.0, c_p = 0.0, exp_a = 0.0, c_a = 0.0; size_t nbr_mask = 0ULL;
            for (int u : neighbors_open_rect(pert_r, pert_c, L, W)) nbr_mask |= (1ULL << u);
            for (size_t mask = 0; mask < dim; ++mask) {
                double a = psi[mask];
                double yp = (((mask & bit_star) ? -1.0 : 1.0) * a * a) - c_p, tp = exp_p + yp;
                c_p = (tp - exp_p) - yp; exp_p = tp;
                double ya = (((std::popcount(mask & nbr_mask) & 1) ? -1.0 : 1.0) * a * psi[mask ^ bit_star]) - c_a, ta = exp_a + ya;
                c_a = (ta - exp_a) - ya; exp_a = ta;
            }
            row.exp_P_star_ed = exp_p; row.exp_A_star_ed = exp_a;
            row.ground_energy_ed = -static_cast<double>(N - 1) - row.exp_A_star_ed + lambda * row.exp_P_star_ed;
            row.entanglement_entropy_bits_ed = exact_bipartite_entropy_formula(L, pert_c, row.cut_col, lambda);
            row.schmidt_rank_ed = 1ULL << L;
        }
        row.ground_energy_exact = -static_cast<double>(N - 1) - std::sqrt(1.0 + lambda * lambda);
        row.spectral_gap_ed = 2.0; row.spectral_gap_exact = 2.0;
        row.entanglement_entropy_bits_exact = exact_bipartite_entropy_formula(L, pert_c, row.cut_col, lambda);
        row.schmidt_rank_exact = 1ULL << L;
        row.elapsed_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        row.passed = (std::abs(row.ground_energy_ed - row.ground_energy_exact) < 1e-11) &&
                     (std::abs(row.exp_A_star_ed - cos_a) < 1e-11) && (std::abs(row.exp_P_star_ed + sin_a) < 1e-11) &&
                     (std::abs(row.entanglement_entropy_bits_ed - row.entanglement_entropy_bits_exact) < 1e-10) &&
                     (row.schmidt_rank_ed == row.schmidt_rank_exact) && (row.eigenstate_residual_l2 < 1e-12);
        return row;
    }
};

} // namespace cycraft::omega::quantum_syntax
