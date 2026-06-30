#include <chrono>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "sherpa/application/index_service.hpp"

namespace {

class TemporaryDirectory {
 public:
  TemporaryDirectory() {
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            ("sherpa-index-benchmark-" + std::to_string(suffix));
    std::filesystem::create_directories(path_);
  }

  ~TemporaryDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
  }

  [[nodiscard]] const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

void print_result(const char* phase, std::size_t iteration, const sherpa::IndexResult& result) {
  std::cout << phase << ',' << iteration << ',' << result.indexed_files << ','
            << result.parsed_files << ',' << result.unchanged_files << ','
            << result.scan_milliseconds << ',' << result.cache_load_milliseconds << ','
            << result.parse_milliseconds << ',' << result.relationship_milliseconds << ','
            << result.persistence_milliseconds << ',' << result.total_milliseconds << '\n';
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2 || argc > 3) {
    std::cerr << "usage: sherpa_index_benchmark <repository> [incremental-iterations]\n";
    return 2;
  }

  try {
    const std::filesystem::path repository = argv[1];
    const std::size_t iterations =
        argc == 3 ? static_cast<std::size_t>(std::stoul(argv[2])) : 5U;
    if (iterations == 0U) {
      throw std::invalid_argument("incremental iterations must be greater than zero");
    }

    TemporaryDirectory temporary;
    const auto database = temporary.path() / "index.sqlite";
    sherpa::IndexService service;

    std::cout << "phase,iteration,files,parsed,reused,scan_ms,cache_ms,parse_ms,"
                 "relationships_ms,persistence_ms,total_ms\n";
    print_result("full", 1, service.index({repository, database}));
    for (std::size_t iteration = 1; iteration <= iterations; ++iteration) {
      print_result("incremental", iteration, service.index({repository, database}));
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "benchmark failed: " << error.what() << '\n';
    return 1;
  }
}
