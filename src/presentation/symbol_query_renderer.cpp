#include "sherpa/presentation/symbol_query_renderer.hpp"

#include <ostream>

#include "json_writer.hpp"

namespace sherpa {

void write_symbol_query_json(std::ostream& output, const SymbolQueryResult& result) {
  using namespace presentation_detail;
  output << "{\"query\":\"symbol\",\"symbol\":";
  write_symbol_json(output, result.symbol);
  output << ",\"callers\":{\"resolved\":" << result.callers.resolved
         << ",\"ambiguous\":" << result.callers.ambiguous
         << ",\"unresolved\":" << result.callers.unresolved << "},\"callees\":{\"resolved\":"
         << result.callees.resolved << ",\"ambiguous\":" << result.callees.ambiguous
         << ",\"unresolved\":" << result.callees.unresolved << "}}\n";
}

void write_symbol_query_text(std::ostream& output, const SymbolQueryResult& result) {
  output << "Symbol " << result.symbol.qualified_name << '\n';
  output << "  Kind: " << result.symbol.kind << '\n';
  output << "  Signature: " << result.symbol.signature << '\n';
  output << "  Defined at: " << result.symbol.file_path << ':' << result.symbol.range.start_line
         << ':' << result.symbol.range.start_column << '\n';
  output << "  Callers: " << result.callers.resolved << " resolved, "
         << result.callers.ambiguous << " ambiguous\n";
  output << "  Callees: " << result.callees.resolved << " resolved, "
         << result.callees.ambiguous << " ambiguous, " << result.callees.unresolved
         << " unresolved\n";
}

}  // namespace sherpa
