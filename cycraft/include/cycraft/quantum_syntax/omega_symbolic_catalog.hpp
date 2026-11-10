#pragma once
// ============================================================================
// SINGULAR Ω — Quantum-Mathematical Computational Syntax (`v9.0.0-REACTOR-INF`)
// Gate Q01, Q02 & RC-03: 40-Operator Symbolic Catalog (`Ω∞` Higher Math),
// UTF-8/NFC Canonicalizer, Confusable/Eywa Guard & Symbolic <-> ASCII Bijection
// Layer: L1 Quantum Symbolic Syntax (`omega_symbolic_catalog.hpp`)
// Enforces Constitutional Rule `SG-ARCH-400` (<= 400 physical lines).
// ============================================================================

#include <cstdint>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace cycraft::omega::quantum_syntax {

enum class OmegaSymbolKind : uint8_t {
    STATE_BRACKET_OPEN,    // ⟦  or [[
    STATE_BRACKET_CLOSE,   // ⟧  or ]]
    SYSTEM_OMEGA,          // Ω  or @@OMEGA@@
    BIND_LEFT_ARROW,       // ←  or <-
    MPS_TENSOR_NET,        // ⨂_MPS or <MPS_KRON>
    TENSOR_MAP_OR_KRON,    // ⊗  or <*>
    COMPOSE_CIRCLE,        // ∘  or <o>
    INNER_CONTRACT_DOT,    // ⋅  or <.>
    CAUSAL_DELTA_OP,       // Δ  or <DELTA>
    COVARIANT_NABLA_G,     // ∇_g or <NABLA_G>
    GRADIENT_NABLA_OP,     // ∇  or <NABLA>
    RETRACT_CURVE_LEFT,    // ↶  or <RETRACT>
    TROPICAL_OPLUS,        // ⊕_trop or <OPLUS_TROP>
    SPARSE_INJECT_OPLUS,   // ⊕  or <+>
    PARALLEL_BARS,         // ∥  or <||>
    PROOF_TURNSTILE,       // ⊢  or |-
    APPROX_EQUAL,          // ≈  or <APPROX>
    ACT_RELU_PHI_PLUS,     // φ₊ or <PHI+>
    ACT_SIGMOID_PHI_SIG,   // φσ or <PHIS>
    ACT_TANH_PHI_TAU,      // φτ or <PHIT>
    ACT_GELU_PHI_G,        // φg or <PHIG>
    FIELD_REAL_R,          // ℝ  or <REAL>
    FIELD_COMPLEX_C,       // ℂ  or <COMPLEX>
    DIRAC_KET_CLOSE,       // ⟩  or |>
    DIRAC_BRA_OPEN,        // ⟨  or <|
    QUANTUM_MEASURE_M,     // ℳ  or <MEAS>
    SYMBIOTIC_MUTUAL_JOIN, // ⋈  or <JOIN>
    REACTOR_OSR_PROMOTE,   // ⚡ or <OSR>
    SCRAM_ANCHOR_GUARD,    // ⛨  or <SCRAM>
    MULTISCALE_BRIDGE_OP,  // ↦  or <SCALE>
    HERMITIAN_DAGGER_ADJ,  // †  or <DAGGER>
    QUANTUM_PARTIAL_TRACE, // Tr( or <TRACE>(
    SERIES_SUM_INF,        // ∑∞ or <SUM_INF>
    INF_HORIZON,           // ∞  or <INF>
    EXTERIOR_DERIV_D,      // d( or <EXT_D>(
    WEDGE_PRODUCT,         // ∧  or <WEDGE>
    HODGE_STAR,            // ⋆  or <HODGE>
    CONTOUR_INTEGRAL,      // ∮  or <OINT>
    SHEAF_COHOMOLOGY_H1,   // H¹_Sheaf or <H1_SHEAF>
    COUNTERDIABATIC_H_CD,  // ℋ_CD or <H_CD>
    TRANSFER_MATRIX_TM,    // ⨂_TM or <TM_KRON>
    FRUSTRATED_ISING_HZ,   // ℋ_ℤ or <H_ISING>
    UNITARY_EXP_PI8,       // 𝒰_π8 or <U_PI8>
    CYCLOTOMIC_RING_Z,     // ℤ  or <Z_RING>
    TOROIDAL_LATTICE_T,    // 𝕋  or <TORUS>
    SECRET_SEAL_TYPE,      // ⚿  or <SECRET>
    BIJECTIVE_PROOF_OP,    // ⇄  or <BIJ>
    HEADER_CONTRACT_SECT,  // §  or <SECT>
    CYCLOTOMIC_ROPE_OP,    // ⊛_θ or <ROPE_THETA>
    SELECTIVE_SSM_SCAN,    // ⟐_S or <SSM_SCAN>
    SIGMA_ORTHO_PROJ,      // ⊥_Σ or <ORTHO_SIGMA>
    SHEAF_SPEC_DECODE,     // ◈_H or <SHEAF_SPEC>
    STABILIZER_STAR_A,     // 𝒜_★ or <STAR_A>
    GAUGE_ROT_P_THETA,     // 𝒫_θ or <ROT_P>
    ANTICOMM_DEFECT_Q,     // 𝒬_θ or <DEFECT_Q>
    PERTURBED_CLUSTER_HL,  // ℋ_λ or <H_LAMBDA>
    ENTANGLEMENT_SVN,      // 𝒮_vN or <S_VN>
    SCHMIDT_RANK_RSCH,     // ℜ_Sch or <R_SCH>
    NONCOMMUTING_HAM_HQ,   // ℋ_ℚ or <H_QUANTUM>
    AUTO_EIGENSOLVE_EL,    // ℰ_λ or <EIG_SOLVE>
    SAFETENSORS_INTROSPECT,// ⟁_ST or <SAFETENSORS>
    MODEL_SYNTHESIZE_QCY,  // ⟁_SYN or <QCY_SYNTH>
    SINGULAR_DB_HYPERINDEX,// ⟁_DB or <SINGULAR_DB>
    ORCHESTRATOR_SUPERLEARN,// ⟁_ORC or <ORCHESTRATOR>
    OTZAR_VISUAL_GOVERNANCE,// ⟁_OTZ or <OTZAR_C2>
    SELF_HEALING_TRY_HEAL, // ⟁_HEAL or <TRY_HEAL>
    ATOMIC_CAUSAL_HYPERGRAPH,// ⟁_ACG or <CAUSAL_GRAPH>
    ADAPTIVE_MULTISCALE_COMP,// ⟁_AMC or <MULTISCALE>
    META_COMPILER_LEX_OP,    // ⟁_LEX or <META_LEX>
    META_COMPILER_AST_OP,    // ⟁_AST or <META_AST>
    META_COMPILER_CIR_OP,    // ⟁_CIR or <META_CIR>
    META_COMPILER_EMIT_OP,   // ⟁_EMIT or <META_EMIT>
    MLX_METAL_GPU_OP,        // ⟁_MLX or <MLX_METAL>
    BNNS_AMX_OPT_OP,         // ⟁_BNNS or <BNNS_AMX>
    HNSW_GRAPH_SEARCH_OP,    // ⟁_HNSW or <HNSW_VEC>
    SPARSE_MOE_UPCYCLE_OP,   // ⟁_MOE or <SPARSE_MOE>
    SARC_CVC_SIMPLEX_OP,     // ⟁_SARC or <SARC_CVC>
    SINGULAR_SPECTRAL_EDGE_OP,// ⟁_EDGE or <SING_EDGE>
    FUSION_TOKAMAK_CBF_OP,   // ⟁_FUS or <FUSION_CBF>
    OPENSOURCE_8_ARC_SYNTH_OP,// ⟁_ARC8 or <ARC8_SYNTH>
    ANAMORPHIC_OPTICS_CAM_OP,// ⟁_CAM or <ANAMORPH_CAM>
    PBR_GGX_WET_BRDF_OP,     // ⟁_PBR or <PBR_GGX_WET>
    VOLUMETRIC_MIE_RAIN_OP,  // ⟁_VOL or <VOL_SCATTER>
    SPATIAL_CINEMA_AUDIO_OP, // ⟁_AUD or <SPATIAL_AUD>
    RELATIVISTIC_PIC_ACC_OP, // ⟁_ACC or <PARTICLE_ACC>
    REACTIVE_MOL_DYNAMICS_OP,// ⟁_RMD or <REACTIVE_MD>
    UNIVERSAL_CAUSAL_EXEC_OP,// ⟁_CIE or <CAUSAL_EXEC>
    MILLENNIUM_FRONTIER_OP,  // ⟁_FRONT or <MATH_FRONTIER>
    SILICON_CAUSAL_RTL_OP,   // ⟁_RTL or <SILICON_RTL>
    OMEGA_RISCV_ISA_OP,      // ⟁_ISA or <OMEGA_ISA>
    NEAR_MEMORY_SRAM_OP,     // ⟁_NMC or <NEAR_MEM>
    QUANTUM_INSPIRED_HUBO_OP // ⟁_QIO or <QUANTUM_OPT>
};

struct SymbolicOperatorSpec {
    OmegaSymbolKind kind; const char* symbolic_utf8; const char* ascii_canonical;
    const char* abi_mangled_name; const char* algebraic_signature; const char* semantics_summary;
};

struct ConfusableDiagnostic {
    bool has_confusable_error = false, has_forbidden_english_kw = false;
    size_t line = 0, column = 0; std::string code, message;
};

class OmegaSymbolicCatalog {
public:
    static const std::vector<SymbolicOperatorSpec>& catalog() {
        // Ordered longest-token-first where prefixes overlap (`∑∞` before `∞`, `∇_g` before `∇`, `⊕_trop` before `⊕`)
        static const std::vector<SymbolicOperatorSpec> kCatalog = {
            {OmegaSymbolKind::STATE_BRACKET_OPEN,    "⟦",        "[[",           "__omega_state_open",   "⟦ id : Field[Shape] ⟧",                "Declare 64B-aligned State Manifold"},
            {OmegaSymbolKind::STATE_BRACKET_CLOSE,   "⟧",        "]]",           "__omega_state_close",  "⟦ id : Field[Shape] ⟧",                "Close State Manifold declaration"},
            {OmegaSymbolKind::SYSTEM_OMEGA,          "Ω",        "@@OMEGA@@",    "__omega_system_block", "Ω[Decls] { Transitions }",             "Quantum-Causal Computational System"},
            {OmegaSymbolKind::BIND_LEFT_ARROW,       "←",        "<-",           "__omega_bind_state",   "Ψ_next ← Transformation(Ψ)",           "Immutable versioned state transition"},
            {OmegaSymbolKind::MPS_TENSOR_NET,        "⨂_MPS",    "<MPS_KRON>",   "__omega_mps_tt_svd",   "A ⨂_MPS B -> CompressedMPS[r]",        "Matrix Product State / TT-SVD contraction"},
            {OmegaSymbolKind::TENSOR_MAP_OR_KRON,    "⊗",        "<*>",          "__omega_tensor_map",   "T ⊗ X (map) | U₁ ⊗ U₂ (Kronecker)",    "Type-directed Vector Map or Kronecker"},
            {OmegaSymbolKind::COMPOSE_CIRCLE,        "∘",        "<o>",          "__omega_compose",      "(F ∘ G)(x) => F(G(x))",                "Fused functional transformation chain"},
            {OmegaSymbolKind::INNER_CONTRACT_DOT,    "⋅",        "<.>",          "__omega_contract_dot", "ℝ[B,K] ⋅ ℝ[K,N] -> ℝ[B,N]",            "Typed tensor/matrix inner contraction"},
            {OmegaSymbolKind::CAUSAL_DELTA_OP,       "Δ",        "<DELTA>",      "__omega_frechet_delta","ΔF(S, δ) -> SparseDelta",              "Symbolic Frechet-Leibniz causal delta"},
            {OmegaSymbolKind::COVARIANT_NABLA_G,     "∇_g",      "<NABLA_G>",    "__omega_riemann_grad", "∇_g L(θ) = F(θ)⁻¹ ∇L(θ)",              "Riemannian Fisher-Rao natural gradient"},
            {OmegaSymbolKind::GRADIENT_NABLA_OP,     "∇",        "<NABLA>",      "__omega_nabla_vjp",    "∇L(θ) -> Cotangent[Shape]",            "Reverse-mode VJP / Parameter-Shift grad"},
            {OmegaSymbolKind::RETRACT_CURVE_LEFT,    "↶",        "<RETRACT>",    "__omega_retract_inv",  "S₁ ↶ δ -> S₀",                         "Exact 0-Log or Boundary-Log retraction"},
            {OmegaSymbolKind::TROPICAL_OPLUS,        "⊕_trop",   "<OPLUS_TROP>", "__omega_tropical_min", "(A ⊕_trop B)_ij = min_k(A_ik + B_kj)", "Tropical Min-Plus Semiring Bellman op"},
            {OmegaSymbolKind::SPARSE_INJECT_OPLUS,   "⊕",        "<+>",          "__omega_sparse_inject","State[N] ⊕ SparseDelta[K] -> State[N]","Causal sparse delta state injection"},
            {OmegaSymbolKind::PARALLEL_BARS,         "∥",        "<||>",         "__omega_parallel_ind", "OpA ∥ OpB (disjoint regions)",         "Verified lock-free parallel execution"},
            {OmegaSymbolKind::PROOF_TURNSTILE,       "⊢",        "|-",           "__omega_proof_oblig",  "⊢ Predicate(Ψ) -> ProofCert",          "Compile + runtime cryptographic proof"},
            {OmegaSymbolKind::APPROX_EQUAL,          "≈",        "<APPROX>",     "__omega_approx_eq",    "A ≈ B (||A - B||_inf <= 1e-12)",       "Bounded numerical equivalence relation"},
            {OmegaSymbolKind::ACT_RELU_PHI_PLUS,     "φ₊",       "<PHI+>",       "__omega_act_relu",     "φ₊(x) = max(x, 0)",                    "Built-in Operator #1: Fused ReLU"},
            {OmegaSymbolKind::ACT_SIGMOID_PHI_SIG,   "φσ",       "<PHIS>",       "__omega_act_sigmoid",  "φσ(x) = 1 / (1 + exp(-x))",            "Built-in Operator #2: Fused Sigmoid"},
            {OmegaSymbolKind::ACT_TANH_PHI_TAU,      "φτ",       "<PHIT>",       "__omega_act_tanh",     "φτ(x) = tanh(x)",                      "Built-in Operator #3: Fused Tanh"},
            {OmegaSymbolKind::ACT_GELU_PHI_G,        "φg",       "<PHIG>",       "__omega_act_gelu",     "φg(x) = 0.5*x*(1 + erf(x/sqrt(2)))",   "Built-in Operator #4: Fused GELU"},
            {OmegaSymbolKind::FIELD_REAL_R,          "ℝ",        "<REAL>",       "__omega_field_f64",    "ℝ[d₁, ..., dᵣ]",                       "IEEE-754 FP64 real tensor manifold"},
            {OmegaSymbolKind::FIELD_COMPLEX_C,       "ℂ",        "<COMPLEX>",    "__omega_field_c128",   "ℂ[2^n]",                               "Split-complex SoA quantum manifold"},
            {OmegaSymbolKind::DIRAC_KET_CLOSE,       "⟩",        "|>",           "__omega_dirac_ket",    "|b₀b₁...bₙ₋₁⟩ -> QuantumState[n]",     "Dirac computational basis ket literal"},
            {OmegaSymbolKind::DIRAC_BRA_OPEN,        "⟨",        "<|",           "__omega_dirac_bra",    "⟨Ψ|M|Ψ⟩ -> ℝ",                         "Hermitian expectation value bracket"},
            {OmegaSymbolKind::QUANTUM_MEASURE_M,     "ℳ",        "<MEAS>",       "__omega_measure_born", "ℳ(Ψ) -> Bit[n]",                       "Born-rule projective measurement"},
            {OmegaSymbolKind::SYMBIOTIC_MUTUAL_JOIN, "⋈",        "<JOIN>",       "__omega_symbiotic_join","Ψ_sym ← A ⋈ B",                       "Symbiotic mutualistic coupling & join"},
            {OmegaSymbolKind::REACTOR_OSR_PROMOTE,   "⚡",       "<OSR>",        "__omega_reactor_osr",  "Ψ_jit ← ⚡(Ψ_vm, k_eff>=1.0)",         "Profile-guided OSR frame transfer to JIT"},
            {OmegaSymbolKind::SCRAM_ANCHOR_GUARD,    "⛨",        "<SCRAM>",      "__omega_scram_guard",  "Ψ_safe ← ⛨(Ψ_hot, Ψ_anchor)",          "Snapshot-anchored 0-ULP SCRAM guard"},
            {OmegaSymbolKind::MULTISCALE_BRIDGE_OP,  "↦",        "<SCALE>",      "__omega_scale_bridge", "U_meso ← U_atomic ↦ Meso_ns",          "Mass-conserving multi-scale bridge"},
            {OmegaSymbolKind::HERMITIAN_DAGGER_ADJ,  "†",        "<DAGGER>",     "__omega_hermitian_adj","U† ⋅ U ≈ I",                           "Hermitian conjugate / unitary adjoint"},
            {OmegaSymbolKind::QUANTUM_PARTIAL_TRACE, "Tr(",      "<TRACE>(",     "__omega_quantum_trace","⊢ Tr(ρ) ≈ 1.0",                        "Quantum density-matrix trace operator"},
            {OmegaSymbolKind::SERIES_SUM_INF,        "∑∞",       "<SUM_INF>",    "__omega_series_inf",   "Ψ_∞ ← ∑∞(c_k) ⊢ Cauchy(Ψ_∞)",          "Infinite-horizon convergent series"},
            {OmegaSymbolKind::INF_HORIZON,           "∞",        "<INF>",        "__omega_inf_manifold", "⟦ S_∞ : ℝ[∞] ⟧",                       "Coinductive infinite-horizon manifold"},
            {OmegaSymbolKind::EXTERIOR_DERIV_D,      "d(",       "<EXT_D>(",     "__omega_exterior_d",   "d(ω) : Ω^k -> Ω^(k+1), d(d(ω)) == 0",  "Exterior derivative with d^2 = 0 guard"},
            {OmegaSymbolKind::WEDGE_PRODUCT,         "∧",        "<WEDGE>",      "__omega_wedge_prod",   "α ∧ β : Ω^p × Ω^q -> Ω^(p+q)",         "Antisymmetric exterior wedge product"},
            {OmegaSymbolKind::HODGE_STAR,            "⋆",        "<HODGE>",      "__omega_hodge_star",   "⋆ω : Ω^k -> Ω^(n-k)",                  "Riemannian discrete Hodge star dual"},
            {OmegaSymbolKind::CONTOUR_INTEGRAL,      "∮",        "<OINT>",       "__omega_stokes_oint",  "∮_∂M ω == ∫_M d(ω)",                   "Generalized Stokes manifold integral"},
            {OmegaSymbolKind::SHEAF_COHOMOLOGY_H1,   "H¹_Sheaf", "<H1_SHEAF>",   "__omega_sheaf_lapl",   "E_F(x) = x^T L_F x == 0",              "Cellular Sheaf cohomology consistency"},
            {OmegaSymbolKind::COUNTERDIABATIC_H_CD,  "ℋ_CD",     "<H_CD>",       "__omega_agp_cd_drive", "ℋ_CD(Ψ) = iℏ[∂_λ P, P] Ψ",            "Counterdiabatic Adiabatic Gauge Drive"},
            {OmegaSymbolKind::TRANSFER_MATRIX_TM,    "⨂_TM",     "<TM_KRON>",    "__omega_cyclotomic_tm","⟨ψ₀|𝒰|ψ₀⟩ ⨂_TM Λ -> ℤ[ζ₁₆]",          "Exact Cyclotomic Row/Butterfly Transfer Matrix"},
            {OmegaSymbolKind::FRUSTRATED_ISING_HZ,   "ℋ_ℤ",      "<H_ISING>",    "__omega_ising_frust",  "ℋ_ℤ(Λ, J, h) -> DiagHamiltonian",      "Frustrated Toroidal Ising Diagonal Hamiltonian"},
            {OmegaSymbolKind::UNITARY_EXP_PI8,       "𝒰_π8",     "<U_PI8>",      "__omega_unitary_pi8",  "𝒰_π8(ℋ) = exp(iπℋ/8)",                 "Exact Cyclotomic ζ₁₆ = exp(iπ/8) Unitary"},
            {OmegaSymbolKind::CYCLOTOMIC_RING_Z,     "ℤ",        "<Z_RING>",     "__omega_ring_z_zeta16","ℤ[ζ₁₆, N] = ℤ[ζ]/(ζ⁸+1)",             "Exact 16th Cyclotomic Integer Ring Manifold"},
            {OmegaSymbolKind::TOROIDAL_LATTICE_T,    "𝕋",        "<TORUS>",      "__omega_torus_lattice","𝕋[L, L] (periodic mod L)",             "2D Periodic Frustrated Toroidal Spin Lattice"},
            {OmegaSymbolKind::SECRET_SEAL_TYPE,      "⚿",        "<SECRET>",     "__omega_secret_seal",  "⚿[32] (ML-KEM-768/XChaCha20)",         "Sealed Post-Quantum Cryptographic Manifold"},
            {OmegaSymbolKind::BIJECTIVE_PROOF_OP,    "⇄",        "<BIJ>",        "__omega_bijective_prf","⊢ ⇄(Ψ₀, Ψ₁)",                          "Exact 0-ULP Algebraic Reversibility Proof"},
            {OmegaSymbolKind::HEADER_CONTRACT_SECT,  "§",        "<SECT>",       "__omega_header_sect",  "§λ / §ω / §κ",                         "Pure Symbolic Constitutional Header Directive"},
            {OmegaSymbolKind::CYCLOTOMIC_ROPE_OP,    "⊛_θ",      "<ROPE_THETA>", "__omega_cyclotomic_rope","Q ⊛_θ K -> ℝ[B,H,S,S]",              "0-ULP Cyclotomic Z[ζ₁₆] Rotary Phase-Coherent Attention"},
            {OmegaSymbolKind::SELECTIVE_SSM_SCAN,    "⟐_S",      "<SSM_SCAN>",   "__omega_parallel_ssm", "⟐_S(A, B, Ψ) -> ℝ[B,S,D]",             "Associative Symplectic Mamba-2/RWKV-7 State-Space Scan"},
            {OmegaSymbolKind::SIGMA_ORTHO_PROJ,      "⊥_Σ",      "<ORTHO_SIGMA>","__omega_sigma_x_ortho","W_1 ← W_0 ⊥_Σ ΔW_0",                   "Zero-Forgetting Σ_X Null-Space Orthogonal Weight Update"},
            {OmegaSymbolKind::SHEAF_SPEC_DECODE,     "◈_H",      "<SHEAF_SPEC>", "__omega_sheaf_spec",   "◈_H(Ψ_0, Ψ_1) -> ℝ[B,V]",              "Sheaf-Cohomology H¹ Verified Speculative & Diffusion Decode"},
            {OmegaSymbolKind::STABILIZER_STAR_A,     "𝒜_★",      "<STAR_A>",     "__omega_stabilizer_a", "𝒜_★(Λ, v) = X_v ∏ Z_u",                "2D Cluster-State Symplectic Stabilizer Star Generator"},
            {OmegaSymbolKind::GAUGE_ROT_P_THETA,     "𝒫_θ",      "<ROT_P>",      "__omega_gauge_rot_p",  "𝒫_θ(v) = cos(θ)X + sin(θ)Y∏Z",         "Commuting SO(2)-Rotated Pauli Stabilizer Operator"},
            {OmegaSymbolKind::ANTICOMM_DEFECT_Q,     "𝒬_θ",      "<DEFECT_Q>",   "__omega_anticomm_q",   "𝒬_θ(v) = -sin(θ)X + cos(θ)Y∏Z",        "Anticommuting Pauli Defect Perturbation Operator"},
            {OmegaSymbolKind::PERTURBED_CLUSTER_HL,  "ℋ_λ",      "<H_LAMBDA>",   "__omega_perturbed_hl", "ℋ_λ(Λ, θ, λ) -> StabilizerIR",         "Perturbed Stabilizer Many-Body Entanglement Hamiltonian"},
            {OmegaSymbolKind::ENTANGLEMENT_SVN,      "𝒮_vN",     "<S_VN>",       "__omega_bipartite_svn","𝒮_vN(Ψ, ∂A) = -Tr(ρ_A log₂ ρ_A)",      "Exact Bipartite von Neumann Entanglement Entropy"},
            {OmegaSymbolKind::SCHMIDT_RANK_RSCH,     "ℜ_Sch",    "<R_SCH>",      "__omega_schmidt_rank", "ℜ_Sch(Ψ, ∂A) = rank(ρ_A)",             "Exact Bipartite Schmidt Rank Across Cut"},
            {OmegaSymbolKind::NONCOMMUTING_HAM_HQ,   "ℋ_ℚ",      "<H_QUANTUM>",  "__omega_noncomm_ham",  "ℋ_ℚ(Λ, J_ℤ, h_𝕏, g_ℤ) -> Ω-QIR",       "General Noncommuting Pauli Hamiltonian (ZZ + X + Z)"},
            {OmegaSymbolKind::AUTO_EIGENSOLVE_EL,    "ℰ_λ",      "<EIG_SOLVE>",  "__omega_auto_eigsolve","ℰ_λ(ℋ_ℚ) -> (E₀, E₁, |ψ₀⟩, ℳ_𝕏, ℳ_ℤ)", "Structure-Directed Automatic Quantum Solver Engine"},
            {OmegaSymbolKind::SAFETENSORS_INTROSPECT,"⟁_ST",     "<SAFETENSORS>","__omega_safetensors",  "⟁_ST(W) -> ⛨ℝ[M, N]",                  "Native Binary Safetensors Header & Zero-Copy Introspector"},
            {OmegaSymbolKind::MODEL_SYNTHESIZE_QCY,  "⟁_SYN",    "<QCY_SYNTH>",  "__omega_qcy_synth",    "⟁_SYN(Ψ, W) -> Ω ⟦ ... ⟧",              "Native C++23 Model Decompiler & 5-Stage .qcy Synthesizer"},
            {OmegaSymbolKind::SINGULAR_DB_HYPERINDEX,"⟁_DB",     "<SINGULAR_DB>","__omega_singular_db",  "⟁_DB(Ψ, K) -> ℝ[B, D]",                 "Distributed Big-Data B^ε-Tree, HDC/HRR & Cyclotomic HyperIndex"},
            {OmegaSymbolKind::ORCHESTRATOR_SUPERLEARN,"⟁_ORC",   "<ORCHESTRATOR>","__omega_orchestrator","⟁_ORC(Ψ_0, Ψ_1) -> ℝ[B, D]",            "Autonomous Super-Learning DAG, PCD Gate & Tropical Council"},
            {OmegaSymbolKind::OTZAR_VISUAL_GOVERNANCE,"⟁_OTZ",   "<OTZAR_C2>",   "__omega_otzar_c2",     "⟁_OTZ(Ψ) -> ⚿[32]",                     "OTZAR Visual C2, Constitutional Governance & Merkle Vault"},
            {OmegaSymbolKind::SELF_HEALING_TRY_HEAL, "⟁_HEAL",   "<TRY_HEAL>",   "__omega_try_heal",     "⟁_HEAL(Ψ, δ) -> ℝ[N]",                  "Self-Healing try/heal Lyapunov & Nanoswarm Consensus"},
            {OmegaSymbolKind::ATOMIC_CAUSAL_HYPERGRAPH,"⟁_ACG",  "<CAUSAL_GRAPH>","__omega_causal_graph","⟁_ACG(U, Λ) -> ℝ[M, N]",                "Atomic Causal Hypergraph & Multi-Target Lowering (RTL/MZI/DNA)"},
            {OmegaSymbolKind::ADAPTIVE_MULTISCALE_COMP,"⟁_AMC",  "<MULTISCALE>", "__omega_multiscale",   "⟁_AMC(Ψ_α, Ψ_κ) -> ℝ[N]",               "Error-Bounded Adaptive Multiscale & Physics-Typed Solver"},
            {OmegaSymbolKind::META_COMPILER_LEX_OP,  "⟁_LEX",    "<META_LEX>",   "__omega_meta_lexer",   "⟁_LEX(ω, Ψ) -> ℤ[T, 4]",                "Self-Hosted UTF-8 Quantum-Symbolic Lexical Automaton"},
            {OmegaSymbolKind::META_COMPILER_AST_OP,  "⟁_AST",    "<META_AST>",   "__omega_meta_ast_sema","⟁_AST(𝒯, W) -> ℤ[V, 8]",                "Self-Hosted Algebraic AST & 16-Law Constitutional Sheaf Sema"},
            {OmegaSymbolKind::META_COMPILER_CIR_OP,  "⟁_CIR",    "<META_CIR>",   "__omega_meta_cir_lower","⟁_CIR(𝒜, Φ) -> ℤ[M, 12]",              "Self-Hosted SIR Ω / Ω-CIR Lowering & Algebraic Fusion"},
            {OmegaSymbolKind::META_COMPILER_EMIT_OP, "⟁_EMIT",   "<META_EMIT>",  "__omega_meta_code_emit","⟁_EMIT(Υ, Θ) -> ℝ[B, D]",              "Self-Hosted Native C++23 / QBE SSA / ARM64 Code Emitter"},
            {OmegaSymbolKind::MLX_METAL_GPU_OP,      "⟁_MLX",    "<MLX_METAL>",  "__omega_mlx_metal_gpu", "⟁_MLX(Ψ, W) -> ℝ[B, D]",               "Native Apple MLX C++ (libmlx.dylib + mlx.metallib) Unified-Memory GPU Tensor & Autograd"},
            {OmegaSymbolKind::BNNS_AMX_OPT_OP,       "⟁_BNNS",   "<BNNS_AMX>",   "__omega_bnns_amx_opt",  "⟁_BNNS(Q, K, V) -> ℝ[B, H, S, D]",     "Native Apple Accelerate BNNS AMX Fused Multi-Head Attention & Hardware AdamW Optimizer"},
            {OmegaSymbolKind::HNSW_GRAPH_SEARCH_OP,  "⟁_HNSW",   "<HNSW_VEC>",   "__omega_hnsw_search",   "⟁_HNSW(Ψ, K) -> ℝ[B, k]",              "Native C++ hnswlib + Eigen3 Hierarchical Navigable Small-World Vector Search & Spectral SVD"},
            {OmegaSymbolKind::SPARSE_MOE_UPCYCLE_OP, "⟁_MOE",    "<SPARSE_MOE>", "__omega_sparse_moe",    "⟁_MOE(Ψ, W) -> ℝ[B, D]",               "38-Model Sparse Mixture-of-Experts (MoE) Upcycling, GRPO Verifiable RL & Null-Space DoRA Merger"},
            {OmegaSymbolKind::SARC_CVC_SIMPLEX_OP,   "⟁_SARC",   "<SARC_CVC>",   "__omega_sarc_cvc",      "⟁_SARC(Ψ, δ) -> ⛨ℝ[N]",                "SARC 5-Valued Conditional Validity Certificate & Ternary Simplex Proof-Constrained Optimization"},
            {OmegaSymbolKind::SINGULAR_SPECTRAL_EDGE_OP,"⟁_EDGE","<SING_EDGE>",  "__omega_singular_edge", "⟁_EDGE(u, v) -> ℤ[8]",                 "SWP-SAS-SE 64B Spectral-Topological Singular Edge (Harary, Hodge-Helmholtz, p-adic AST, FTLE, Epoch)"},
            {OmegaSymbolKind::FUSION_TOKAMAK_CBF_OP, "⟁_FUS",    "<FUSION_CBF>", "__omega_fusion_cbf",    "⟁_FUS(Ψ_p, B_θ) -> ⛨ℝ[N]",             "Magnetic-Confinement D-T Nuclear Fusion Core (Lawson Q_fus, Symplectic Plasma, MHD CBF & 42ns SCRAM)"},
            {OmegaSymbolKind::OPENSOURCE_8_ARC_SYNTH_OP,"⟁_ARC8","<ARC8_SYNTH>", "__omega_arc8_synth",    "⟁_ARC8(S_0, Δ) -> ⛨ℝ[N]",              "8-OpenSource Incremental/Decremental/Harmonic Singular Arc × SIAC Distributed Synthesis"},
            {OmegaSymbolKind::ANAMORPHIC_OPTICS_CAM_OP,"⟁_CAM",  "<ANAMORPH_CAM>","__omega_anamorph_cam", "⟁_CAM(Ψ, θ) -> ⛨ℝ[H, W, 3]",           "Detuned ARRI ALFA 2x Anamorphic Optics, Elliptical Bokeh & ACEScg Filmic Color Science"},
            {OmegaSymbolKind::PBR_GGX_WET_BRDF_OP,   "⟁_PBR",    "<PBR_GGX_WET>","__omega_pbr_ggx_wet",   "⟁_PBR(N, V, L) -> ⛨ℝ[H, W, 3]",        "Dual-Layer Thin-Film Wet Asphalt Cook-Torrance GGX Microfacet BRDF & Khronos PBR Lookup"},
            {OmegaSymbolKind::VOLUMETRIC_MIE_RAIN_OP,"⟁_VOL",    "<VOL_SCATTER>","__omega_vol_scatter",   "⟁_VOL(R, B_0) -> ⛨ℝ[H, W, 3]",         "Anisotropic Henyey-Greenstein Volumetric Rain/Fog Ray-Marching & Bat-Signal Tyndall Cone"},
            {OmegaSymbolKind::SPATIAL_CINEMA_AUDIO_OP,"⟁_AUD",   "<SPATIAL_AUD>","__omega_spatial_aud",   "⟁_AUD(S_0, Δ) -> ⛨ℝ[T, 2]",            "48 kHz Stereo Spatial Film Score (Giacchino Minor-Third Motif) & Batmobile Turbine Foley"},
            {OmegaSymbolKind::RELATIVISTIC_PIC_ACC_OP,"⟁_ACC",   "<PARTICLE_ACC>","__omega_particle_acc", "⟁_ACC(Ψ_p, B_0) -> ⛨ℝ[N, 6]",          "Relativistic Symplectic Beam Dynamics, Esirkepov PIC & Riemannian Magnet Optimization"},
            {OmegaSymbolKind::REACTIVE_MOL_DYNAMICS_OP,"⟁_RMD",  "<REACTIVE_MD>","__omega_reactive_md",   "⟁_RMD(R_0, δ_b) -> ⛨ℝ[N, 3]",          "Multi-Scale Reactive QM/MM Catalysis, Transition-State ΔG‡ & Incremental Verlet Cone"},
            {OmegaSymbolKind::UNIVERSAL_CAUSAL_EXEC_OP,"⟁_CIE",  "<CAUSAL_EXEC>","__omega_causal_exec",   "⟁_CIE(S_0, Δ_K) -> ⛨ℝ[N]",             "150x Universal Causal Incremental Execution Engine (Ω-UCIEE) with AMX Crossover Guard"},
            {OmegaSymbolKind::MILLENNIUM_FRONTIER_OP,"⟁_FRONT",  "<MATH_FRONTIER>","__omega_math_frontier","⟁_FRONT(Ψ_0, ω_0) -> ⛨ℝ[N]",          "Navier-Stokes BKM, Riemann Critical-Line Z(t), Yang-Mills Gap & Alcubierre WEC Auditor"},
            {OmegaSymbolKind::SILICON_CAUSAL_RTL_OP, "⟁_RTL",    "<SILICON_RTL>", "__omega_silicon_rtl",  "⟁_RTL(Ψ_0, δ_0) -> ⛨ℝ[N]",             "SystemVerilog-2017 Causal Compute Unit (Ω-CCU) & Priority Wavefront Silicon Engine"},
            {OmegaSymbolKind::OMEGA_RISCV_ISA_OP,    "⟁_ISA",    "<OMEGA_ISA>",   "__omega_riscv_isa",    "⟁_ISA(Ψ_0, δ_0) -> ⛨ℤ[N]",             "RISC-V custom-3 (0x7B) 6-Op Ω-ISA & Reversible TwoSum/Cyclotomic Z[ζ₁₆] Hardware ALU"},
            {OmegaSymbolKind::NEAR_MEMORY_SRAM_OP,   "⟁_NMC",    "<NEAR_MEM>",    "__omega_near_mem_sram","⟁_NMC(Ψ_0, K_0) -> ⛨ℝ[N]",             "SARC Certificate-Governed Near-Memory Compute Fabric & 64B SWP-SAS-SE SRAM Crossbar"},
            {OmegaSymbolKind::QUANTUM_INSPIRED_HUBO_OP,"⟁_QIO",  "<QUANTUM_OPT>", "__omega_quantum_hubo", "⟁_QIO(Ψ_0, J_0) -> ⛨ℝ[N]",             "Quantum-Inspired Ballistic Cubic-HUBO dSB, Counterdiabatic AGP & 1-Cycle Hardware SCRAM"}
        };
        return kCatalog;
    }

    static uint64_t fast_fnv1a_64(const std::string& s) noexcept {
        uint64_t h = 14695981039346656037ULL;
        for (unsigned char c : s) { h ^= static_cast<uint64_t>(c); h *= 1099511628211ULL; }
        return h;
    }

    static std::string canonicalize_nfc(const std::string& input) {
        std::string out = input;
        if (out.find("\xE2\x84\xA6") != std::string::npos) replace_all(out, "\xE2\x84\xA6", "Ω");
        if (out.find("\xC2\xB7") != std::string::npos) replace_all(out, "\xC2\xB7", "⋅");
        if (out.find("\xE2\x82") != std::string::npos) {
            replace_all(out, "₀", "_0"); replace_all(out, "₁", "_1"); replace_all(out, "₂", "_2"); replace_all(out, "₃", "_3");
        }
        return out;
    }

    static ConfusableDiagnostic audit_symbolic_purity(const std::string& src) {
        ConfusableDiagnostic diag{};
        size_t bad_pos = src.find("\xCD\xBE");
        if (bad_pos != std::string::npos) {
            diag.has_confusable_error = true; diag.line = count_lines_up_to(src, bad_pos);
            diag.code = "CY-SYM-CONFUSABLE"; diag.message = "Deceptive Unicode homoglyph U+037E (Greek Question Mark) detected instead of ';'";
            return diag;
        }
        size_t cyr_pos = src.find("\xD0\x90");
        if (cyr_pos != std::string::npos) {
            diag.has_confusable_error = true; diag.line = count_lines_up_to(src, cyr_pos);
            diag.code = "CY-SYM-CONFUSABLE"; diag.message = "Deceptive Cyrillic homoglyph U+0410 detected in identifier";
            return diag;
        }

        static const char* kForbiddenEnglish[] = {
            "for ", "while ", "def ", "class ", "return ", "if ", "else ", "switch ",
            "async ", "await ", "import ", "try:", "catch ", "throw ", "var ", "let ", "fn ", "function "
        };
        static const char* kLegacyCyKeywords[] = {
            "form ", "state ", "arc ", "flow ", "law ", "mind ", "service ", "endpoint "
        };
        static const char* kUtf8WithAsciiLetters[] = {
            "H¹_Sheaf", "⊕_trop", "⨂_MPS", "⨂_TM", "𝒰_π8", "ℋ_CD", "𝒮_vN", "ℜ_Sch", "∇_g", "Tr(", "d(", "φg"
        };
        std::istringstream iss(src);
        std::string line;
        size_t line_no = 0;
        while (std::getline(iss, line)) {
            ++line_no;
            size_t cpos = line.find("//");
            std::string code_part = (cpos == std::string::npos) ? line : line.substr(0, cpos);
            size_t hpos = code_part.find('#');
            if (hpos != std::string::npos) code_part = code_part.substr(0, hpos);
            for (const char* kw : kForbiddenEnglish) {
                if (code_part.find(kw) != std::string::npos) {
                    diag.has_forbidden_english_kw = true;
                    diag.line = line_no;
                    diag.code = "CY-SYM-NO-ENGLISH-KW";
                    diag.message = std::string("Forbidden English imperative keyword in Ω Symbolic Core: '") + kw + "'";
                    return diag;
                }
            }
            for (const char* lkw : kLegacyCyKeywords) {
                if (code_part.find(lkw) != std::string::npos) {
                    diag.has_forbidden_english_kw = true;
                    diag.line = line_no;
                    diag.code = "CY-RC-013-LEGACY-CY-FORBIDDEN";
                    diag.message = std::string("Forbidden legacy .cy structural keyword in .qcy module: '") + lkw + "'";
                    return diag;
                }
            }
            std::string unquoted;
            unquoted.reserve(code_part.size());
            bool in_quotes = false;
            for (char ch : code_part) {
                if (ch == '"') { in_quotes = !in_quotes; continue; }
                if (!in_quotes) unquoted.push_back(ch);
            }
            if (unquoted.find("eywa") != std::string::npos || unquoted.find("Eywa") != std::string::npos) {
                diag.has_forbidden_english_kw = true;
                diag.line = line_no;
                diag.code = "CY-RC-003-EYWA-FORBIDDEN";
                diag.message = "Forbidden language identifier 'Eywa' violated SINGULAR-QUANTUM-EXCLUSIVE-001";
                return diag;
            }
            if (unquoted.find("d(d(") != std::string::npos && unquoted.find("!= 0") != std::string::npos) {
                diag.has_confusable_error = true;
                diag.line = line_no;
                diag.code = "CY-MATH-DD-ZERO";
                diag.message = "Exterior calculus topological violation: d(d(ω)) must identically equal 0";
                return diag;
            }
            // Fast-path SIAC Prefix-Guided Operator Strip (11 tokens + ⟁_ prefix instead of 168 scans/line)
            size_t tri_pos = 0;
            while ((tri_pos = unquoted.find("⟁_", tri_pos)) != std::string::npos) {
                size_t end_p = tri_pos + std::string("⟁_").size();
                while (end_p < unquoted.size() &&
                       ((unquoted[end_p] >= 'A' && unquoted[end_p] <= 'Z') ||
                        (unquoted[end_p] >= '0' && unquoted[end_p] <= '9'))) ++end_p;
                unquoted.replace(tri_pos, end_p - tri_pos, " ");
            }
            for (const char* u_tok : kUtf8WithAsciiLetters) {
                if (unquoted.find(u_tok) != std::string::npos) replace_all(unquoted, u_tok, " ");
            }
            if (unquoted.find('<') != std::string::npos || unquoted.find("@@") != std::string::npos) {
                for (const auto& spec : catalog()) replace_all(unquoted, spec.ascii_canonical, " ");
                for (const char* asc_tok : {"<PSI>", "<D_VAR>", "<TH_VAR>", "<S0>", "<S1>"}) replace_all(unquoted, asc_tok, " ");
            }
            std::string cur_word;
            for (size_t idx = 0; idx <= unquoted.size(); ++idx) {
                unsigned char uch = (idx < unquoted.size()) ? static_cast<unsigned char>(unquoted[idx]) : 0;
                if ((uch >= 'A' && uch <= 'Z') || (uch >= 'a' && uch <= 'z')) {
                    cur_word.push_back(static_cast<char>(uch));
                } else {
                    if (cur_word.size() >= 2) {
                        diag.has_forbidden_english_kw = true;
                        diag.line = line_no;
                        diag.code = "CY-RC-014-ENGLISH-TOKEN-IN-QCY";
                        diag.message = "Forbidden English word token in pure Quantum-Symbolic .qcy code: '" + cur_word + "'";
                        return diag;
                    }
                    cur_word.clear();
                }
            }
        }
        return diag;
    }

    static std::string to_ascii_canonical(const std::string& symbolic_src) {
        std::string out = symbolic_src;
        for (const auto& spec : catalog()) replace_all(out, spec.symbolic_utf8, spec.ascii_canonical);
        replace_all(out, "Ψ", "<PSI>"); replace_all(out, "δ", "<D_VAR>"); replace_all(out, "θ", "<TH_VAR>");
        replace_all(out, "₀", "<S0>"); replace_all(out, "₁", "<S1>");
        return out;
    }

    static std::string to_symbolic_canonical(const std::string& ascii_src) {
        if (ascii_src.find('<') == std::string::npos && ascii_src.find("[[") == std::string::npos &&
            ascii_src.find("]]") == std::string::npos && ascii_src.find("@@") == std::string::npos &&
            ascii_src.find("|-") == std::string::npos && ascii_src.find("|>") == std::string::npos) return ascii_src;
        std::string out = ascii_src;
        for (const auto& spec : catalog()) replace_all(out, spec.ascii_canonical, spec.symbolic_utf8);
        replace_all(out, "<PSI>", "Ψ"); replace_all(out, "<D_VAR>", "δ"); replace_all(out, "<TH_VAR>", "θ");
        replace_all(out, "<S0>", "₀"); replace_all(out, "<S1>", "₁");
        return out;
    }

private:
    static void replace_all(std::string& str, const std::string& from, const std::string& to) {
        if (from.empty()) return;
        for (size_t p = 0; (p = str.find(from, p)) != std::string::npos; p += to.length()) str.replace(p, from.length(), to);
    }

    static size_t count_lines_up_to(const std::string& s, size_t pos) {
        size_t lines = 1;
        for (size_t i = 0; i < pos && i < s.size(); ++i) if (s[i] == '\n') ++lines;
        return lines;
    }
};

} // namespace cycraft::omega::quantum_syntax
