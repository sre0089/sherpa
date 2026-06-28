#pragma once

#include <iosfwd>

#include "sherpa/domain/impact.hpp"

namespace sherpa {

void write_impact_json(std::ostream& output, const ImpactResult& result);
void write_impact_text(std::ostream& output, const ImpactResult& result);

}  // namespace sherpa
