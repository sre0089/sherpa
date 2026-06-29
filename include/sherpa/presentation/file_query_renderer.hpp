#pragma once

#include <iosfwd>

#include "sherpa/domain/file_query.hpp"

namespace sherpa {

void write_file_query_json(std::ostream& output, const FileQueryResult& result);
void write_file_query_text(std::ostream& output, const FileQueryResult& result);

}  // namespace sherpa
