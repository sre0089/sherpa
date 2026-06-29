#include "sherpa/presentation/path_renderer.hpp"

#include <ostream>

#include "json_writer.hpp"

namespace sherpa {

void write_path_json(std::ostream& output, const PathResult& result) {
  using namespace presentation_detail;
  output << "{\"schema_version\":1,\"ok\":true,\"query\":\"path\",\"source\":";
  write_symbol_json(output, result.source);
  output << ",\"target\":";
  write_symbol_json(output, result.target);
  output << ",\"status\":";
  write_json_string(output, to_string(result.status));
  output << ",\"steps\":[";
  for (std::size_t index = 0; index < result.steps.size(); ++index) {
    if (index != 0) {
      output << ',';
    }
    const auto& step = result.steps[index];
    output << "{\"source\":";
    write_symbol_json(output, step.source);
    output << ",\"target\":";
    write_symbol_json(output, step.target);
    output << ",\"resolution\":";
    write_json_string(output, to_string(step.resolution));
    output << ",\"confidence\":";
    write_json_string(output, to_string(step.confidence));
    output << ",\"provenance\":";
    write_json_string(output, step.provenance);
    output << ",\"evidence\":";
    write_range_json(output, step.evidence);
    output << '}';
  }
  output << "]}\n";
}

void write_path_text(std::ostream& output, const PathResult& result) {
  output << "Path from " << result.source.qualified_name << " to " << result.target.qualified_name
         << ": " << to_string(result.status);
  if (result.status == PathStatus::kNotFound) {
    output << "\n  No directed call path found\n";
    return;
  }
  output << " (" << result.steps.size() << " step";
  if (result.steps.size() != 1) {
    output << 's';
  }
  output << ")\n";
  for (std::size_t index = 0; index < result.steps.size(); ++index) {
    const auto& step = result.steps[index];
    output << "  " << index + 1 << ". " << step.source.qualified_name << " --calls--> "
           << step.target.qualified_name << " [" << to_string(step.resolution) << ", "
           << to_string(step.confidence) << "] at " << step.source.file_path << ':'
           << step.evidence.start_line << ':' << step.evidence.start_column << '\n';
  }
}

}  // namespace sherpa
