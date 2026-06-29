#include "sherpa/presentation/query_error_renderer.hpp"

#include <ostream>

#include "json_writer.hpp"

namespace sherpa {

void write_query_error_json(std::ostream& output, std::string_view code,
                            std::string_view message,
                            const std::vector<QuerySymbol>& candidates) {
  using namespace presentation_detail;
  output << "{\"schema_version\":1,\"ok\":false,\"error\":{\"code\":";
  write_json_string(output, code);
  output << ",\"message\":";
  write_json_string(output, message);
  output << ",\"candidates\":[";
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    if (index != 0) {
      output << ',';
    }
    write_symbol_json(output, candidates[index]);
  }
  output << "]}}\n";
}

}  // namespace sherpa
