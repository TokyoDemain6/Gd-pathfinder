#include "gdpf/trace.hpp"

#include <cstdio>
#include <ostream>

namespace gdpf {

void write_trace_header(std::ostream& out) {
    out << "tick,x,y,y_vel,mode,speed,mini,flipped,on_ground,hold,dead\n";
}

void write_trace_row(std::ostream& out, const PlayerState& p) {
    char line[160];
    const int n = std::snprintf(line, sizeof(line), "%u,%.6f,%.6f,%.6f,%d,%d,%d,%d,%d,%d,%d\n",
                                static_cast<unsigned>(p.tick), p.x, p.y, p.y_vel, static_cast<int>(p.mode),
                                static_cast<int>(p.speed), p.mini ? 1 : 0, p.upside_down ? 1 : 0,
                                p.on_ground ? 1 : 0, p.button ? 1 : 0, p.dead ? 1 : 0);
    if (n > 0) out.write(line, n);
}

}  // namespace gdpf
