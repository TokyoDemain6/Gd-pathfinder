// Per-tick trace output, in the format described in docs/trace-format.md.
//
// The same format is what the in-game trace logger will write, so a trace from
// the simulator and a trace from the game can be compared line by line.
#pragma once

#include <iosfwd>

#include "gdpf/sim.hpp"

namespace gdpf {

void write_trace_header(std::ostream& out);
void write_trace_row(std::ostream& out, const PlayerState& p);

}  // namespace gdpf
