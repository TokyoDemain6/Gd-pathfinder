// Decides up front whether a level is inside what the simulator handles, so
// the tool can say "unsupported" instead of producing a path that dies.
#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "gdpf/level.hpp"
#include "gdpf/objects.hpp"

namespace gdpf {

enum class Verdict {
    Supported,    // every object is known and implemented
    Unverified,   // nothing known to be unsupported, but some object IDs are not in the table
    Unsupported,  // uses something the simulator does not implement
};

std::string_view name(Verdict v);

struct Issue {
    bool blocking = false;  // true: makes the level Unsupported
    std::string message;
};

struct Analysis {
    Verdict verdict = Verdict::Supported;
    std::vector<Issue> issues;
    std::array<std::size_t, kObjKindCount> kind_counts{};
    std::size_t unknown_objects = 0;  // objects whose ID is not in the table
    std::size_t unknown_ids = 0;      // distinct IDs among them
};

Analysis analyze(const Level& level, const ObjectTable& table = ObjectTable::builtin());

}  // namespace gdpf
