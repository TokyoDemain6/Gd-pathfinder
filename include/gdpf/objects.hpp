// What each object ID means for gameplay: its kind and its hitbox.
//
// The built-in table is PROVISIONAL. It covers a small set of common objects
// and was written from community references, not measured from the game. The
// authoritative table is meant to be dumped from the running game by the
// trace-logger mod and loaded over the top with ObjectTable::set().
#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_map>

#include "gdpf/types.hpp"

namespace gdpf {

enum class ObjKind : std::uint8_t {
    Unknown,          // not in the table: could be decoration, could be a hazard
    Solid,            // block the player can stand on
    Hazard,           // rectangular kill box
    Saw,              // circular kill box
    Slope,
    Pad,              // sub = PadType
    Orb,              // sub = OrbType
    GamemodePortal,   // sub = Gamemode
    GravityPortal,    // sub = GravityChange
    SizePortal,       // sub = 1 for mini, 0 for normal
    SpeedPortal,      // sub = Speed
    DualPortal,       // sub = 1 to enter dual, 0 to leave
    MirrorPortal,     // visual only in classic levels
    TeleportPortal,
    StartPos,
    Collectible,      // coins and similar: no effect on movement
    VisualTrigger,    // cannot change where the player goes
    GameplayTrigger,  // can change where the player goes: out of scope
};
inline constexpr int kObjKindCount = 18;

std::string_view name(ObjKind kind);

enum class PadType : std::uint8_t { Yellow, Pink, Red, Blue, Spider };
enum class OrbType : std::uint8_t {
    Yellow, Pink, Red, Blue, Green, Black, Dash, Spider, Teleport, Toggle
};
enum class GravityChange : std::uint8_t { Normal, Flipped, Toggle };

struct ObjInfo {
    ObjKind kind = ObjKind::Unknown;
    std::uint8_t sub = 0;
    // Hitbox at scale 1 and rotation 0. For ObjKind::Saw, `width` is the
    // radius and `height` is unused.
    float width = 0;
    float height = 0;
};

class ObjectTable {
public:
    // The provisional table described at the top of this file.
    static const ObjectTable& builtin();

    const ObjInfo* find(int id) const;
    void set(int id, ObjInfo info);
    std::size_t size() const { return map_.size(); }

private:
    std::unordered_map<int, ObjInfo> map_;
};

}  // namespace gdpf
