#pragma once

#include <iosfwd>
#include <string_view>
#include <vector>

#include "sherpa/domain/call_query.hpp"

namespace sherpa {

void write_query_error_json(std::ostream& output, std::string_view code,
                            std::string_view message,
                            const std::vector<QuerySymbol>& candidates = {});

}  // namespace sherpa
