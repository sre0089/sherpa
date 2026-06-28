#pragma once

#include <iosfwd>

#include "sherpa/domain/call_query.hpp"

namespace sherpa {

void write_call_query_json(std::ostream& output, const CallQueryResult& result);
void write_call_query_text(std::ostream& output, const CallQueryResult& result);

}  // namespace sherpa
