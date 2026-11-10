#pragma once
#include "cycraft/quantum_syntax/omega_executable_ir.hpp"
#include "cycraft/quantum_syntax/omega_language_contract.hpp"
#include <charconv>
#include <set>
namespace cycraft::omega::executable {
struct Token { std::string text; size_t line=1; bool number=false,quoted=false; };
class Lexer {
    std::string src_; size_t pos_=0,line_=1;
public:
    Token token;
    explicit Lexer(std::string s):src_(std::move(s)) {
        if(src_.size()>1<<20) throw Error("E_BUDGET: source exceeds 1 MiB");
        for(size_t i=0;i<src_.size();) {
            unsigned char lead=src_[i++];uint32_t cp=lead;size_t continuation=0;uint32_t minimum=0;
            if(lead>=0xc2&&lead<=0xdf){cp=lead&31;continuation=1;minimum=0x80;}
            else if(lead>=0xe0&&lead<=0xef){cp=lead&15;continuation=2;minimum=0x800;}
            else if(lead>=0xf0&&lead<=0xf4){cp=lead&7;continuation=3;minimum=0x10000;}
            else if(lead>=128)throw Error("E_UTF8: invalid leading byte");
            while(continuation--){if(i>=src_.size()||(static_cast<unsigned char>(src_[i])&0xc0)!=0x80)throw Error("E_UTF8: invalid continuation");cp=(cp<<6)|(src_[i++]&63);}
            if(cp<minimum||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))throw Error("E_UTF8: invalid code point");
            if((cp>=0x202a&&cp<=0x202e)||(cp>=0x2066&&cp<=0x2069)||cp==0x200b||cp==0xfeff)throw Error("E_UTF8: invisible/bidi character");
        }
        next();
    }
    void next() {
        while(pos_<src_.size()) {
            unsigned char c=src_[pos_];
            if(c=='#' || src_.compare(pos_,2,"//")==0) {while(pos_<src_.size()&&src_[pos_]!='\n') ++pos_;continue;}
            if(std::isspace(c)) {if(c=='\n'&&++line_>400) throw Error("E_BUDGET: source lines");++pos_;continue;}
            break;
        }
        token={"",line_};if(pos_==src_.size()) return;
        if(src_[pos_]=='"') {
            size_t start=++pos_;
            while(pos_<src_.size()&&src_[pos_]!='"') {
                if(src_[pos_]=='\\'||static_cast<unsigned char>(src_[pos_])<32) throw Error("E_STRING: escapes/control characters are not supported");++pos_;
            }
            if(pos_==src_.size()) throw Error("E_STRING: unterminated string");
            token={src_.substr(start,pos_-start),line_,false,true};++pos_;return;
        }
        for(const char* op:{"§λ","§ω","§κ","⟦","⟧","←","⊢","⊕","⊗","⋅","=="}) {
            std::string s(op);if(src_.compare(pos_,s.size(),s)==0){token.text=s;pos_+=s.size();return;}
        }
        char c=src_[pos_];
        if(std::string("[](){},:+-*/").find(c)!=std::string::npos){token.text=src_.substr(pos_++,1);return;}
        size_t start=pos_;
        if(std::isdigit(static_cast<unsigned char>(c)) || c=='.') {
            double value;const char* first=src_.data()+pos_;
            auto r=std::from_chars(first,src_.data()+src_.size(),value);
            if(r.ec!=std::errc{} || r.ptr==first || !std::isfinite(value)) throw Error("E_NUMBER: invalid or non-finite number");
            pos_=static_cast<size_t>(r.ptr-src_.data());token={src_.substr(start,pos_-start),line_,true};return;
        }
        while(pos_<src_.size()) {
            unsigned char b=src_[pos_];
            if(b<128 && !(std::isalnum(b)||b=='_')) break;
            bool boundary=false;
            for(const char* op:{"⟦","⟧","←","⊢","⊕","⊗","⋅"}) if(src_.compare(pos_,std::string(op).size(),op)==0) boundary=true;
            if(boundary) break;++pos_;
        }
        if(pos_==start) throw Error("E_TOKEN: unsupported byte at line "+std::to_string(line_));
        token.text=src_.substr(start,pos_-start);
    }
    bool take(const std::string& t) {if(!token.quoted&&token.text==t){next();return true;}return false;}
    void need(const std::string& t) {if(!take(t)) throw Error("E_SYNTAX: expected "+t+" at line "+std::to_string(token.line));}
    std::string identifier() {
        const auto t=token;
        if(t.text.empty()||t.number||t.quoted) throw Error("E_NAME: identifier expected");
        for(unsigned char c:t.text) if(c<128&&!(std::isalnum(c)||c=='_')) throw Error("E_NAME: invalid identifier");
        if(t.text=="Ω"||t.text=="ℝ"||t.text=="φ₊"||t.text=="φτ"||t.text=="φσ"||t.text=="∑") throw Error("E_NAME: reserved symbol");
        next();return t.text;
    }
    double number() {
        bool minus=take("-");if(!minus)take("+");
        if(!token.number) throw Error("E_NUMBER: numeric literal expected");
        double x=0;auto r=std::from_chars(token.text.data(),token.text.data()+token.text.size(),x);
        if(r.ec!=std::errc{}||!std::isfinite(x))throw Error("E_NUMBER: invalid number");next();return minus?-x:x;
    }
};
class Grammar {
    Lexer lex_; Program p_; std::map<std::string,Shape> declarations_; size_t depth_=0;
    size_t primary() {
        if(++depth_>64) throw Error("E_BUDGET: expression depth");
        size_t result, line=lex_.token.line;
        if(lex_.take("-")) {size_t a=primary();result=p_.operation(Op::neg,a,a,line);}
        else if(lex_.take("(")) {result=expression();lex_.need(")");}
        else if(lex_.take("[")) {
            Value v;
            do {if(v.data.size()>=max_elements)throw Error("E_BUDGET: literal");v.data.push_back(lex_.number());}while(lex_.take(","));
            lex_.need("]");v.shape={v.data.size()};result=p_.literal(std::move(v),line);
        } else if(lex_.token.number) result=p_.literal(Value{{1},{lex_.number()}},line);
        else {
            std::string name=lex_.token.text;
            if(name=="φ₊"||name=="φτ"||name=="φσ"||name=="∑") {
                lex_.next();lex_.need("(");size_t a=expression();lex_.need(")");
                Op op=name=="φ₊"?Op::relu:name=="φτ"?Op::tanh:name=="φσ"?Op::sigmoid:Op::sum;
                result=p_.operation(op,a,a,line);
            } else {
                name=lex_.identifier();auto it=p_.outputs.find(name);
                if(it==p_.outputs.end()) throw Error("E_UNINITIALIZED: "+name+" at line "+std::to_string(line));result=it->second;
            }
        }
        --depth_;return result;
    }
    size_t expression(int minimum=0) {
        size_t a=primary();
        while(true) {
            std::string t=lex_.token.text; int precedence;Op op;
            if(t=="+"||t=="⊕") {precedence=1;op=Op::add;}
            else if(t=="-"){precedence=1;op=Op::sub;}
            else if(t=="*"){precedence=2;op=Op::mul;}
            else if(t=="/"){precedence=2;op=Op::div;}
            else if(t=="⋅"){precedence=2;op=Op::matmul;}
            else if(t=="⊗"){precedence=2;op=Op::kron;}
            else break;
            if(precedence<minimum)break;size_t line=lex_.token.line;lex_.next();
            size_t b=expression(precedence+1);a=p_.operation(op,a,b,line);
        }
        return a;
    }
public:
    explicit Grammar(const std::string& src):lex_(src) {}
    Program parse() {
        std::set<std::string> metadata;
        for(int i=0;i<3;++i) {
            std::string tag=lex_.token.text;
            if(tag!="§λ"&&tag!="§ω"&&tag!="§κ")throw Error("E_HEADER: expected layer, owner and contract");
            if(!metadata.insert(tag).second)throw Error("E_HEADER: duplicate metadata");
            lex_.next();lex_.need("(");
            if(!lex_.token.quoted||lex_.token.text.empty())throw Error("E_HEADER: nonempty quoted value required");
            if(tag=="§λ"&&!quantum_syntax::valid_canonical_layer(lex_.token.text))throw Error("E_HEADER: unknown layer");
            lex_.next();lex_.need(")");
        }
        lex_.need("Ω");lex_.need("⟦");
        do {
            std::string name=lex_.identifier();lex_.need(":");lex_.need("ℝ");lex_.need("[");Shape s;
            do {
                if(!lex_.token.number)throw Error("E_SHAPE: literal positive integer required");
                size_t n=0;const auto& t=lex_.token.text;auto r=std::from_chars(t.data(),t.data()+t.size(),n);
                if(r.ec!=std::errc{}||r.ptr!=t.data()+t.size()||!n)throw Error("E_SHAPE: invalid dimension");
                s.push_back(n);if(s.size()>2)throw Error("E_SHAPE: rank exceeds 2");lex_.next();
            }while(lex_.take(","));lex_.need("]");elements(s);
            if(!declarations_.emplace(name,s).second)throw Error("E_NAME: duplicate declaration "+name);
            if(declarations_.size()>256)throw Error("E_BUDGET: declarations");
        }while(lex_.take(","));
        lex_.need("⟧");lex_.need("{");
        while(!lex_.take("}")) {
            size_t line=lex_.token.line;
            if(lex_.token.text.empty())throw Error("E_SYNTAX: unclosed body");
            if(lex_.take("⊢")) {
                size_t a=expression();lex_.need("==");size_t b=expression();
                if(p_.nodes[a].shape!=p_.nodes[b].shape)throw Error("E_SHAPE: assertion operands");
                p_.checks.push_back({a,b,line});
            } else {
                std::string name=lex_.identifier();auto d=declarations_.find(name);
                if(d==declarations_.end())throw Error("E_UNDECLARED: "+name);
                if(p_.outputs.count(name))throw Error("E_SSA: duplicate assignment; use replay for input updates");
                lex_.need("←");size_t first_new=p_.nodes.size();size_t id=expression();auto& node=p_.nodes[id];
                if(node.op==Op::literal && id>=first_new && elements(d->second)==node.literal.data.size()) {
                    node.shape=d->second;node.literal.shape=d->second;p_.inputs[name]=id;
                }
                if(node.shape!=d->second)throw Error("E_SHAPE: assignment to "+name);
                p_.outputs[name]=id;
            }
        }
        if(!lex_.token.text.empty())throw Error("E_SYNTAX: trailing tokens");
        if(p_.outputs.size()!=declarations_.size())throw Error("E_UNINITIALIZED: every declaration requires an assignment");
        if(p_.checks.empty())throw Error("E_CONTRACT: an executable equality assertion is required");
        return std::move(p_);
    }
};
inline Program parse(const std::string& src) {return Grammar(src).parse();}
} // namespace cycraft::omega::executable
