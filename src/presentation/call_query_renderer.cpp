#include "sherpa/presentation/call_query_renderer.hpp"

#include <iomanip>
#include <ostream>
#include <string>
#include <string_view>

namespace sherpa {
namespace {

void write_json_string(std::ostream& output, std::string_view value) {
  output << '"';
  for (const char raw_character : value) {
    const auto character = static_cast<unsigned char>(raw_character);
    switch (character) {
      case '"':
        output << "\\\"";
        break;
      case '\\':
        output << "\\\\";
        break;
      case '\b':
        output << "\\b";
        break;
      case '\f':
        output << "\\f";
        break;
      case '\n':
        output << "\\n";
        break;
      case '\r':
        output << "\\r";
        break;
      case '\t':
        output << "\\t";
        break;
      default:
        if (character < 0x20U) {
          output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                 << static_cast<unsigned int>(character) << std::dec << std::setfill(' ');
        } else {
          output << static_cast<char>(character);
        }
    }
  }
  output << '"';
}

void write_range_json(std::ostream& output, const SourceRange& range) {
  output << "{\"start\":{\"line\":" << range.start_line << ",\"column\":" << range.start_column
         << "},\"end\":{\"line\":" << range.end_line << ",\"column\":" << range.end_column << "}}";
}

void write_symbol_json(std::ostream& output, const QuerySymbol& symbol) {
  output << "{\"kind\":";
  write_json_string(output, symbol.kind);
  output << ",\"name\":";
  write_json_string(output, symbol.name);
  output << ",\"qualified_name\":";
  write_json_string(output, symbol.qualified_name);
  output << ",\"signature\":";
  write_json_string(output, symbol.signature);
  output << ",\"file\":";
  write_json_string(output, symbol.file_path);
  output << ",\"range\":";
  write_range_json(output, symbol.range);
  output << '}';
}

}  // namespace

void write_call_query_json(std::ostream& output, const CallQueryResult& result) {
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
