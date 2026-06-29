#include "sherpa/presentation/file_query_renderer.hpp"

#include <ostream>
#include <string>

#include "json_writer.hpp"

namespace sherpa {
namespace {

void write_file_query_candidate_json(std::ostream& output, const FileQueryCandidate& candidate) {
  using namespace presentation_detail;
  output << "{\"path\":";
  write_json_string(output, candidate.path);
  output << ",\"reason\":";
  write_json_string(output, candidate.reason);
  output << ",\"rank\":" << candidate.rank << '}';
}

void write_file_include_json(std::ostream& output, const FileIncludeRecord& record) {
  using namespace presentation_detail;
  output << "{\"source\":";
  write_json_string(output, record.source_path);
  output << ",\"target\":";
  if (record.target_path) {
    write_json_string(output, *record.target_path);
  } else {
    output << "null";
  }
  output << ",\"target_text\":";
  write_json_string(output, record.target_text);
  output << ",\"resolution\":";
  write_json_string(output, to_string(record.resolution));
  output << ",\"confidence\":";
  write_json_string(output, to_string(record.confidence));
  output << ",\"provenance\":";
  write_json_string(output, record.provenance);
  output << ",\"evidence\":";
  write_range_json(output, record.evidence);
  output << ",\"candidates\":[";
  for (std::size_t index = 0; index < record.candidates.size(); ++index) {
    if (index != 0) {
      output << ',';
    }
    write_file_query_candidate_json(output, record.candidates[index]);
  }
  output << "]}";
}

std::string display_include_target(const FileIncludeRecord& record) {
  return record.target_path ? *record.target_path : record.target_text;
}

void write_include_section(std::ostream& output, const char* heading,
                           const std::vector<FileIncludeRecord>& records) {
  output << "  " << heading << ":\n";
  if (records.empty()) {
    output << "    None\n";
    return;
  }
  for (const auto& record : records) {
    output << "    " << display_include_target(record) << " [" << to_string(record.resolution)
           << ", " << to_string(record.confidence) << "] at " << record.source_path << ':'
           << record.evidence.start_line << ':' << record.evidence.start_column << '\n';
    for (const auto& candidate : record.candidates) {
      output << "      candidate " << candidate.rank << ": " << candidate.path << '\n';
    }
  }
}

}  // namespace

void write_file_query_json(std::ostream& output, const FileQueryResult& result) {
  using namespace presentation_detail;
  output << "{\"schema_version\":1,\"ok\":true,\"query\":\"file\",\"path\":";
  write_json_string(output, result.path);
  output << ",\"definitions\":[";
  for (std::size_t index = 0; index < result.definitions.size(); ++index) {
    if (index != 0) {
      output << ',';
    }
    write_symbol_json(output, result.definitions[index]);
  }
  output << "],\"incoming_includes\":[";
  for (std::size_t index = 0; index < result.incoming_includes.size(); ++index) {
    if (index != 0) {
      output << ',';
    }
    write_file_include_json(output, result.incoming_includes[index]);
  }
  output << "],\"outgoing_includes\":[";
  for (std::size_t index = 0; index < result.outgoing_includes.size(); ++index) {
    if (index != 0) {
      output << ',';
    }
    write_file_include_json(output, result.outgoing_includes[index]);
  }
  output << "]}\n";
}

void write_file_query_text(std::ostream& output, const FileQueryResult& result) {
  output << "File " << result.path << '\n';
  output << "  Definitions:\n";
  if (result.definitions.empty()) {
    output << "    None\n";
  } else {
    for (const auto& definition : result.definitions) {
      output << "    " << definition.qualified_name << " (" << definition.signature << ") at "
             << definition.file_path << ':' << definition.range.start_line << ':'
             << definition.range.start_column << '\n';
    }
  }
  write_include_section(output, "Incoming includes", result.incoming_includes);
  write_include_section(output, "Outgoing includes", result.outgoing_includes);
}

}  // namespace sherpa
