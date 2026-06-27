#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace sherpa {

[[nodiscard]] std::string fingerprint(std::string_view value);
[[nodiscard]] std::string fingerprint_file(const std::filesystem::path& path);

}  // namespace sherpa
