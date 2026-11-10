#pragma once
#include <filesystem>
#include <string>

namespace singular::media {
// Fully decode bounded scanline EXR parts and report channel sample validity.
std::string inspect_exr(const std::filesystem::path& path);
// Header-only admission before an external image decoder is launched.
std::string inspect_environment_exr(const std::filesystem::path& path);
}
