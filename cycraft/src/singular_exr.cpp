#include "../include/cycraft/media/exr_inspection.hpp"
#include <zlib.h>
#define TINYEXR_USE_MINIZ 0
#define TINYEXR_IMPLEMENTATION
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-function"
#endif
#include "../../third_party/tinyexr/tinyexr.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#include <cmath>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace singular::media {
// A 3840x2160 render with the current 27 float channels requires 895,795,200 bytes.
// Keep an explicit aggregate bound; this decoder holds all decoded parts together.
constexpr int64_t decoded_pixel_budget_bytes=1024ll*1024*1024;
constexpr uint64_t input_budget_bytes=1024ull*1024*1024;
struct ExrResources {
    EXRHeader single{};
    EXRHeader** multiple=nullptr;
    int count=0;
    std::vector<EXRImage> images;
    const char* error=nullptr;
    ExrResources() { InitEXRHeader(&single); }
    ~ExrResources() {
        for(auto& image:images)FreeEXRImage(&image);
        FreeEXRHeader(&single);
        if(multiple) {
            for(int i=0;i<count;++i)if(multiple[i]) { FreeEXRHeader(multiple[i]);std::free(multiple[i]); }
            std::free(multiple);
        }
        if(error)FreeEXRErrorMessage(error);
    }
    ExrResources(const ExrResources&)=delete;
    ExrResources& operator=(const ExrResources&)=delete;
    void check(int result) {
        if(result!=TINYEXR_SUCCESS)throw std::runtime_error(error?error:"EXR operation failed");
    }
    EXRHeader& header(int i) { return multiple?*multiple[i]:single; }
};
struct ExrLayout { int64_t width,height;int channels; };
inline ExrLayout exr_layout(EXRHeader& header) {
    if(header.tiled || header.non_image)throw std::runtime_error("Tiled or deep EXR part is unsupported");
    const auto& window=header.data_window;
    ExrLayout size{int64_t(window.max_x)-window.min_x+1,int64_t(window.max_y)-window.min_y+1,header.num_channels};
    if(size.width<1 || size.height<1 || size.width>8192 || size.height>8192 || size.channels<1 || size.channels>64)
        throw std::runtime_error("EXR part exceeds dimension/channel budget");
    for(int channel=0;channel<size.channels;++channel) {
        if(header.channels[channel].x_sampling!=1 || header.channels[channel].y_sampling!=1)
            throw std::runtime_error("Subsampled EXR channel is unsupported");
        if(header.pixel_types[channel]==TINYEXR_PIXELTYPE_HALF)
            header.requested_pixel_types[channel]=TINYEXR_PIXELTYPE_FLOAT;
        const int type=header.requested_pixel_types[channel];
        if(type!=TINYEXR_PIXELTYPE_FLOAT && type!=TINYEXR_PIXELTYPE_UINT)
            throw std::runtime_error("EXR channel pixel type unsupported");
    }
    return size;
}
inline std::string exr_name(const char* value) {
    const auto name=std::string(value,strnlen(value,256));
    if(name.size()==256 || std::any_of(name.begin(),name.end(),[](unsigned char c){return c<32 || c>126;}))
        throw std::runtime_error("EXR name invalid");
    return name;
}
std::string inspect_environment_exr(const std::filesystem::path& path) {
    constexpr uint64_t file_limit=64ull*1024*1024, decoded_limit=128ull*1024*1024;
    if(path.extension()!=".exr" || !std::filesystem::is_regular_file(path)
       || !std::filesystem::file_size(path) || std::filesystem::file_size(path)>file_limit)
        throw std::runtime_error("Environment EXR exceeds file/format budget");
    EXRVersion version{};
    if(ParseEXRVersionFromFile(&version,path.c_str())!=TINYEXR_SUCCESS
       || version.multipart || version.non_image || version.tiled)
        throw std::runtime_error("Environment requires one scanline EXR part");
    ExrResources data;
    data.check(ParseEXRHeaderFromFile(&data.single,&version,path.c_str(),&data.error));
    const auto size=exr_layout(data.single);
    const auto& display=data.single.display_window;
    const auto& window=data.single.data_window;
    if(display.min_x!=window.min_x || display.min_y!=window.min_y
       || display.max_x!=window.max_x || display.max_y!=window.max_y
       || window.min_x!=0 || window.min_y!=0)
        throw std::runtime_error("Environment EXR requires uncropped zero-origin windows");
    const auto bytes=size.width*size.height*size.channels*4;
    if(size.width>4096 || size.height>2048 || size.width!=2*size.height
       || size.channels>4 || bytes>decoded_limit)
        throw std::runtime_error("Environment EXR exceeds panorama decode budget");
    std::ostringstream out;
    out<<"{\"status\":\"ENVIRONMENT_EXR_HEADER_ADMITTED_NOT_DECODED\",\"width\":"<<size.width
       <<",\"height\":"<<size.height<<",\"channels\":"<<size.channels
       <<",\"decoded_channel_bytes\":"<<bytes<<",\"decoded_budget_bytes\":"<<decoded_limit
       <<",\"file_budget_bytes\":"<<file_limit<<"}";
    return out.str();
}
std::string inspect_exr(const std::filesystem::path& path) {
    if(!std::filesystem::is_regular_file(path) || std::filesystem::file_size(path)>input_budget_bytes)
        throw std::runtime_error("EXR missing or larger than input budget");
    EXRVersion version{};
    if(ParseEXRVersionFromFile(&version,path.c_str())!=TINYEXR_SUCCESS)
        throw std::runtime_error("Invalid EXR version header");
    if(version.non_image || version.tiled)throw std::runtime_error("Deep or tiled EXR is unsupported");
    ExrResources data;
    if(version.multipart)
        data.check(ParseEXRMultipartHeaderFromFile(&data.multiple,&data.count,&version,path.c_str(),&data.error));
    else {
        data.check(ParseEXRHeaderFromFile(&data.single,&version,path.c_str(),&data.error));
        data.count=1;
    }
    if(data.count<1 || data.count>32)throw std::runtime_error("EXR part count exceeds budget");
    std::vector<ExrLayout> layouts;
    std::vector<const EXRHeader*> headers;
    int64_t bytes=0;
    for(int i=0;i<data.count;++i) {
        auto& header=data.header(i);const auto size=exr_layout(header);
        bytes+=size.width*size.height*size.channels*4;
        if(bytes>decoded_pixel_budget_bytes)throw std::runtime_error("Decoded EXR exceeds total pixel memory budget");
        layouts.push_back(size);headers.push_back(&header);
    }
    data.images.resize(data.count);
    for(auto& image:data.images)InitEXRImage(&image);
    if(version.multipart)
        data.check(LoadEXRMultipartImageFromFile(data.images.data(),headers.data(),data.count,path.c_str(),&data.error));
    else data.check(LoadEXRImageFromFile(data.images.data(),&data.single,path.c_str(),&data.error));
    std::ostringstream out;
    out<<"{\"status\":\"EXR_FULLY_DECODED\",\"decoded_pixel_bytes\":"<<bytes
       <<",\"decoded_pixel_budget_bytes\":"<<decoded_pixel_budget_bytes
       <<",\"input_budget_bytes\":"<<input_budget_bytes<<",\"parts\":[";
    for(int part=0;part<data.count;++part) {
        const auto& size=layouts[part];const auto& image=data.images[part];const auto& header=data.header(part);
        if(image.width!=size.width || image.height!=size.height || image.num_channels!=size.channels || !image.images)
            throw std::runtime_error("Decoded EXR dimensions disagree with header");
        if(part)out<<",";
        out<<"{\"name\":"<<std::quoted(exr_name(header.name))<<",\"width\":"<<size.width<<",\"height\":"<<size.height<<",\"channels\":[";
        for(int channel=0;channel<size.channels;++channel) {
            if(!image.images[channel])throw std::runtime_error("EXR channel pixels absent");
            uint64_t nonfinite=0;
            if(header.requested_pixel_types[channel]==TINYEXR_PIXELTYPE_FLOAT) {
                const auto* values=reinterpret_cast<const float*>(image.images[channel]);
                for(int64_t pixel=0;pixel<size.width*size.height;++pixel)nonfinite+=!std::isfinite(values[pixel]);
            }
            if(channel)out<<",";
            out<<"{\"name\":"<<std::quoted(exr_name(header.channels[channel].name))<<",\"nonfinite_pixels\":"<<nonfinite<<"}";
        }
        out<<"]}";
    }
    return out.str()+"]}";
}
}
