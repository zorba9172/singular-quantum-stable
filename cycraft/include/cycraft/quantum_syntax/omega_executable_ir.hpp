#pragma once
#include "cycraft/quantum_syntax/omega_executable_value.hpp"
#include <map>
#include <tuple>
namespace cycraft::omega::executable {
struct Node { Op op=Op::literal; size_t a=0,b=0,line=0; Value literal; Shape shape; };
struct Check { size_t a,b,line; };
struct Program {
    std::vector<Node> nodes;
    std::vector<Check> checks;
    std::map<std::string,size_t> outputs,inputs;
    std::map<std::tuple<Op,size_t,size_t>,size_t> expressions;
    size_t bytes=0;
    size_t add(Node n) {
        if(nodes.size()>=max_nodes) throw Error("E_BUDGET: IR nodes");
        size_t cost=elements(n.shape)*sizeof(double);
        if(cost>max_bytes/8-bytes) throw Error("E_BUDGET: live values, IR literals and transaction reserve");
        bytes+=cost; nodes.push_back(std::move(n)); return nodes.size()-1;
    }
    size_t literal(Value v,size_t line) {
        validate(v); Node n; n.line=line;n.shape=v.shape;n.literal=std::move(v);return add(std::move(n));
    }
    size_t operation(Op op,size_t a,size_t b,size_t line) {
        // Pure ordered operands only: no reassociation, FMA contraction or
        // approximate/hash-only reuse; input literals are never interned.
        auto key=std::tuple{op,a,b};auto found=expressions.find(key);
        if(found!=expressions.end())return found->second;
        Node n; n.op=op;n.a=a;n.b=b;n.line=line;
        n.shape=shape_of(op,nodes.at(a).shape,nodes.at(b).shape);size_t id=add(std::move(n));expressions.emplace(key,id);return id;
    }
};
inline Value evaluate(const Node& n,const std::vector<Value>& state) {
    if(n.op==Op::literal) return n.literal;
    return apply(n.op,state.at(n.a),state.at(n.b));
}
inline std::vector<Value> interpret(const Program& p) {
    std::vector<Value> state;state.reserve(p.nodes.size());
    for(const auto& n:p.nodes) state.push_back(evaluate(n,state));
    for(const auto& c:p.checks) assertion(state.at(c.a),state.at(c.b),c.line);
    return state;
}
inline void output(const Program& p,const std::vector<Value>& state,std::ostream& o=std::cout) {
    for(const auto& [name,id]:p.outputs) print_value(o,name,state.at(id));
}
} // namespace cycraft::omega::executable
