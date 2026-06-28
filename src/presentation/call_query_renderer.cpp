#include "sherpa/presentation/call_query_renderer.hpp"

#include <ostream>
#include <string>

#include "json_writer.hpp"

namespace sherpa {

void write_call_query_json(std::ostream& output, const CallQueryResult& result) {
  using namespace presentation_detail;
  output << "{\"query\":";
  write_json_string(output, to_string(result.direction));
  output << ",\"symbol\":";
  write_symbol_json(output, result.symbol);
  output << ",\"calls\":[";
  for (std::size_t index = 0; index < result.calls.size(); ++index) {
    if (index != 0) {
      output << ',';
    }
    const auto& call = result.calls[index];
    output << "{\"caller\":";
    write_symbol_json(output, call.caller);
    output << ",\"callee\":";
    if (call.callee) {
      write_symbol_json(output, *call.callee);
    } else {
      output << "null";
    }
    output << ",\"target_text\":";
    write_json_string(output, call.target_text);
    output << ",\"resolution\":";
    write_json_string(output, to_string(call.resolution));
    output << ",\"confidence\":";
    write_json_string(output, to_string(call.confidence));
    output << ",\"provenance\":";
    write_json_string(output, call.provenance);
    output << ",\"evidence\":";
    write_range_json(output, call.evidence);
    output << ",\"candidates\":[";
    for (std::size_t candidate_index = 0; candidate_index < call.candidates.size();
         ++candidate_index) {
      if (candidate_index != 0) {
        output << ',';
      }
      const auto& candidate = call.candidates[candidate_index];
      output << "{\"symbol\":";
      write_symbol_json(output, candidate.symbol);
      output << ",\"reason\":";
      write_json_string(output, candidate.reason);
      output << ",\"rank\":" << candidate.rank << '}';
    }
    output << "]}";
  }
  output << "]}\n";
}

void write_call_query_text(std::ostream& output, const CallQueryResult& result) {
  const bool callers = result.direction == CallQueryDirection::kCallers;
  output << (callers ? "Callers" : "Callees") << " of " << result.symbol.qualified_name << " ("
         << result.symbol.file_path << ':' << result.symbol.range.start_line << ")\n";
  if (result.calls.empty()) {
    output << "  None\n";
    return;
  }
  for (const auto& call : result.calls) {
    const std::string& display_name =
        callers ? call.caller.qualified_name
                : (call.callee ? call.callee->qualified_name : call.target_text);
    output << "  " << display_name << " [" << to_string(call.resolution) << ", "
           << to_string(call.confidence) << "] at " << call.caller.file_path << ':'
           << call.evidence.start_line << ':' << call.evidence.start_column << '\n';
    for (const auto& candidate : call.candidates) {
      output << "    candidate " << candidate.rank << ": " << candidate.symbol.qualified_name
             << " (" << candidate.symbol.file_path << ':' << candidate.symbol.range.start_line
             << ")\n";
    }
  }
}

}  // namespace sherpa
