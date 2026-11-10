#pragma once
#include "cycraft/quantum_syntax/omega_executable_value.hpp"
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
namespace cycraft::omega::executable {
inline int process(const std::vector<std::string>& args) {
    if(args.empty())throw Error("E_PROCESS: missing executable");
    std::vector<char*> argv;for(const auto& s:args)argv.push_back(const_cast<char*>(s.c_str()));argv.push_back(nullptr);
    pid_t pid;int e=posix_spawnp(&pid,argv[0],nullptr,nullptr,argv.data(),environ);
    if(e)throw Error("E_PROCESS: spawn error "+std::to_string(e));
    int status=0;while(waitpid(pid,&status,0)<0)if(errno!=EINTR)throw Error("E_PROCESS: waitpid");
    return WIFEXITED(status)?WEXITSTATUS(status):128+(WIFSIGNALED(status)?WTERMSIG(status):0);
}
struct TemporaryDirectory {
    std::filesystem::path path;
    TemporaryDirectory() {
        std::string pattern=(std::filesystem::temp_directory_path()/"singular-native-XXXXXX").string();
        std::vector<char> buf(pattern.begin(),pattern.end());buf.push_back(0);
        if(!mkdtemp(buf.data()))throw Error("E_IO: temporary directory");path=buf.data();
    }
    ~TemporaryDirectory(){std::error_code ec;std::filesystem::remove_all(path,ec);}
    TemporaryDirectory(const TemporaryDirectory&)=delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&)=delete;
};
inline std::string read_source(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary);if(!in)throw Error("E_IO: cannot open "+path.string());
    std::string data;char buffer[4096];
    while(in.read(buffer,sizeof(buffer))||in.gcount()){
        if(data.size()+static_cast<size_t>(in.gcount())>1<<20)throw Error("E_BUDGET: source exceeds 1 MiB");data.append(buffer,static_cast<size_t>(in.gcount()));
    }
    if(!in.eof())throw Error("E_IO: read failed");return data;
}
inline void write_file(const std::filesystem::path& path,const std::string& content) {
    std::ofstream out(path,std::ios::binary);out<<content;out.close();if(!out)throw Error("E_IO: write failed "+path.string());
}
} // namespace cycraft::omega::executable
