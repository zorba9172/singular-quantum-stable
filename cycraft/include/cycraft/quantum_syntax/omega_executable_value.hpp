#pragma once
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
namespace cycraft::omega::executable {
inline constexpr size_t max_elements = 1 << 20, max_nodes = 4096, max_bytes = 64 << 20;
struct Error : std::runtime_error { using std::runtime_error::runtime_error; };
using Shape = std::vector<size_t>;
inline size_t elements(const Shape& s) {
    if (s.empty() || s.size() > 2) throw Error("E_SHAPE: rank must be 1 or 2");
    size_t n=1;
    for (auto d:s) { if (!d || d>max_elements/n) throw Error("E_BUDGET: tensor size"); n*=d; }
    return n;
}
struct Value { Shape shape{1}; std::vector<double> data; };
enum class Op { literal, add, sub, mul, div, matmul, kron, neg, relu, tanh, sigmoid, sum };
inline Shape shape_of(Op op, const Shape& a, const Shape& b={1}) {
    elements(a); elements(b);
    if (op==Op::sum) return {1};
    if (op==Op::matmul) {
        if (a.size()!=2 || (b.size()!=1 && b.size()!=2) || a[1]!=b[0]) throw Error("E_SHAPE: contraction");
        Shape s=b.size()==1 ? Shape{a[0]} : Shape{a[0],b[1]}; elements(s); return s;
    }
    if (op==Op::kron) {
        if (a.size()!=b.size()) throw Error("E_SHAPE: Kronecker rank");
        Shape s=a;
        for (size_t i=0;i<s.size();++i) { if (b[i]>max_elements/s[i]) throw Error("E_BUDGET: Kronecker size"); s[i]*=b[i]; }
        elements(s); return s;
    }
    if (op==Op::add || op==Op::sub || op==Op::mul || op==Op::div) {
        if(a==Shape{1}) return b;
        if(b==Shape{1} || a==b) return a;
        throw Error("E_SHAPE: elementwise operands (only scalar broadcasting is supported)");
    }
    return a;
}
inline void validate(const Value& v) {
    if(v.data.size()!=elements(v.shape)) throw Error("E_SHAPE: data length");
    for(double x:v.data) if(!std::isfinite(x)) throw Error("E_NUMERIC: non-finite value");
}
inline Value apply(Op op, const Value& a, const Value& b=Value{{1},{0}}) {
    validate(a); validate(b);
    Value out{shape_of(op,a.shape,b.shape),{}}; out.data.resize(elements(out.shape));
    if(op==Op::matmul) {
        size_t m=a.shape[0], k=a.shape[1], n=b.shape.size()==2?b.shape[1]:1;
        if(m>100000000/k/n) throw Error("E_BUDGET: contraction work");
        for(size_t i=0;i<m;++i) for(size_t j=0;j<n;++j) {
            double s=0;
            for(size_t q=0;q<k;++q) s=s+a.data[i*k+q]*b.data[q*n+j];
            out.data[i*n+j]=s;
        }
    } else if(op==Op::kron) {
        size_t ar=a.shape.size()==2?a.shape[0]:1, ac=a.data.size()/ar;
        size_t br=b.shape.size()==2?b.shape[0]:1, bc=b.data.size()/br;
        for(size_t i=0;i<ar;++i) for(size_t j=0;j<ac;++j)
            for(size_t k=0;k<br;++k) for(size_t l=0;l<bc;++l)
                out.data[(i*br+k)*(ac*bc)+j*bc+l]=a.data[i*ac+j]*b.data[k*bc+l];
    } else if(op==Op::sum) {
        double s=0; for(double x:a.data) s=s+x; out.data[0]=s;
    } else for(size_t i=0;i<out.data.size();++i) {
        double x=a.data[a.data.size()==1?0:i], y=b.data[b.data.size()==1?0:i];
        switch(op) {
            case Op::add: out.data[i]=x+y; break; case Op::sub: out.data[i]=x-y; break;
            case Op::mul: out.data[i]=x*y; break;
            case Op::div: if(y==0) throw Error("E_NUMERIC: division by zero"); out.data[i]=x/y; break;
            case Op::neg: out.data[i]=-x; break;
            case Op::relu: out.data[i]=std::max(0.0,x); break;
            case Op::tanh: out.data[i]=std::tanh(x); break;
            case Op::sigmoid: out.data[i]=x>=0 ? 1/(1+std::exp(-x)) : std::exp(x)/(1+std::exp(x)); break;
            default: throw Error("E_OPCODE: unsupported operation");
        }
    }
    validate(out); return out;
}
inline bool identical(const Value& a,const Value& b) {
    if(a.shape!=b.shape || a.data.size()!=b.data.size()) return false;
    for(size_t i=0;i<a.data.size();++i) if(std::bit_cast<uint64_t>(a.data[i])!=std::bit_cast<uint64_t>(b.data[i])) return false;
    return true;
}
inline bool equal(const Value& a,const Value& b) { return a.shape==b.shape && a.data==b.data; }
inline void assertion(const Value& a,const Value& b,size_t line) {
    if(!equal(a,b)) throw Error("E_ASSERT: equality failed at line "+std::to_string(line));
}
inline std::string quote(const std::string& s) {
    std::string out="\"";
    for(unsigned char c:s) {
        if(c=='"' || c=='\\') {out+='\\';out+=char(c);}
        else if(c<32) throw Error("E_TEXT: control character"); else out+=char(c);
    }
    return out+'"';
}
inline void print_value(std::ostream& o,const std::string& name,const Value& v) {
    o<<"{\"symbol\":"<<quote(name)<<",\"shape\":[";
    for(size_t i=0;i<v.shape.size();++i) o<<(i?",":"")<<v.shape[i];
    o<<"],\"values\":["<<std::setprecision(17);
    for(size_t i=0;i<v.data.size();++i) o<<(i?",":"")<<v.data[i];
    o<<"]}\n";
}
} // namespace cycraft::omega::executable
