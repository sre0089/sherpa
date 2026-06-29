#include "sherpa/presentation/impact_renderer.hpp"

#include <ostream>
#include <string>
#include <vector>

#include "json_writer.hpp"

namespace sherpa {
namespace {

std::string display_name(const ImpactNode& node) {
  return node.symbol ? node.symbol->qualified_name : node.file_path;
}

void write_node_json(std::ostream& output, const ImpactNode& node) {
  using namespace presentation_detail;
  output << "{\"kind\":";
  write_json_string(output, to_string(node.kind));
  output << ",\"file\":";
  write_json_string(output, node.file_path);
  output << ",\"symbol\":";
  if (node.symbol) {
    write_symbol_json(output, *node.symbol);
  } else {
    output << "null";
  }
  output << '}';
}

void write_records_json(std::ostream& output, const std::vector<ImpactRecord>& records) {
  using namespace presentation_detail;
  output << '[';
  for (std::size_t index = 0; index < records.size(); ++index) {
    if (index != 0) {
      output << ',';
    }
    const auto& record = records[index];
    output << "{\"node\":";
    write_node_json(output, record.node);
    output << ",\"dependency\":";
    write_node_json(output, record.dependency);
    output << ",\"relationship\":";
    write_json_string(output, to_string(record.relationship));
    output << ",\"confidence\":";
    write_json_string(output, to_string(record.confidence));
    output << ",\"provenance\":";
    write_json_string(output, record.provenance);
    output << ",\"evidence\":";
    write_range_json(output, record.evidence);
    output << ",\"depth\":" << record.depth << '}';
  }
  output << ']';
}

void write_records_text(std::ostream& output, const std::vector<ImpactRecord>& records) {
  if (records.empty()) {
    output << "  None\n";
    return;
  }
  for (const auto& record : records) {
    output << "  " << display_name(record.node);
    if (record.node.symbol) {
      output << " (" << record.node.file_path << ')';
    }
    output << " --" << to_string(record.relationship) << "--> " << display_name(record.dependency)
           << " [depth " << record.depth << ", " << to_string(record.confidence) << "] at "
           << record.node.file_path << ':' << record.evidence.start_line << ':'
           << record.evidence.start_column << '\n';
  }
}

}  // namespace

void write_impact_json(std::ostream& output, const ImpactResult& result) {
  output << "{\"schema_version\":1,\"ok\":true,\"query\":\"impact\",\"target\":";
  write_node_json(output, result.target);
  output << ",\"confirmed\":";
  write_records_json(output, result.confirmed);
  output << ",\"possible\":";
  write_records_json(output, result.possible);
  output << "}\n";
}

void write_impact_text(std::ostream& output, const ImpactResult& result) {
  output << "Impact of " << to_string(result.target.kind) << ' ' << display_name(result.target)
         << '\n';
  output << "Confirmed impact (" << result.confirmed.size() << "):\n";
  write_records_text(output, result.confirmed);
  output << "Possible impact from ambiguous relationships (" << result.possible.size() << "):\n";
  write_records_text(output, result.possible);
}

}  // namespace sherpa
