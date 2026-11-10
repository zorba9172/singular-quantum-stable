// Native process boundary. Scene authoring is C++; Blender API scripts are authorized adapters.
#include <array>
#include <cerrno>
#include <charconv>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mach-o/dyld.h>
#include <spawn.h>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#include <simdjson.h>
#include "../include/cycraft/media/exr_inspection.hpp"
extern char** environ;
namespace fs = std::filesystem;
static void admit_environment(const fs::path& contract) {
    constexpr uint64_t limit=16ull*1024*1024;
    const auto length=fs::file_size(contract);
    if(length>limit)throw std::runtime_error("Scene contract exceeds byte budget");
    std::ifstream stream(contract,std::ios::binary);
    std::string bytes(static_cast<size_t>(length),'\0');
    if(!stream.read(bytes.data(),bytes.size()) || stream.peek()!=std::char_traits<char>::eof())
        throw std::runtime_error("Scene contract changed while reading");
    simdjson::dom::parser parser;
    auto document=parser.parse(bytes).value();
    auto unique=[](simdjson::dom::element object,std::string_view key) {
        size_t count=0;
        for(auto field:object.get_object().value())count+=field.key==key;
        if(count>1)throw std::runtime_error("Duplicate environment admission field");
    };
    unique(document,"world");
    auto world=document["world"];
    if(world.error())throw std::runtime_error("Scene world is required");
    unique(world.value(),"environment");
    auto environment=world["environment"];
    if(environment.error()==simdjson::NO_SUCH_FIELD)return;
    unique(environment.value(),"uri");
    auto uri=environment["uri"].get_string().value();
    if(uri.empty() || uri.size()>4096 || uri.find('\0')!=std::string_view::npos)
        throw std::runtime_error("Invalid environment path");
    fs::path path{std::string(uri)};
    if(!path.is_absolute())throw std::runtime_error("Environment path must be absolute");
    std::cout<<singular::media::inspect_environment_exr(path)<<std::endl;
}
static volatile sig_atomic_t active_child = 0;
static volatile sig_atomic_t cancellation_signal = 0;
static void cancel_render(int signal) {
    cancellation_signal = signal;
    if (active_child > 0) kill(-active_child, signal == SIGALRM ? SIGKILL : signal);
}

static int launch(const std::vector<std::string>& arguments) {
    std::vector<char*> argv;
    argv.reserve(arguments.size() + 1);
    for (const auto& arg : arguments) argv.push_back(const_cast<char*>(arg.c_str()));
    argv.push_back(nullptr);
    unsigned deadline = 3600;
    if (const char* setting = std::getenv("SINGULAR_RENDER_TIMEOUT_SECONDS")) {
        const std::string input(setting);
        auto parsed = std::from_chars(input.data(), input.data() + input.size(), deadline);
        if (parsed.ec != std::errc{} || parsed.ptr != input.data() + input.size() ||
            deadline == 0 || deadline > 21600) {
            std::cerr << "Render timeout must be an integer in 1..21600 seconds\n";
            return 2;
        }
    }
    posix_spawnattr_t attributes;
    int error = posix_spawnattr_init(&attributes);
    if (error != 0) return 125;
    error = posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
    if (error == 0) error = posix_spawnattr_setpgroup(&attributes, 0);
    pid_t child = -1;
    if (error == 0)
        error = posix_spawn(&child, arguments.front().c_str(), nullptr, &attributes, argv.data(), environ);
    posix_spawnattr_destroy(&attributes);
    if (error != 0) {
        std::cerr << "Child process unavailable: errno=" << error << "\n";
        return 127;
    }
    active_child = child;
    if (cancellation_signal) cancel_render(cancellation_signal);
    alarm(deadline);
    int status = 0;
    while (waitpid(child, &status, 0) < 0) {
        if (errno == EINTR) continue;
        alarm(0);
        kill(-child, SIGKILL);
        std::cerr << "Unable to collect child process status\n";
        return 125;
    }
    alarm(0);
    active_child = 0;
    if (cancellation_signal) return 128 + cancellation_signal;
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}

struct StagingDirectory {
    fs::path path;
    ~StagingDirectory() { std::error_code ec; fs::remove_all(path, ec); }
};

static StagingDirectory create_stage(const fs::path& target,const std::string& prefix) {
    if(fs::exists(target))throw std::runtime_error("Output package already exists");
    fs::create_directories(target.parent_path());
    std::string pattern=(target.parent_path()/(prefix+"-XXXXXX")).string();
    std::vector<char> bytes(pattern.begin(),pattern.end());bytes.push_back('\0');
    const char* created=mkdtemp(bytes.data());
    if(!created)throw std::runtime_error("Cannot create package staging directory");
    return {fs::path(created)};
}

static void publish_stage(const StagingDirectory& stage,const fs::path& target) {
    if(fs::exists(target))throw std::runtime_error("Output package was published by another producer");
    fs::rename(stage.path,target);
}

static int bounded_integer(const std::string& input,int low,int high) {
    int value=0;
    auto parsed=std::from_chars(input.data(),input.data()+input.size(),value);
    if(parsed.ec!=std::errc{} || parsed.ptr!=input.data()+input.size() || value<low || value>high)
        throw std::runtime_error("Integer argument is outside supported bounds");
    return value;
}

int main(int argc, char** argv) {
    std::signal(SIGINT, cancel_render);
    std::signal(SIGTERM, cancel_render);
    std::signal(SIGALRM, cancel_render);
    try {
        if(argc==3 && std::string(argv[1])=="inspect-environment-exr") {
            std::cout<<singular::media::inspect_environment_exr(fs::absolute(argv[2]))<<"\n";
            return 0;
        }
        const char* override_path = std::getenv("SINGULAR_BLENDER_EXECUTABLE");
        const char* user_home = std::getenv("HOME");
        fs::path executable = override_path ? fs::path(override_path)
            : fs::path(user_home ? user_home : "") / "Applications/Blender-5.2.3.app/Contents/MacOS/Blender";
        std::array<char,4096> self{};
        uint32_t self_size = self.size();
        if (_NSGetExecutablePath(self.data(), &self_size) != 0)
            throw std::runtime_error("Executable path exceeds supported length");
        const fs::path workspace = fs::canonical(self.data()).parent_path().parent_path().parent_path();
        const fs::path adapters = workspace / "SingularQuantum/cycraft/adapters/blender";
        if (argc == 2 && std::string(argv[1]) == "status")
            return launch({executable.string(), "--version"});
        if(argc==4 && std::string(argv[1])=="inspect-exr") {
            const auto result=singular::media::inspect_exr(fs::canonical(argv[2]));
            const auto target=fs::absolute(argv[3]);
            auto stage=create_stage(target,".exr-stage");
            std::ofstream report(stage.path/"inspection.json");
            report<<result<<"\n";report.close();
            if(!report)throw std::runtime_error("EXR report write failed");
            publish_stage(stage,target);
            std::cout<<"EXR_FULLY_DECODED "<<target<<"\n";
            return 0;
        }
        if(argc==7 && std::string(argv[1])=="encode-film") {
            const fs::path frames=fs::canonical(argv[2]),target=fs::absolute(argv[3]);
            const int first=bounded_integer(argv[4],1,999999);
            const int last=bounded_integer(argv[5],first,999999);
            const int fps=bounded_integer(argv[6],1,120);
            if(last-first>=240)throw std::runtime_error("Film frame budget exceeded");
            const char* encoder_override=std::getenv("SINGULAR_FFMPEG_EXECUTABLE");
            const fs::path encoder=encoder_override?fs::path(encoder_override):
                fs::path(user_home?user_home:"")/".local/bin/ffmpeg";
            auto stage=create_stage(target,".film-stage");
            int status=launch({executable.string(),"--background","--factory-startup","--disable-autoexec",
                "--python-exit-code","1","--python",(adapters/"verify_frames.py").string(),"--",
                frames.string(),(stage.path/"frames.json").string(),argv[4],argv[5],argv[6]});
            if(status!=0)return status;
            for(int frame=first;frame<=last;++frame) {
                std::ostringstream stem;stem<<"frame_"<<std::setfill('0')<<std::setw(6)<<frame;
                const auto result=singular::media::inspect_exr(frames/(stem.str()+".exr"));
                std::ofstream report(stage.path/(stem.str()+"_exr.json"));
                report<<result<<"\n";report.close();
                if(!report)throw std::runtime_error("EXR report write failed");
            }
            const auto movie=stage.path/"film.mp4";
            status=launch({encoder.string(),"-hide_banner","-nostdin","-n","-framerate",std::to_string(fps),
                "-start_number",std::to_string(first),"-i",(frames/"frame_%06d.png").string(),
                "-frames:v",std::to_string(last-first+1),"-c:v","libx264","-crf","18",
                "-pix_fmt","yuv420p","-movflags","+faststart",movie.string()});
            if(status!=0)return status;
            const auto progress=stage.path/"decode.txt";
            status=launch({encoder.string(),"-hide_banner","-nostdin","-v","error","-xerror",
                "-i",movie.string(),"-map","0:v:0","-progress",progress.string(),"-f","null","-"});
            if(status!=0)return status;
            std::ifstream decoded(progress);std::string line;int count=0;bool ended=false;
            while(std::getline(decoded,line)) {
                if(line.starts_with("frame="))count=bounded_integer(line.substr(6),0,240);
                if(line=="progress=end")ended=true;
            }
            if(count!=last-first+1 || !ended || !fs::is_regular_file(movie) || fs::file_size(movie)<128)
                throw std::runtime_error("Encoded film did not decode the complete frame sequence");
            std::ofstream receipt(stage.path/"film.json");
            receipt<<"{\"status\":\"ENCODED_AND_FULLY_DECODED\",\"frames\":"<<count
                   <<",\"fps\":"<<fps<<",\"movie\":\"film.mp4\",\"exr_validation\":\"FULL_DECODE\"}\n";
            receipt.close();if(!receipt)throw std::runtime_error("Film receipt write failed");
            publish_stage(stage,target);
            std::cout<<"FILM_VERIFIED "<<target<<" frames="<<count<<" fps="<<fps<<"\n";
            return 0;
        }
        if (argc == 5 && std::string(argv[1]) == "verify-deformation") {
            if (!fs::is_directory(argv[2]) || fs::exists(argv[4]))
                throw std::runtime_error("Deformation verification needs a scene package and a new report");
            return launch({executable.string(), "--background", "--factory-startup", "--disable-autoexec",
                "--python-exit-code", "1", "--python", (adapters / "verify_deformation.py").string(),
                "--", fs::absolute(argv[2]).string(), argv[3], fs::absolute(argv[4]).string()});
        }
        if (argc == 4 && std::string(argv[1]) == "inspect-scene") {
            if (!fs::is_regular_file(argv[2]) || fs::exists(argv[3]))
                throw std::runtime_error("Inspection requires an existing scene and a new report path");
            return launch({executable.string(), "--background", "--factory-startup", "--disable-autoexec",
                "--python-exit-code", "1", "--python", (adapters / "inspect_scene.py").string(),
                "--", fs::absolute(argv[2]).string(), fs::absolute(argv[3]).string()});
        }
        if (argc == 4 && std::string(argv[1]) == "assemble") {
            const fs::path target = fs::absolute(argv[3]);
            if (fs::exists(target) || !fs::is_regular_file(argv[2]))
                throw std::runtime_error("Assembly needs an existing contract and a new output directory");
            admit_environment(argv[2]);
            auto stage=create_stage(target,".scene-stage");
            int status=launch({executable.string(),"--background","--factory-startup","--disable-autoexec",
                "--python-exit-code","1","--python",(adapters/"assemble.py").string(),"--",
                fs::absolute(argv[2]).string(),stage.path.string()});
            if(status!=0)return status;
            for(const char* name:{"nocturne.blend","nocturne.glb","nocturne.usdc","assembly.json","interchange.json"})
                if(!fs::is_regular_file(stage.path/name) || fs::file_size(stage.path/name)<32)
                    throw std::runtime_error(std::string("Missing assembly output: ")+name);
            publish_stage(stage,target);
            std::cout<<"SCENE_PUBLISHED "<<target<<"\n";
            return 0;
        }
        bool smoke = argc >= 3 && std::string(argv[1]) == "smoke";
        bool range = argc >= 7 && std::string(argv[1]) == "render-range";
        bool shot = (argc >= 6 && std::string(argv[1]) == "render-shot") || range;
        bool render = (argc >= 5 && std::string(argv[1]) == "render") || shot;
        if ((!smoke && !render) || (smoke && argc > 4) || (render && argc > (range ? 9 : shot ? 8 : 6))) {
            std::cerr << "Usage: singular-cycles status\n"
                      << "       singular-cycles inspect-exr IMAGE.exr NEW_OUTPUT_DIR\n"
                      << "       singular-cycles inspect-scene SCENE NEW_REPORT.json\n"
                      << "       singular-cycles verify-deformation SCENE_DIR MESH_NAME NEW_REPORT.json\n"
                      << "       singular-cycles assemble CONTRACT.json NEW_OUTPUT_DIR\n"
                      << "       singular-cycles encode-film FRAMES NEW_OUTPUT_DIR FIRST LAST FPS\n"
                      << "       singular-cycles render-shot SCENE.blend OUTPUT_DIR FRAME CAMERA [CPU|METAL [--preview]]\n"
                      << "       singular-cycles render-range SCENE.blend OUTPUT_DIR START END CAMERA [CPU|METAL [--preview]]\n"
                      << "       singular-cycles smoke OUTPUT_DIR [CPU|METAL]\n"
                      << "       singular-cycles render SCENE.blend OUTPUT_DIR FRAME [CPU|METAL]\n";
            return 2;
        }
        int frame = 1;
        if (render) {
            frame=bounded_integer(argv[4],1,999999);
            fs::path scene = argv[2];
            if (scene.extension() != ".blend" || !fs::is_regular_file(scene) || fs::file_size(scene) < 128) {
                std::cerr << "Scene unavailable or too small to be a serialized Blender project\n";
                return 2;
            }
        }
        int last_frame = frame;
        if (range) {
            last_frame=bounded_integer(argv[5],frame,999999);
            if (last_frame - frame >= 240)
                throw std::runtime_error("Frame range must be ascending and contain at most 240 frames");
        }
        const int device_index = smoke ? 3 : (range ? 7 : shot ? 6 : 5);
        const std::string device = argc > device_index ? argv[device_index] : "METAL";
        if (device != "CPU" && device != "METAL") {
            std::cerr << "Unsupported device; choose CPU or METAL\n";
            return 2;
        }
        const bool preview = shot && argc > device_index + 1;
        if (preview && std::string(argv[device_index + 1]) != "--preview")
            throw std::runtime_error("Unsupported render profile; expected --preview");
        fs::path output = fs::absolute(argv[smoke ? 2 : 3]);
        auto stage=create_stage(output,".cycles-stage");
        std::vector<std::string> command{executable.string(), "--background", "--factory-startup",
                                         "--disable-autoexec"};
        if (render) command.push_back(fs::absolute(argv[2]).string());
        if (shot) command.insert(command.end(), {"--python-exit-code","1","--python",
            (adapters/"render.py").string(),"--",stage.path.string(),std::to_string(frame),
            argv[range ? 6 : 5],device,std::to_string(last_frame),preview ? "PREVIEW" : "FINAL"});
        else command.insert(command.end(), {"--engine", "CYCLES", "--render-output",
            (stage.path / "frame_######").string(), "--render-format", "PNG", "--render-frame",
            std::to_string(frame), "--", "--cycles-device", device, "--cycles-print-stats"});
        std::cout << "Backend=Cycles | Device=" << device
                  << " | Input=" << (smoke ? "Blender factory cube (SMOKE ONLY)" : argv[2]) << "\n";
        std::cout.flush();
        const int status = launch(command);
        if (status != 0) { std::cerr << "Blender failed: exit=" << status << "\n"; return status; }
        for (int current = frame; current <= last_frame; ++current) {
        std::ostringstream name;
        name << "frame_" << std::setfill('0') << std::setw(6) << current << ".png";
        const fs::path produced = stage.path / name.str();
        std::ifstream image(produced, std::ios::binary);
        std::array<unsigned char, 8> signature{};
        image.read(reinterpret_cast<char*>(signature.data()), signature.size());
        constexpr std::array<unsigned char, 8> png{137,80,78,71,13,10,26,10};
        if (!image || signature != png || fs::file_size(produced) <= 8) {
            std::cerr << "Blender returned success without a fresh PNG\n";
            return 1;
        }
        image.close();
        if (shot) {
            fs::path exr = produced.filename(); exr.replace_extension(".exr");
            fs::path metadata = produced.filename(); metadata.replace_extension(".json");
            std::ifstream passes(stage.path / exr, std::ios::binary);
            std::array<unsigned char, 4> magic{};
            passes.read(reinterpret_cast<char*>(magic.data()), magic.size());
            if (!passes || magic != std::array<unsigned char,4>{0x76,0x2f,0x31,0x01} ||
                !fs::is_regular_file(stage.path / metadata) || fs::file_size(stage.path / metadata) < 32)
                throw std::runtime_error("Render is missing EXR passes or completion metadata");
        }
        }
        // Publish the immutable package in one same-filesystem rename. This also
        // preserves ExFAT sidecars without enumerating metadata as render products.
        publish_stage(stage,output);
        std::cout << "RENDER_VERIFIED " << output << " frames=" << (last_frame - frame + 1) << "\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Cycles adapter error: " << error.what() << "\n";
        return 1;
    }
}

