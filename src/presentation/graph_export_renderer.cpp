#include "sherpa/presentation/graph_export_renderer.hpp"

#include <ostream>

#include "json_writer.hpp"

namespace sherpa {

void write_graph_export_json(std::ostream& output, const GraphExport& graph) {
  using namespace presentation_detail;
  output << "{\"schema_version\":" << graph.schema_version
         << ",\"generator\":{\"name\":\"sherpa\",\"version\":";
  write_json_string(output, graph.generator_version);
  output << "},\"repository\":{\"root\":";
  write_json_string(output, graph.repository_root);
  output << "},\"counts\":{\"nodes\":" << graph.nodes.size() << ",\"edges\":" << graph.edges.size()
         << "},\"nodes\":[";

  for (std::size_t index = 0; index < graph.nodes.size(); ++index) {
    if (index != 0) {
      output << ',';
    }
    const auto& node = graph.nodes[index];
    output << "{\"id\":";
    write_json_string(output, node.id);
    output << ",\"type\":";
    write_json_string(output, to_string(node.kind));
    output << ",\"file\":";
    write_json_string(output, node.file_path);
    if (node.symbol) {
      output << ",\"role\":\"definition\",\"symbol\":";
      write_symbol_json(output, *node.symbol);
    }
    output << '}';
  }

  output << "],\"edges\":[";
  for (std::size_t index = 0; index < graph.edges.size(); ++index) {
    if (index != 0) {
      output << ',';
    }
    const auto& edge = graph.edges[index];
    output << "{\"id\":";
    write_json_string(output, edge.id);
    output << ",\"type\":";
    write_json_string(output, to_string(edge.kind));
    output << ",\"source\":";
    write_json_string(output, edge.source_id);
    output << ",\"target\":";
    if (edge.target_id) {
      write_json_string(output, *edge.target_id);
    } else {
      output << "null";
    }
    output << ",\"target_text\":";
    write_json_string(output, edge.target_text);
    output << ",\"resolution\":";
    write_json_string(output, to_string(edge.resolution));
    output << ",\"confidence\":";
    write_json_string(output, to_string(edge.confidence));
    output << ",\"provenance\":";
    write_json_string(output, edge.provenance);
    output << ",\"evidence\":";
    write_range_json(output, edge.evidence);
    output << ",\"candidates\":[";
    for (std::size_t candidate_index = 0; candidate_index < edge.candidates.size();
         ++candidate_index) {
      if (candidate_index != 0) {
        output << ',';
      }
      const auto& candidate = edge.candidates[candidate_index];
      output << "{\"node\":";
      write_json_string(output, candidate.node_id);
      output << ",\"reason\":";
      write_json_string(output, candidate.reason);
      output << ",\"rank\":" << candidate.rank << '}';
    }
    output << "]}";
  }
  output << "]}\n";
}

}  // namespace sherpa
