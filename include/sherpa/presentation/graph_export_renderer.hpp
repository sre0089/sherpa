#pragma once

#include <iosfwd>

#include "sherpa/domain/graph_export.hpp"

namespace sherpa {

void write_graph_export_json(std::ostream& output, const GraphExport& graph);

}  // namespace sherpa
