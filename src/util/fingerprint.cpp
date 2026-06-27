#include "sherpa/util/fingerprint.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace sherpa {
namespace {

constexpr std::uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

std::string to_hex(std::uint64_t value) {
  std::ostringstream stream;
  stream << std::hex << std::setfill('0') << std::setw(16) << value;
  return stream.str();
}

}  // namespace

std::string fingerprint(std::string_view value) {
  std::uint64_t result = kFnvOffsetBasis;
  for (const char character : value) {
    result ^= static_cast<unsigned char>(character);
    result *= kFnvPrime;
  }
  return to_hex(result);
}

std::string fingerprint_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("could not read file: " + path.string());
  }

  std::uint64_t result = kFnvOffsetBasis;
  std::array<char, 64 * 1024> buffer{};
  while (input) {
    input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const auto bytes_read = input.gcount();
    for (std::streamsize index = 0; index < bytes_read; ++index) {
      result ^= static_cast<unsigned char>(buffer[static_cast<std::size_t>(index)]);
      result *= kFnvPrime;
    }
  }

  if (!input.eof()) {
    throw std::runtime_error("failed while reading file: " + path.string());
  }
  return to_hex(result);
}

}  // namespace sherpa
