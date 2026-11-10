#pragma once
// ============================================================================
// CYCRAFT Ω — The Computational Reality Language (`Code Reality.`)
// Layer: L1 Language Multi-Dimensional Semantic AST (`cycraft_ast.hpp`)
// Enforces Constitutional Rule `SG-ARCH-400` (<= 400 lines per file).
// Represents the 6 First-Class Causal Constructs (`form`, `state`, `arc`,
// `flow`, `law`, `mind`), Native Application Fabric (`module`, `service`,
// `endpoint`), and general-purpose functions (`fn`).
// ============================================================================

#include "cycraft/language/cycraft_units_and_types.hpp"

#include <string>
#include <vector>

namespace cycraft::omega::language {

struct CyFieldDecl {
    std::string name;
    bool is_versioned_state = false;
    CyTypeDescriptor type;
    double initial_value = 0.0;
    bool has_initial_value = false;
    size_t line = 1;
};

struct CyFormDecl {
    std::string name;
    std::vector<CyFieldDecl> fields;
    size_t line = 1;
};

struct CyArcGuardClause {
    std::string lhs;
    std::string op;
    std::string rhs;
    double threshold = 0.0;
};

struct CyArcDecl {
    std::string name;
    std::string source_expr;
    std::string target_expr;
    std::string transform_expr;
    std::string map_fn;
    std::string delta_mode = "incremental";
    std::string retract_mode = "exact_inverse";
    std::string fallback_mode = "full_solve";
    std::vector<CyArcGuardClause> guards;
    size_t line = 1;
};

struct CyFlowParam {
    std::string name;
    CyTypeDescriptor type;
};

struct CyFlowStmt {
    std::string kind;
    std::string target;
    std::string expr;
    std::string aux;
    size_t line = 1;
};

struct CyFlowDecl {
    std::string name;
    std::vector<CyFlowParam> params;
    std::string return_type;
    std::string using_backend;
    std::string requires_clause;
    std::string ensures_clause;
    std::vector<CyFlowStmt> statements;
    size_t line = 1;
};

struct CyLawAssertion {
    std::string lhs_expr;
    std::string op;
    double rhs_value = 0.0;
    SiUnitDimension rhs_unit = SiUnitDimension::dimensionless();
    std::string raw_text;
    size_t line = 1;
};

struct CyLawDecl {
    std::string name;
    std::vector<CyLawAssertion> assertions;
    size_t line = 1;
};

struct CyMindDecl {
    std::string name;
    std::string model_id;
    std::vector<std::string> observed_signals;
    std::vector<std::string> proposed_actions;
    bool no_unverified_state_commit = false;
    double maximum_error = 1e-6;
    bool attempts_direct_state_write = false;
    size_t line = 1;
};

struct CyFunctionDecl {
    std::string name;
    std::vector<CyFlowParam> params;
    std::string return_type;
    std::string body_expr;
    size_t line = 1;
};

struct CyEndpointDecl {
    std::string name;
    std::string route = "/v1/infer";
    std::string method = "POST";
    std::string auth = "required";
    std::string timeout = "10s";
    std::string input_type = "Prompt";
    std::string return_type = "Result<Response, ApiError>";
    std::string body_expr;
    size_t line = 1;
};

struct CyServiceDecl {
    std::string name;
    std::vector<std::pair<std::string, std::string>> injected_deps;
    std::vector<CyEndpointDecl> endpoints;
    size_t line = 1;
};

struct CyModuleContractDecl {
    std::string name;
    std::vector<std::string> provides_list;
    std::vector<std::string> requires_list;
    std::string max_lines = "300";
    std::string max_functions = "12";
    std::string network_policy = "deny";
    std::string secrets_policy = "vault_only";
    std::string timeout = "2s";
    std::string retry_count = "3";
    size_t line = 1;
};

struct CyModuleAst {
    std::string source_path;
    std::string canonical_layer = "domain";
    std::string owner = "cycraft-core";
    std::string contract_id = "CY-GENESIS-001";
    std::vector<CyFormDecl> forms;
    std::vector<CyArcDecl> arcs;
    std::vector<CyFlowDecl> flows;
    std::vector<CyLawDecl> laws;
    std::vector<CyMindDecl> minds;
    std::vector<CyFunctionDecl> functions;
    std::vector<CyModuleContractDecl> module_contracts;
    std::vector<CyServiceDecl> services;
};

} // namespace cycraft::omega::language
