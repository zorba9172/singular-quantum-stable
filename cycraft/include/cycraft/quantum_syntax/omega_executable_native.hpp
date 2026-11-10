#pragma once
#include "cycraft/quantum_syntax/omega_executable_ir.hpp"
#include "cycraft/quantum_syntax/omega_executable_process.hpp"
#include <dlfcn.h>
#include <sstream>
namespace cycraft::omega::executable {
inline std::string value_source(const Value& v) {
    std::ostringstream o;o<<"Value{{";
    for(size_t i=0;i<v.shape.size();++i)o<<(i?",":"")<<v.shape[i];o<<"},{"<<std::hexfloat;
    for(size_t i=0;i<v.data.size();++i)o<<(i?",":"")<<v.data[i];o<<"}}";return o.str();
}
inline std::string native_source(const Program& p,bool library) {
    std::ostringstream o;
    o<<"#include \"cycraft/quantum_syntax/omega_executable_value.hpp\"\nusing namespace cycraft::omega::executable;\n";
    o<<(library?"extern \"C\" int singular_entry()":"int main()")<<" {try {\n";
    for(size_t i=0;i<p.nodes.size();++i) {
        const auto& n=p.nodes[i];o<<"Value v"<<i<<"=";
        if(n.op==Op::literal)o<<value_source(n.literal);
        else o<<"apply(static_cast<Op>("<<static_cast<int>(n.op)<<"),v"<<n.a<<",v"<<n.b<<")";
        o<<";\n";
    }
    for(const auto& c:p.checks)o<<"assertion(v"<<c.a<<",v"<<c.b<<","<<c.line<<");\n";
    for(const auto& [name,id]:p.outputs)o<<"print_value(std::cout,"<<quote(name)<<",v"<<id<<");\n";
    o<<"return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\\n';return 1;}}\n";return o.str();
}
inline std::filesystem::path compile(const Program& p,const std::filesystem::path& include,
                                     TemporaryDirectory& tmp,bool library) {
    auto source=tmp.path/"program.cpp", binary=tmp.path/(library?"program.dylib":"program");
    write_file(source,native_source(p,library));
    std::vector<std::string> args{"clang++","-std=c++23","-O3","-fno-fast-math","-ffp-contract=off","-I",include.string(),source.string(),"-o",binary.string()};
    if(library) args.push_back("-dynamiclib");
    if(process(args)!=0)throw Error("E_COMPILER: native compilation failed");return binary;
}
inline int jit(const Program& p,const std::filesystem::path& include) {
    TemporaryDirectory tmp;auto binary=compile(p,include,tmp,true);
    void* handle=dlopen(binary.c_str(),RTLD_NOW|RTLD_LOCAL);
    if(!handle)throw Error("E_JIT: "+std::string(dlerror()));
    struct LibraryGuard {void* h;~LibraryGuard(){dlclose(h);}} guard{handle};
    dlerror();auto entry=reinterpret_cast<int(*)()>(dlsym(handle,"singular_entry"));
    const char* error=dlerror();if(error||!entry)throw Error("E_JIT: entry unavailable");return entry();
}
inline void compile_native(const Program& p,const std::filesystem::path& include,const std::filesystem::path& out) {
    if(std::filesystem::exists(out))throw Error("E_OUTPUT: refusing to overwrite existing artifact");
    TemporaryDirectory tmp;auto binary=compile(p,include,tmp,false);
    // No shell interpolation and no writes to an existing executable inode.
    std::filesystem::copy_file(binary,out,std::filesystem::copy_options::none);
}
} // namespace cycraft::omega::executable
