// Level string parsing: text in, a list of objects out.
//
// A decoded level string looks like
//
//     kS38,...,kA2,0,kA4,0,...;1,1,2,15,3,15;1,8,2,45,3,15,6,90;...
//
// Segments are separated by ';'. The first is the level header (keys start
// with "k"). Every other segment is one object, written as key,value pairs.
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "gdpf/types.hpp"

namespace gdpf {

// Object keys used by the parser. See docs/level-format.md.
namespace key {
inline constexpr int Id = 1;
inline constexpr int X = 2;
inline constexpr int Y = 3;
inline constexpr int FlipX = 4;
inline constexpr int FlipY = 5;
inline constexpr int Rotation = 6;
inline constexpr int Scale = 32;
inline constexpr int Groups = 57;
inline constexpr int MultiActivate = 99;
inline constexpr int ScaleX = 128;
inline constexpr int ScaleY = 129;
}  // namespace key

// Starting state for a run. Comes from the level header, or from a start
// position object, which carries the same keys.
struct StartSettings {
    Gamemode gamemode = Gamemode::Cube;  // kA2
    Speed speed = Speed::Normal;         // kA4
    bool mini = false;                   // kA3
    bool dual = false;                   // kA8
    bool two_player = false;             // kA10
    bool flip_gravity = false;           // kA11
    bool platformer = false;             // kA22
};

struct LevelObject {
    int id = 0;
    double x = 0;
    double y = 0;
    double rotation = 0;  // degrees, clockwise as stored
    double scale_x = 1;   // key 32 folded together with 128
    double scale_y = 1;   // key 32 folded together with 129
    bool flip_x = false;
    bool flip_y = false;
    std::vector<int> groups;

    // The untouched "key,value,key,value" text, so nothing is lost and rarely
    // used keys can still be read without storing every key for every object.
    std::string raw;

    // Looks up a key in `raw`. Numeric keys only match numeric keys.
    std::optional<std::string_view> prop(int key) const;
    std::optional<std::string_view> prop(std::string_view key) const;
};

struct Level {
    std::string name;  // only known when the input was a .gmd export
    StartSettings start;
    std::string header;  // raw header segment
    std::vector<LevelObject> objects;
};

struct ParseResult {
    bool ok = false;
    std::string error;
    Level level;
};

// Accepts any of:
//   * a decoded level string ("kS38,...;1,1,2,15,3,15;...")
//   * an encoded level string (base64 of gzip or zlib data, "H4sI..." / "eJ...")
//   * the text of a .gmd export, from which the k4 (level string) and
//     k2 (name) entries are read
ParseResult parse_level(std::string_view text);

// Reads the kA* keys out of a header segment or a start position object.
StartSettings parse_start_settings(std::string_view key_value_text);

}  // namespace gdpf
