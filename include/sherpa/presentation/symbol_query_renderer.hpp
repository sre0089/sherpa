#pragma once

#include <iosfwd>

#include "sherpa/domain/symbol_query.hpp"

namespace sherpa {

void write_symbol_query_json(std::ostream& output, const SymbolQueryResult& result);
void write_symbol_query_text(std::ostream& output, const SymbolQueryResult& result);

}  // namespace sherpa
