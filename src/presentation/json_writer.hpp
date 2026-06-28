#pragma once

#include <iomanip>
#include <ostream>
#include <string_view>

#include "sherpa/domain/call_query.hpp"

namespace sherpa::presentation_detail {

inline void write_json_string(std::ostream& output, std::string_view value) {
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

inline void write_range_json(std::ostream& output, const SourceRange& range) {
  output << "{\"start\":{\"line\":" << range.start_line << ",\"column\":" << range.start_column
         << "},\"end\":{\"line\":" << range.end_line << ",\"column\":" << range.end_column << "}}";
}

inline void write_symbol_json(std::ostream& output, const QuerySymbol& symbol) {
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

}  // namespace sherpa::presentation_detail
