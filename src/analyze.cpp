#include "gdpf/analyze.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

#include "gdpf/sim.hpp"

namespace gdpf {

std::string_view name(Verdict v) {
    switch (v) {
        case Verdict::Supported: return "supported";
        case Verdict::Unverified: return "unverified";
        case Verdict::Unsupported: return "unsupported";
    }
    return "?";
}

namespace {

std::string id_list(const std::map<int, std::size_t>& counts, std::size_t limit) {
    // Most frequent first.
    std::vector<std::pair<int, std::size_t>> sorted(counts.begin(), counts.end());
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const auto& a, const auto& b) { return a.second > b.second; });
    std::string out;
    for (std::size_t i = 0; i < sorted.size() && i < limit; ++i) {
        if (i != 0) out += ", ";
        out += std::to_string(sorted[i].first) + " x" + std::to_string(sorted[i].second);
    }
    if (sorted.size() > limit) out += ", ... " + std::to_string(sorted.size() - limit) + " more";
    return out;
}

bool on_grid(double rotation) {
    const double r = std::fmod(std::abs(rotation), 90.0);
    return r < 1e-6 || 90.0 - r < 1e-6;
}

}  // namespace

Analysis analyze(const Level& level, const ObjectTable& table) {
    Analysis a;
    const auto block = [&](std::string message) { a.issues.push_back({true, std::move(message)}); };

    if (level.start.platformer) block("platformer level");
    if (level.start.dual) block("starts in dual mode");
    if (level.start.gamemode == Gamemode::Spider) block("starts as the spider (spider teleport is not implemented)");

    std::map<std::string_view, std::map<int, std::size_t>> unsupported;  // reason -> id -> count
    std::map<int, std::size_t> unknown;
    std::size_t rotated_solids = 0;

    for (const LevelObject& obj : level.objects) {
        const ObjInfo* info = table.find(obj.id);
        if (info == nullptr || info->kind == ObjKind::Unknown) {
            ++a.kind_counts[static_cast<std::size_t>(ObjKind::Unknown)];
            ++unknown[obj.id];
            continue;
        }
        ++a.kind_counts[static_cast<std::size_t>(info->kind)];
        const std::string_view reason = unsupported_reason(*info);
        if (!reason.empty()) ++unsupported[reason][obj.id];
        if (info->kind == ObjKind::Solid && !on_grid(obj.rotation)) ++rotated_solids;
    }

    for (const auto& [reason, ids] : unsupported) {
        std::size_t total = 0;
        for (const auto& [id, count] : ids) total += count;
        block(std::string(reason) + ": " + std::to_string(total) + " object(s) (IDs " + id_list(ids, 8) + ")");
    }
    if (rotated_solids != 0) {
        block("solids rotated off the 90-degree grid: " + std::to_string(rotated_solids) + " object(s)");
    }

    a.unknown_ids = unknown.size();
    for (const auto& [id, count] : unknown) a.unknown_objects += count;
    if (a.unknown_objects != 0) {
        a.issues.push_back({false, std::to_string(a.unknown_objects) + " object(s) with " +
                                       std::to_string(a.unknown_ids) +
                                       " ID(s) missing from the object table, treated as decoration (IDs " +
                                       id_list(unknown, 12) + ")"});
    }

    const bool blocked = std::any_of(a.issues.begin(), a.issues.end(), [](const Issue& i) { return i.blocking; });
    a.verdict = blocked ? Verdict::Unsupported
                        : (a.unknown_objects != 0 ? Verdict::Unverified : Verdict::Supported);
    return a;
}

}  // namespace gdpf
