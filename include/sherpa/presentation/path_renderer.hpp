#pragma once

#include <iosfwd>

#include "sherpa/domain/path_query.hpp"

namespace sherpa {

void write_path_json(std::ostream& output, const PathResult& result);
void write_path_text(std::ostream& output, const PathResult& result);

}  // namespace sherpa
