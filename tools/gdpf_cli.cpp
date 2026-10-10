// Command-line front end.
//
//   gdpf inspect <level>
//       Parse a level and report whether the simulator can handle it.
//
//   gdpf sim <level> [--inputs <file>] [--trace <file>] [--max-ticks <n>]
//       Run the simulator over an input script and optionally write a trace.
//
// <level> is a file holding a level string (decoded or encoded) or a .gmd
// export. An input script has one "<tick> <0|1>" pair per line: from that
// tick on, the button is up (0) or down (1). Lines starting with # are
// ignored.

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "gdpf/analyze.hpp"
#include "gdpf/constants.hpp"
#include "gdpf/level.hpp"
#include "gdpf/sim.hpp"
#include "gdpf/trace.hpp"

namespace {

int usage() {
    std::fprintf(stderr,
                 "usage:\n"
                 "  gdpf inspect <level>\n"
                 "  gdpf sim <level> [--inputs <file>] [--trace <file>] [--max-ticks <n>]\n");
    return 1;
}

bool read_file(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream buffer;
    buffer << in.rdbuf();
    out = buffer.str();
    return true;
}

bool load_level(const std::string& path, gdpf::Level& level) {
    std::string text;
    if (!read_file(path, text)) {
        std::fprintf(stderr, "error: cannot read %s\n", path.c_str());
        return false;
    }
    gdpf::ParseResult parsed = gdpf::parse_level(text);
    if (!parsed.ok) {
        std::fprintf(stderr, "error: %s\n", parsed.error.c_str());
        return false;
    }
    level = std::move(parsed.level);
    return true;
}

int inspect(const std::string& path) {
    gdpf::Level level;
    if (!load_level(path, level)) return 1;
    const gdpf::Analysis a = gdpf::analyze(level);

    if (!level.name.empty()) std::printf("name:     %s\n", level.name.c_str());
    std::printf("start:    %s, %s speed%s%s\n", std::string(gdpf::name(level.start.gamemode)).c_str(),
                std::string(gdpf::name(level.start.speed)).c_str(), level.start.mini ? ", mini" : "",
                level.start.flip_gravity ? ", gravity flipped" : "");
    std::printf("objects:  %zu\n", level.objects.size());
    for (int k = 0; k < gdpf::kObjKindCount; ++k) {
        const std::size_t count = a.kind_counts[static_cast<std::size_t>(k)];
        if (count == 0) continue;
        std::printf("  %-18s %zu\n", std::string(gdpf::name(static_cast<gdpf::ObjKind>(k))).c_str(), count);
    }
    std::printf("verdict:  %s\n", std::string(gdpf::name(a.verdict)).c_str());
    for (const gdpf::Issue& issue : a.issues) {
        std::printf("  %s %s\n", issue.blocking ? "[blocks]" : "[check] ", issue.message.c_str());
    }
    return a.verdict == gdpf::Verdict::Unsupported ? 2 : 0;
}

// Reads "<tick> <0|1>" lines into tick -> button state.
bool load_inputs(const std::string& path, std::map<std::uint32_t, bool>& inputs) {
    std::ifstream in(path);
    if (!in) {
        std::fprintf(stderr, "error: cannot read %s\n", path.c_str());
        return false;
    }
    std::string line;
    int line_no = 0;
    while (std::getline(in, line)) {
        ++line_no;
        const std::size_t start = line.find_first_not_of(" \t\r");
        if (start == std::string::npos || line[start] == '#') continue;
        std::istringstream fields(line);
        long tick = -1;
        int state = -1;
        if (!(fields >> tick >> state) || tick < 0 || (state != 0 && state != 1)) {
            std::fprintf(stderr, "error: %s:%d: expected \"<tick> <0|1>\"\n", path.c_str(), line_no);
            return false;
        }
        inputs[static_cast<std::uint32_t>(tick)] = state == 1;
    }
    return true;
}

int simulate(const std::string& path, const std::string& inputs_path, const std::string& trace_path,
             std::uint32_t max_ticks) {
    gdpf::Level level;
    if (!load_level(path, level)) return 1;

    std::map<std::uint32_t, bool> inputs;
    if (!inputs_path.empty() && !load_inputs(inputs_path, inputs)) return 1;

    std::ofstream trace;
    if (!trace_path.empty()) {
        trace.open(trace_path);
        if (!trace) {
            std::fprintf(stderr, "error: cannot write %s\n", trace_path.c_str());
            return 1;
        }
        gdpf::write_trace_header(trace);
    }

    const gdpf::World world(level);
    gdpf::PlayerState p = world.initial_state();
    if (trace.is_open()) gdpf::write_trace_row(trace, p);

    bool hold = false;
    while (p.running() && p.tick < max_ticks) {
        // p.tick is the tick just completed, so the next step is tick + 1.
        if (const auto it = inputs.find(p.tick + 1); it != inputs.end()) hold = it->second;
        world.step(p, hold);
        if (trace.is_open()) gdpf::write_trace_row(trace, p);
    }

    const double seconds = static_cast<double>(p.tick) / gdpf::constants::kTicksPerSecond;
    if (p.finished) {
        std::printf("finished at tick %u (%.3f s)\n", p.tick, seconds);
    } else if (p.dead) {
        std::printf("died at tick %u (%.3f s), x = %.2f, y = %.2f (%.1f%% of the level)\n", p.tick, seconds, p.x,
                    p.y, 100.0 * p.x / world.end_x());
    } else if (p.fault != gdpf::Fault::None) {
        std::printf("stopped at tick %u, x = %.2f: %s\n", p.tick, p.x, std::string(gdpf::describe(p.fault)).c_str());
    } else {
        std::printf("still running after %u ticks, x = %.2f\n", p.tick, p.x);
    }
    return p.finished ? 0 : 3;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) return usage();
    const std::string command = argv[1];
    const std::string level_path = argv[2];

    if (command == "inspect") return argc == 3 ? inspect(level_path) : usage();

    if (command == "sim") {
        std::string inputs_path;
        std::string trace_path;
        std::uint32_t max_ticks = 240 * 60 * 10;  // ten minutes of play
        for (int i = 3; i < argc; ++i) {
            const std::string flag = argv[i];
            if (i + 1 >= argc) return usage();
            const std::string value = argv[++i];
            if (flag == "--inputs") inputs_path = value;
            else if (flag == "--trace") trace_path = value;
            else if (flag == "--max-ticks") max_ticks = static_cast<std::uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
            else return usage();
        }
        return simulate(level_path, inputs_path, trace_path, max_ticks);
    }

    return usage();
}
