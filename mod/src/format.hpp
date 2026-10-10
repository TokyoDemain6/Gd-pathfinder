// Text the mod produces: file names, the level report and object dump rows.
//
// Deliberately free of Geode and game headers, so it can be built and tested
// off the game (tests/test_modformat.cpp).
#pragma once

#include <cstdio>
#include <string>
#include <string_view>

#include "gdpf/analyze.hpp"
#include "gdpf/level.hpp"

namespace pfmod {

// A file name (without extension) that is safe on every platform. Online
// levels get their ID appended so two levels with the same name do not
// overwrite each other.
inline std::string safe_file_name(std::string_view level_name, int level_id) {
    std::string out;
    for (char c : level_name) {
        const bool keep = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                          c == ' ' || c == '-' || c == '_';
        out.push_back(keep ? c : '_');
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    while (!out.empty() && out.front() == ' ') out.erase(out.begin());
    if (out.empty()) out = "level";
    if (out.size() > 60) out.resize(60);
    if (level_id > 0) out += "_" + std::to_string(level_id);
    return out;
}

// The popup text for a level, using the game's colour tags (<cg> green,
// <cy> yellow, <cr> red, closed by </c>).
inline std::string report_message(const gdpf::Level& level, const gdpf::Analysis& analysis,
                                  std::size_t max_issues = 5) {
    std::string out;
    switch (analysis.verdict) {
        case gdpf::Verdict::Supported:
            out = "<cg>Supported</c>: every object is one the simulator knows.";
            break;
        case gdpf::Verdict::Unverified:
            out = "<cy>Unverified</c>: nothing unsupported was found, but some objects are not in the "
                  "object table yet.";
            break;
        case gdpf::Verdict::Unsupported:
            out = "<cr>Unsupported</c>: this level uses things the simulator does not handle.";
            break;
    }
    out += "\n" + std::to_string(level.objects.size()) + " objects.";

    std::size_t shown = 0;
    for (const gdpf::Issue& issue : analysis.issues) {
        if (shown == max_issues) {
            out += "\n...and " + std::to_string(analysis.issues.size() - shown) + " more.";
            break;
        }
        std::string text = issue.message;
        if (text.size() > 110) text = text.substr(0, 107) + "...";
        out += "\n- " + text;
        ++shown;
    }
    return out;
}

// One object as the game sees it once the level is loaded.
struct ObjectRow {
    int id = 0;
    int type = 0;       // the game's GameObjectType
    double x = 0;       // current position
    double y = 0;
    double start_x = 0; // position the object was placed at
    double start_y = 0;
    double rotation = 0;
    double scale_x = 1;
    double scale_y = 1;
    bool flip_x = false;
    bool flip_y = false;
    double rect_x = 0;  // hitbox: left and bottom edge, then size
    double rect_y = 0;
    double rect_w = 0;
    double rect_h = 0;
    double radius = 0;  // above zero for circular hitboxes
    bool no_touch = false;
    bool passable = false;
    bool trigger = false;
};

inline constexpr const char* kObjectCsvHeader =
    "id,type,x,y,start_x,start_y,rotation,scale_x,scale_y,flip_x,flip_y,"
    "rect_x,rect_y,rect_w,rect_h,radius,no_touch,passable,trigger";

inline void append_object_row(std::string& out, const ObjectRow& r) {
    // %.10g keeps every digit of the game's 32-bit floats and has a bounded
    // length whatever the value, so a row can never be cut short.
    char line[512];
    const int n = std::snprintf(
        line, sizeof(line), "%d,%d,%.10g,%.10g,%.10g,%.10g,%.10g,%.10g,%.10g,%d,%d,%.10g,%.10g,%.10g,%.10g,%.10g,%d,%d,%d\n",
        r.id, r.type, r.x, r.y, r.start_x, r.start_y, r.rotation, r.scale_x, r.scale_y, r.flip_x ? 1 : 0,
        r.flip_y ? 1 : 0, r.rect_x, r.rect_y, r.rect_w, r.rect_h, r.radius, r.no_touch ? 1 : 0,
        r.passable ? 1 : 0, r.trigger ? 1 : 0);
    if (n > 0 && n < static_cast<int>(sizeof(line))) out.append(line, static_cast<std::size_t>(n));
}

}  // namespace pfmod
