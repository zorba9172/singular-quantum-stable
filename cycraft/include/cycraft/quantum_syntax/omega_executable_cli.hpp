#pragma once
#include "cycraft/quantum_syntax/omega_executable_grammar.hpp"
#include "cycraft/quantum_syntax/omega_executable_native.hpp"
#include "cycraft/quantum_syntax/omega_executable_incremental.hpp"
#include <chrono>
namespace cycraft::omega::executable {
inline std::filesystem::path runtime_include(const char* argv0) {
    std::error_code ec;auto executable=std::filesystem::canonical(argv0,ec);
    if(ec)throw Error("E_RUNTIME: invoke executable by path");
    auto root=executable.parent_path().parent_path();auto include=root/"cycraft/include";
    if(!std::filesystem::exists(include/"cycraft/quantum_syntax/omega_executable_value.hpp"))throw Error("E_RUNTIME: installed runtime headers unavailable");
    return include;
}
inline void replay(const Program& p,const std::string& events,bool incremental) {
    Incremental machine(p);std::istringstream in(events);std::string line;size_t sequence=0;
    while(std::getline(in,line)) {
        Lexer lex(line);if(lex.token.text.empty())continue;
        if(++sequence>10000)throw Error("E_BUDGET: event count");
        auto start=std::chrono::steady_clock::now();
        if(lex.take("↶")){if(!lex.token.text.empty())throw Error("E_EVENT: trailing undo tokens");machine.undo();}
        else {
            auto name=lex.identifier();lex.need("←");auto it=p.inputs.find(name);
            if(it==p.inputs.end())throw Error("E_INPUT: unknown input "+name);
            Value v;v.shape=p.nodes[it->second].shape;
            if(lex.take("[")){do{if(v.data.size()>=max_elements)throw Error("E_BUDGET: event");v.data.push_back(lex.number());}while(lex.take(","));lex.need("]");}
            else v.data.push_back(lex.number());
            if(!lex.token.text.empty())throw Error("E_EVENT: trailing tokens");machine.update(name,std::move(v),incremental);
        }
        auto elapsed=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
        std::cerr<<"{\"event\":"<<sequence<<",\"update_us\":"<<elapsed<<",\"evaluated_nodes\":"<<machine.evaluated<<",\"cutoffs\":"<<machine.cutoffs<<"}\n";
        output(p,machine.state());
    }
}
inline int smoke() {
    const std::string src="§λ(\"core\")\n§ω(\"smoke\")\n§κ(\"arithmetic\")\nΩ ⟦ α : ℝ[2], β : ℝ[2] ⟧ {\nα ← [2,3]\nβ ← α * 2 + 1\n⊢ β == [5,7]\n}\n";
    auto p=parse(src);auto state=interpret(p);
    if(state.at(p.outputs.at("β")).data!=std::vector<double>{5,7})throw Error("E_SMOKE: arithmetic");
    Incremental runtime(p);bool rejected=false;
    try{runtime.update("α",Value{{2},{3,4}});}catch(const Error&){rejected=true;}
    if(!rejected||!identical(runtime.state().at(p.outputs.at("β")),state.at(p.outputs.at("β"))))throw Error("E_SMOKE: rollback");
    std::cout<<"{\"scope\":\"executable arithmetic and transactional assertion rollback\",\"status\":\"passed\",\"scientific_advantage\":null}\n";return 0;
}
inline int enforce(const std::filesystem::path& root) {
    if(!std::filesystem::is_directory(root))throw Error("E_IO: enforcement root is not a directory");
    size_t files=0,failed=0,visited=0,bytes=0;
    for(const auto& entry:std::filesystem::recursive_directory_iterator(root)) {
        if(++visited>100000)throw Error("E_BUDGET: directory entries");
        if(!entry.is_regular_file()||entry.path().extension()!=".qcy"||entry.path().filename().string().rfind("._",0)==0)continue;
        if(++files>10000)throw Error("E_BUDGET: enforcement files");
        try {
            auto source=read_source(entry.path());if(source.size()>max_bytes-bytes)throw Error("E_BUDGET: enforcement bytes");bytes+=source.size();parse(source);
        }catch(const std::exception& e){++failed;if(failed<=50)std::cerr<<entry.path()<<": "<<e.what()<<'\n';}
    }
    std::cout<<"{\"profile\":\"qcy-executable-1\",\"files\":"<<files<<",\"rejected\":"<<failed<<",\"executed\":false}\n";
    return files>0&&failed==0?0:1;
}
inline int cli(int argc,char** argv) {
    try {
        if(argc<2)throw Error("E_USAGE: run|jit|verify|compile-native|replay|replay-full|verify-all|capabilities");
        const std::string command=argv[1];
        if(command=="--version"||command=="version"||command=="-v") {std::cout<<"SINGULAR qcy-executable-1 (limited executable profile; no general or scientific certification)\n";return 0;}
        if(command=="capabilities") {
            std::cout<<"{\"profile\":\"qcy-executable-1\",\"values\":\"finite float64, rank 1/2\",\"backends\":[\"vm\",\"native\",\"jit\"],\"distributed_transport\":null,\"quantum_solver_binding\":null,\"scientific_advantage\":null,\"patentability\":null}\n";return 0;
        }
        if(command=="verify-all")return smoke();
        if(command=="deep-read")throw Error("E_UNAVAILABLE: source scanning is not semantic verification; use the release source manifest");
        if(command=="enforce"||command=="enforce-workspace") {if(argc!=3)throw Error("E_USAGE: enforcement root required");return enforce(argv[2]);}
        if(command=="release-stable"||command.rfind("audit-",0)==0)
            throw Error("E_UNVERIFIED: legacy gate-count certification is retired; use the frozen release validator");
        if(argc<3)throw Error("E_USAGE: source path required");
        if(command!="run"&&command!="jit"&&command!="verify"&&command!="compile-native"&&command!="replay"&&command!="replay-full")throw Error("E_COMMAND: unsupported command "+command);
        Program p=parse(read_source(argv[2]));
        if(command=="verify") {
            std::cout<<"{\"status\":\"typechecked\",\"runtime_assertions_executed\":false,\"ir_nodes\":"<<p.nodes.size()<<"}\n";return 0;
        }
        if(command=="run"){output(p,interpret(p));return 0;}
        if(command=="jit")return jit(p,runtime_include(argv[0]));
        if(argc!=4)throw Error("E_USAGE: output path or events path required");
        if(command=="compile-native"){compile_native(p,runtime_include(argv[0]),argv[3]);return 0;}
        replay(p,read_source(argv[3]),command=="replay");return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
} // namespace cycraft::omega::executable
