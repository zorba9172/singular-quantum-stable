#pragma once
#include "cycraft/quantum_syntax/omega_executable_ir.hpp"
#include <queue>
namespace cycraft::omega::executable {
// Exact cache admission: immutable typed DAG, one ordered event transaction,
// bit-identical cached values, and executable postconditions. No hash-only hits.
class Incremental {
    Program p_;std::vector<Value> state_;
    std::vector<std::vector<size_t>> users_;
    std::vector<std::pair<size_t,Value>> undo_;
    bool can_undo_=false;
public:
    size_t evaluated=0,cutoffs=0;
    explicit Incremental(Program p):p_(std::move(p)),state_(interpret(p_)),users_(p_.nodes.size()) {
        for(size_t i=0;i<p_.nodes.size();++i)if(p_.nodes[i].op!=Op::literal) {
            users_[p_.nodes[i].a].push_back(i);
            if(p_.nodes[i].b!=p_.nodes[i].a)users_[p_.nodes[i].b].push_back(i);
        }
    }
    const std::vector<Value>& state()const{return state_;}
    const Program& program()const{return p_;}
    bool can_undo()const{return can_undo_;}
    void update(const std::string& name,Value value,bool incremental=true) {
        auto it=p_.inputs.find(name);if(it==p_.inputs.end())throw Error("E_INPUT: not an independent literal input: "+name);
        validate(value);size_t id=it->second;
        if(value.shape!=state_[id].shape)throw Error("E_SHAPE: update changes type");
        std::vector<std::pair<size_t,Value>> changes;changes.reserve(p_.nodes.size());
        evaluated=0;cutoffs=0;
        auto replace=[&](size_t n,Value v){changes.emplace_back(n,std::move(state_[n]));state_[n]=std::move(v);};
        try {
            replace(id,std::move(value));
            if(incremental) {
                std::priority_queue<size_t,std::vector<size_t>,std::greater<size_t>> ready;
                std::vector<bool> queued(p_.nodes.size(),false);
                auto schedule=[&](size_t n){for(size_t user:users_[n])if(!queued[user]){queued[user]=true;ready.push(user);}};
                if(!identical(changes.back().second,state_[id]))schedule(id);else ++cutoffs;
                while(!ready.empty()) {
                    size_t n=ready.top();ready.pop();Value next=evaluate(p_.nodes[n],state_);++evaluated;
                    if(identical(next,state_[n])){++cutoffs;continue;}
                    replace(n,std::move(next));schedule(n);
                }
            }else {
                for(size_t n=0;n<p_.nodes.size();++n)if(p_.nodes[n].op!=Op::literal){replace(n,evaluate(p_.nodes[n],state_));++evaluated;}
            }
            for(const auto& c:p_.checks)assertion(state_[c.a],state_[c.b],c.line);
        }catch(...){for(auto i=changes.rbegin();i!=changes.rend();++i)state_[i->first]=std::move(i->second);throw;}
        undo_=std::move(changes);can_undo_=true;
    }
    void undo() {
        if(!can_undo_)throw Error("E_UNDO: no committed transaction");
        evaluated=0;cutoffs=0;
        for(auto i=undo_.rbegin();i!=undo_.rend();++i)state_[i->first]=std::move(i->second);
        undo_.clear();can_undo_=false;
    }
};
} // namespace cycraft::omega::executable
