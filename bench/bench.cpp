// The measurements behind the performance checks (see bench/README.md).
//
//   gdpf_bench info                  sizes, object-table counts, compiler
//   gdpf_bench scenarios             one result line per mechanic scenario
//   gdpf_bench time [reps]           wall-clock time per case (best of reps)
//   gdpf_bench work                  collision work per case (gdpf_bench_stats only)
//   gdpf_bench cases                 the cases `time`, `work` and `run` use
//   gdpf_bench run <case> <k>        run one case for k batches and exit
//
// Every command prints JSON. `run` exists so an outside tool (callgrind) can
// count the instructions a case costs: run it for one batch and for two, and
// the difference is one batch with all the setup cancelled out.
//
// Everything here is deterministic: inputs come from a fixed pseudo-random
// sequence, never from the clock.

#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "gdpf/level.hpp"
#include "gdpf/objects.hpp"
#include "gdpf/sim.hpp"

using namespace gdpf;

namespace {

// ------------------------------------------------------------- helpers ---

// xorshift64: the same sequence on every platform.
struct Rng {
    std::uint64_t s = 0x9E3779B97F4A7C15ull;
    std::uint64_t next() {
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        return s;
    }
    std::uint64_t below(std::uint64_t n) { return next() % n; }
};

// FNV-1a over 64-bit words.
struct Hash {
    std::uint64_t h = 1469598103934665603ull;
    void add(std::uint64_t v) { h = (h ^ v) * 1099511628211ull; }
    void add(double v) {
        std::uint64_t bits;
        std::memcpy(&bits, &v, sizeof(bits));
        add(bits);
    }
};

std::string obj(int id, int x, int y, const std::string& extra = "") {
    return "1," + std::to_string(id) + ",2," + std::to_string(x) + ",3," + std::to_string(y) + extra + ";";
}

World make_world(const std::string& text) {
    ParseResult r = parse_level(text);
    if (!r.ok) {
        std::fprintf(stderr, "bench: level did not parse: %s\n", r.error.c_str());
        std::exit(2);
    }
    return World(r.level);
}

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "bench: cannot read %s\n", path.c_str());
        std::exit(2);
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

// A block far to the right and far above, so a level is long without putting
// anything in the player's way.
std::string far_block(int x) {
    return obj(1, x, 3000);
}

// --------------------------------------------------------------- levels ---

// `columns` grid columns with `per_column` objects stacked in each, all above
// `min_y`: out of the player's reach, but in the columns it passes through.
// This is what decoration costs the collision code.
std::string corridor(int mode, int columns, int per_column, int min_y) {
    std::string t = "kA2," + std::to_string(mode) + ";";
    for (int c = 0; c < columns; ++c) {
        for (int k = 0; k < per_column; ++k) {
            const int id = k % 3 == 0 ? 1 : (k % 3 == 1 ? 8 : 36);  // block, spike, orb
            t += obj(id, 15 + 30 * c, min_y + 30 * k);
        }
    }
    return t + far_block(30 * columns);
}

// A cube level with something to deal with every few blocks: a spike, a
// block to hop onto, or a low spike pair, plus decoration overhead.
std::string obstacle_course(int sections) {
    Rng rng;
    std::string t = "kA2,0;";
    int x = 600;
    for (int i = 0; i < sections; ++i) {
        switch (rng.below(3)) {
            case 0: t += obj(8, x, 15); break;
            case 1: t += obj(1, x, 15) + obj(1, x + 30, 15); break;
            default: t += obj(39, x, 15) + obj(39, x + 30, 15); break;
        }
        for (int k = 0; k < 4; ++k) t += obj(k % 2 == 0 ? 1 : 8, x + 30 * static_cast<int>(rng.below(8)), 255 + 30 * k);
        x += 240 + 30 * static_cast<int>(rng.below(4));
    }
    return t + far_block(x + 300);
}

// A big decoded level string, to time the parser and World construction.
std::string big_level_text(int objects) {
    static const int ids[] = {1, 1, 1, 8, 39, 103, 36, 35, 12, 13, 200, 1332, 9999};
    static const int rotations[] = {0, 0, 90, 180, 270, 45};
    static const char* const half_steps[] = {"", "", ".5", ""};
    static const int offsets[] = {0, 0, 7, 15};
    Rng rng;
    std::string t = "kA13,0,kA15,0,kA16,0,kA2,1,kA4,3;";
    t.reserve(static_cast<std::size_t>(objects) * 44);
    for (int i = 0; i < objects; ++i) {
        const std::uint64_t pick = rng.below(4);
        t += "1," + std::to_string(ids[rng.below(13)]);
        t += ",2," + std::to_string(15 + 30 * (i / 6) + offsets[pick]) + half_steps[pick];
        t += ",3," + std::to_string(15 + 30 * static_cast<int>(rng.below(9)));
        t += ",6," + std::to_string(rotations[rng.below(6)]);
        t += ",21," + std::to_string(1 + rng.below(40));
        t += ",57," + std::to_string(1 + rng.below(98)) + "." + std::to_string(1 + rng.below(98)) + ";";
    }
    return t;
}

// ---------------------------------------------------------------- cases ---

// A case does `n` iterations of something and returns a checksum (so the
// work cannot be optimised away). `prepare` builds whatever the loop needs;
// only the loop is timed.
using Loop = std::function<std::uint64_t(std::uint64_t n)>;

struct Case {
    std::string name;
    std::string unit;           // what one iteration is: "tick" or "load"
    std::string what;           // one line for the report
    std::uint64_t timed_n;      // iterations per timing run
    std::uint64_t counted_n;    // iterations in one batch, for instruction and work counts
    std::function<Loop()> prepare;
};

// Steps a level with random input, starting over whenever the run ends.
// `flip_every`: on average the button changes once per that many ticks.
Loop random_play(const std::string& level_text, std::uint64_t flip_every) {
    auto world = std::make_shared<World>(make_world(level_text));
    return [world, flip_every](std::uint64_t n) {
        Rng rng;
        Hash hash;
        PlayerState p = world->initial_state();
        bool hold = false;
        for (std::uint64_t i = 0; i < n; ++i) {
            if (rng.below(flip_every) == 0) hold = !hold;
            world->step(p, hold);
            if (!p.running()) {
                hash.add(p.x);
                p = world->initial_state();
                hold = false;
            }
        }
        hash.add(p.y);
        return hash.h;
    };
}

// What a search does: from a saved state, try a short burst of random input.
// If the player survives it, save the new state and carry on from there; if
// a saved state has been tried too often, give up on it and go back to the
// one before. Exercises stepping next to obstacles and copying states.
Loop checkpoint_search(const std::string& level_text) {
    auto world = std::make_shared<World>(make_world(level_text));
    return [world](std::uint64_t n) {
        constexpr std::uint32_t kBurst = 40;  // ticks per try
        constexpr std::uint32_t kTries = 6;   // tries from one saved state before giving up on it
        struct Saved {
            PlayerState state;
            std::uint32_t tries = 0;
        };
        Rng rng;
        Hash hash;
        std::vector<Saved> saved{{world->initial_state()}};
        std::uint64_t done = 0;
        while (done < n) {
            if (saved.back().tries == kTries) {
                if (saved.size() > 1) saved.pop_back();
                else saved.back().tries = 0;
                continue;
            }
            ++saved.back().tries;
            PlayerState p = saved.back().state;
            bool hold = false;
            for (std::uint32_t i = 0; i < kBurst && p.running() && done < n; ++i, ++done) {
                if (rng.below(12) == 0) hold = !hold;
                world->step(p, hold);
            }
            if (p.running()) {
                saved.push_back({p});
            } else if (p.finished) {
                hash.add(static_cast<std::uint64_t>(p.tick));
                saved.assign(1, Saved{world->initial_state()});
            }
        }
        hash.add(saved.back().state.x);
        hash.add(static_cast<std::uint64_t>(saved.size()));
        return hash.h;
    };
}

std::vector<Case> all_cases() {
    constexpr int kColumns = 3000;
    constexpr int kAbove = 345;  // above every gamemode's corridor
    std::vector<Case> cases;
    const auto sim = [&](std::string name, std::string what, std::uint64_t timed_n, std::function<Loop()> prepare,
                         std::uint64_t counted_n = 20000) {
        cases.push_back({std::move(name), "tick", std::move(what), timed_n, counted_n, std::move(prepare)});
    };
    sim("cube_empty", "cube, nothing nearby", 2000000, [] { return random_play(corridor(0, kColumns, 0, kAbove), 40); });
    sim("cube_light", "cube, 2 objects per column overhead", 1000000,
        [] { return random_play(corridor(0, kColumns, 2, kAbove), 40); });
    sim("cube_dense", "cube, 10 objects per column overhead", 400000,
        [] { return random_play(corridor(0, kColumns, 10, kAbove), 40); });
    sim("cube_packed", "cube, 30 objects per column overhead", 150000,
        [] { return random_play(corridor(0, kColumns, 30, kAbove), 40); });
    sim("ship_empty", "ship, nothing nearby", 2000000, [] { return random_play(corridor(1, kColumns, 0, kAbove), 12); });
    sim("ship_dense", "ship, 10 objects per column overhead", 400000,
        [] { return random_play(corridor(1, kColumns, 10, kAbove), 12); });
    sim("wave_dense", "wave, 10 objects per column overhead", 400000,
        [] { return random_play(corridor(4, kColumns, 10, kAbove), 12); });
    sim("course_search", "cube, obstacle course, try-and-rewind search", 1000000,
        [] { return checkpoint_search(obstacle_course(400)); }, 100000);

    cases.push_back({"parse_decoded", "load", "parse a 50,000-object level string", 3, 1, [] {
                         auto text = std::make_shared<std::string>(big_level_text(50000));
                         return Loop([text](std::uint64_t n) {
                             std::uint64_t sum = 0;
                             for (std::uint64_t i = 0; i < n; ++i) sum += parse_level(*text).level.objects.size();
                             return sum;
                         });
                     }});
    cases.push_back({"parse_encoded", "load", "decode and parse the 4,000-object test level", 20, 4, [] {
                         auto text = std::make_shared<std::string>(read_file(std::string(GDPF_FIXTURES) + "/big.gzip.txt"));
                         return Loop([text](std::uint64_t n) {
                             std::uint64_t sum = 0;
                             for (std::uint64_t i = 0; i < n; ++i) sum += parse_level(*text).level.objects.size();
                             return sum;
                         });
                     }});
    cases.push_back({"world_build", "load", "build the collision world for 50,000 objects", 10, 2, [] {
                         auto level = std::make_shared<Level>(parse_level(big_level_text(50000)).level);
                         return Loop([level](std::uint64_t n) {
                             std::uint64_t sum = 0;
                             for (std::uint64_t i = 0; i < n; ++i) sum += World(*level).colliders().size();
                             return sum;
                         });
                     }});
    return cases;
}

// ------------------------------------------------------------ scenarios ---

const char* fault_name(Fault f) {
    switch (f) {
        case Fault::None: return "None";
        case Fault::Slope: return "Slope";
        case Fault::RotatedSolid: return "RotatedSolid";
        case Fault::SpiderJump: return "SpiderJump";
        case Fault::DashOrb: return "DashOrb";
        case Fault::SpiderOrb: return "SpiderOrb";
        case Fault::TeleportOrb: return "TeleportOrb";
        case Fault::ToggleOrb: return "ToggleOrb";
        case Fault::SpiderPad: return "SpiderPad";
        case Fault::TeleportPortal: return "TeleportPortal";
        case Fault::DualPortal: return "DualPortal";
        case Fault::UnknownBoost: return "UnknownBoost";
    }
    return "Other";
}

struct Scenario {
    std::string name;
    std::string level;
    bool start_in_air = false;     // start at (400, 150), not standing at x = 0
    std::uint32_t press_tick = 0;  // hold the button for two ticks from here (0 = never)
    std::uint64_t flip_every = 0;  // random input, as in random_play (0 = none)
    std::uint32_t ticks = 600;
};

// One small level per (mechanic, gamemode) pair, plus free play in every
// gamemode. Nothing is rotated off the grid, so no result depends on how a
// platform's maths library rounds sin() and cos().
std::vector<Scenario> all_scenarios() {
    static const char* const modes[] = {"cube", "ship", "ball", "ufo", "wave", "robot", "spider", "swing"};
    static const std::uint64_t free_flip[] = {40, 12, 60, 30, 12, 50, 0, 30};  // spider: any click stops the run
    struct Named {
        const char* name;
        int id;
    };
    static const Named pads[] = {{"yellow", 35}, {"pink", 140}, {"red", 1332}, {"blue", 67}, {"spider", 3005}};
    static const Named orbs[] = {{"yellow", 36},   {"pink", 141},  {"red", 1333},  {"blue", 84},      {"green", 1022},
                                 {"black", 1330},  {"dash", 1704}, {"spider", 3004}, {"teleport", 3027}, {"toggle", 1594}};
    static const Named portals[] = {
        {"gravity-normal", 10}, {"gravity-flipped", 11}, {"gravity-toggle", 2926}, {"size-normal", 99},
        {"size-mini", 101},     {"speed-0.5x", 200},     {"speed-1x", 201},        {"speed-2x", 202},
        {"speed-3x", 203},      {"speed-4x", 1334},      {"to-cube", 12},          {"to-ship", 13},
        {"to-ball", 47},        {"to-ufo", 111},         {"to-wave", 660},         {"to-robot", 745},
        {"to-spider", 1331},    {"to-swing", 1933},      {"dual-on", 286},         {"dual-off", 287},
        {"teleport", 747},
    };
    static const Named speeds[] = {{"0.5x", 1}, {"2x", 2}, {"3x", 3}, {"4x", 4}};  // kA4 values

    const std::string far = far_block(60000);
    std::vector<Scenario> out;
    for (int m = 0; m < 8; ++m) {
        const std::string mode = modes[m];
        const std::string head = "kA2," + std::to_string(m);

        out.push_back({"free/" + mode, head + ";" + far, false, 0, free_flip[m], 3000});
        out.push_back({"free/" + mode + "/mini", head + ",kA3,1;" + far, false, 0, free_flip[m], 3000});
        out.push_back({"free/" + mode + "/flipped", head + ",kA11,1;" + far, false, 0, free_flip[m], 3000});
        if (m == 0) {
            for (const Named& s : speeds) {
                out.push_back({"free/cube/" + std::string(s.name), head + ",kA4," + std::to_string(s.id) + ";" + far, false, 0,
                               free_flip[m], 3000});
            }
        }
        // A pad on the floor, walked into.
        for (const Named& pad : pads) {
            out.push_back({"pad/" + std::string(pad.name) + "/" + mode, head + ";" + obj(pad.id, 300, 3) + far});
        }
        // An orb scaled up to fill the screen, clicked from mid-air.
        for (const Named& orb : orbs) {
            out.push_back({"orb/" + std::string(orb.name) + "/" + mode, head + ";" + obj(orb.id, 500, 150, ",32,10") + far,
                           true, 3});
        }
        // A portal standing on the floor, walked into.
        for (const Named& portal : portals) {
            out.push_back({"portal/" + std::string(portal.name) + "/" + mode, head + ";" + obj(portal.id, 300, 45) + far});
        }
    }
    return out;
}

// "<how it ended>@<tick>#<hash of every tick's state>"
std::string run_scenario(const Scenario& s) {
    const World world = make_world(s.level);
    PlayerState p = world.initial_state();
    if (s.start_in_air) {
        p.x = 400;
        p.y = 150;
        p.on_ground = false;
    }
    Rng rng;
    Hash hash;
    bool hold = false;
    while (p.running() && p.tick < s.ticks) {
        const std::uint32_t next = p.tick + 1;
        if (s.flip_every != 0 && rng.below(s.flip_every) == 0) hold = !hold;
        if (s.press_tick != 0) hold = next == s.press_tick || next == s.press_tick + 1;
        world.step(p, hold);
        hash.add(p.x);
        hash.add(p.y);
        hash.add(p.y_vel);
        const auto bit = [](auto value, int shift) { return static_cast<std::uint64_t>(value) << shift; };
        hash.add(bit(p.mode, 0) | bit(p.speed, 8) | bit(p.mini, 16) | bit(p.upside_down, 17) | bit(p.on_ground, 18) |
                 bit(p.dead, 19) | bit(p.finished, 20) | bit(p.fault, 24));
    }
    std::string ended = "ok";
    if (p.dead) ended = "dead";
    else if (p.finished) ended = "finished";
    else if (p.fault != Fault::None) ended = std::string("fault:") + fault_name(p.fault);
    char tail[48];
    std::snprintf(tail, sizeof(tail), "@%u#%016" PRIx64, static_cast<unsigned>(p.tick), hash.h);
    return ended + tail;
}

// ------------------------------------------------------------- commands ---

std::string compiler_name() {
#if defined(__clang__)
    return std::string("clang ") + __clang_version__;
#elif defined(__GNUC__)
    return std::string("gcc ") + __VERSION__;
#elif defined(_MSC_VER)
    return "msvc " + std::to_string(_MSC_FULL_VER);
#else
    return "unknown";
#endif
}

int cmd_info() {
    const ObjectTable& table = ObjectTable::builtin();
    std::map<std::string, int> kinds;
    for (int id = 1; id <= 20000; ++id) {
        if (const ObjInfo* info = table.find(id)) ++kinds[std::string(name(info->kind))];
    }
    std::printf("{\n  \"compiler\": \"%s\",\n", compiler_name().c_str());
#ifdef GDPF_STATS
    std::printf("  \"stats\": true,\n");
#else
    std::printf("  \"stats\": false,\n");
#endif
    std::printf("  \"sizes\": {\"PlayerState\": %zu, \"Collider\": %zu},\n", sizeof(PlayerState), sizeof(Collider));
    std::printf("  \"object_table\": {\"total\": %zu", table.size());
    for (const auto& [kind, count] : kinds) std::printf(", \"%s\": %d", kind.c_str(), count);
    std::printf("}\n}\n");
    return 0;
}

int cmd_scenarios() {
    const std::vector<Scenario> scenarios = all_scenarios();
    std::printf("{\n");
    for (std::size_t i = 0; i < scenarios.size(); ++i) {
        std::printf("  \"%s\": \"%s\"%s\n", scenarios[i].name.c_str(), run_scenario(scenarios[i]).c_str(),
                    i + 1 == scenarios.size() ? "" : ",");
    }
    std::printf("}\n");
    return 0;
}

int cmd_time(int reps) {
    using Clock = std::chrono::steady_clock;
    const std::vector<Case> cases = all_cases();
    std::printf("{\n");
    for (std::size_t i = 0; i < cases.size(); ++i) {
        const Case& c = cases[i];
        const Loop loop = c.prepare();
        double best = 1e300;
        std::uint64_t sum = 0;
        for (int r = 0; r < reps; ++r) {
            const auto t0 = Clock::now();
            sum += loop(c.timed_n);
            const auto t1 = Clock::now();
            best = std::min(best, std::chrono::duration<double, std::nano>(t1 - t0).count());
        }
        std::printf("  \"%s\": {\"unit\": \"%s\", \"what\": \"%s\", \"ns\": %.3f, \"checksum\": \"%016" PRIx64 "\"}%s\n",
                    c.name.c_str(), c.unit.c_str(), c.what.c_str(), best / static_cast<double>(c.timed_n), sum,
                    i + 1 == cases.size() ? "" : ",");
    }
    std::printf("}\n");
    return 0;
}

int cmd_work() {
#ifdef GDPF_STATS
    std::printf("{\n");
    bool first = true;
    for (const Case& c : all_cases()) {
        if (c.unit != "tick") continue;
        const Loop loop = c.prepare();
        sim_stats() = SimStats{};
        loop(c.counted_n);
        std::printf("%s  \"%s\": {\"ticks\": %" PRIu64 ", \"collider_visits\": %" PRIu64 ", \"overlap_tests\": %" PRIu64 "}",
                    first ? "" : ",\n", c.name.c_str(), c.counted_n, sim_stats().collider_visits,
                    sim_stats().overlap_tests);
        first = false;
    }
    std::printf("\n}\n");
    return 0;
#else
    std::fprintf(stderr, "bench: `work` needs the counters; run gdpf_bench_stats instead\n");
    return 2;
#endif
}

int cmd_run(const std::string& name, std::uint64_t n) {
    for (const Case& c : all_cases()) {
        if (c.name != name) continue;
        const std::uint64_t sum = c.prepare()(n * c.counted_n);
        std::printf("{\"case\": \"%s\", \"iterations\": %" PRIu64 ", \"checksum\": \"%016" PRIx64 "\"}\n", name.c_str(),
                    n * c.counted_n, sum);
        return 0;
    }
    std::fprintf(stderr, "bench: no case named %s\n", name.c_str());
    return 2;
}

int cmd_cases() {
    const std::vector<Case> cases = all_cases();
    std::printf("{\n");
    for (std::size_t i = 0; i < cases.size(); ++i) {
        std::printf("  \"%s\": {\"unit\": \"%s\", \"what\": \"%s\", \"counted_n\": %" PRIu64 "}%s\n", cases[i].name.c_str(),
                    cases[i].unit.c_str(), cases[i].what.c_str(), cases[i].counted_n, i + 1 == cases.size() ? "" : ",");
    }
    std::printf("}\n");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string command = argc > 1 ? argv[1] : "";
    if (command == "info") return cmd_info();
    if (command == "cases") return cmd_cases();
    if (command == "scenarios") return cmd_scenarios();
    if (command == "work") return cmd_work();
    if (command == "time") return cmd_time(argc > 2 ? std::max(1, std::atoi(argv[2])) : 5);
    if (command == "run" && argc == 4) return cmd_run(argv[2], std::strtoull(argv[3], nullptr, 10));
    std::fprintf(stderr, "usage: gdpf_bench info | cases | scenarios | work | time [reps] | run <case> <batches>\n");
    return 2;
}
