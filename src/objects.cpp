#include "gdpf/objects.hpp"

#include <initializer_list>

namespace gdpf {

std::string_view name(ObjKind kind) {
    switch (kind) {
        case ObjKind::Unknown: return "unknown";
        case ObjKind::Solid: return "solid";
        case ObjKind::Hazard: return "hazard";
        case ObjKind::Saw: return "saw";
        case ObjKind::Slope: return "slope";
        case ObjKind::Pad: return "pad";
        case ObjKind::Orb: return "orb";
        case ObjKind::GamemodePortal: return "gamemode portal";
        case ObjKind::GravityPortal: return "gravity portal";
        case ObjKind::SizePortal: return "size portal";
        case ObjKind::SpeedPortal: return "speed portal";
        case ObjKind::DualPortal: return "dual portal";
        case ObjKind::MirrorPortal: return "mirror portal";
        case ObjKind::TeleportPortal: return "teleport portal";
        case ObjKind::StartPos: return "start position";
        case ObjKind::Collectible: return "collectible";
        case ObjKind::VisualTrigger: return "visual trigger";
        case ObjKind::GameplayTrigger: return "gameplay trigger";
    }
    return "?";
}

const ObjInfo* ObjectTable::find(int id) const {
    const auto it = map_.find(id);
    return it == map_.end() ? nullptr : &it->second;
}

void ObjectTable::set(int id, ObjInfo info) {
    map_[id] = info;
}

namespace {

template <typename Sub>
ObjInfo info(ObjKind kind, Sub sub, float w, float h) {
    return ObjInfo{kind, static_cast<std::uint8_t>(sub), w, h};
}

ObjInfo info(ObjKind kind, float w = 0, float h = 0) {
    return ObjInfo{kind, 0, w, h};
}

void add(ObjectTable& t, std::initializer_list<int> ids, ObjInfo i) {
    for (int id : ids) t.set(id, i);
}

// PROVISIONAL. IDs and hitbox sizes here come from memory and from reading
// community references (see docs/physics.md, "Sources"). None of it has been
// checked against the game yet. Two rules keep mistakes safe:
//   * an ID is only listed as a VisualTrigger when there is no doubt about
//     it, because wrongly calling a gameplay trigger "visual" would silently
//     produce a wrong path;
//   * anything not listed stays Unknown and is reported, not guessed.
ObjectTable make_builtin() {
    ObjectTable t;

    // --- solids ---------------------------------------------------------
    add(t, {1, 2, 3, 4, 6, 7}, info(ObjKind::Solid, 30, 30));  // basic blocks
    add(t, {40}, info(ObjKind::Solid, 30, 14));                // half slab

    // --- hazards --------------------------------------------------------
    add(t, {8}, info(ObjKind::Hazard, 6, 12));      // spike
    add(t, {39}, info(ObjKind::Hazard, 6, 5.6f));   // half spike
    add(t, {103}, info(ObjKind::Hazard, 4, 7.6f));  // medium spike
    add(t, {88}, info(ObjKind::Saw, 32.3f, 0));     // large saw
    add(t, {89}, info(ObjKind::Saw, 21.6f, 0));     // medium saw
    add(t, {98}, info(ObjKind::Saw, 12, 0));        // small saw

    // --- slopes ---------------------------------------------------------
    // Listed so that levels using them are rejected by name; the simulator
    // does not implement slopes yet. Sizes are the bounding box.
    add(t,
        {289, 294, 299, 305, 309, 315, 321, 326, 331, 337, 343, 349, 353, 363, 371, 483, 492,
         651, 665, 673, 709, 711, 726, 728, 886, 1338, 1341, 1344, 1717, 1723, 1743, 1745, 1747, 1749, 1906},
        info(ObjKind::Slope, 30, 30));
    add(t,
        {291, 295, 301, 307, 311, 317, 323, 327, 333, 339, 345, 351, 355, 364, 366, 367, 372, 484, 493,
         652, 666, 674, 710, 712, 727, 729, 887, 1339, 1342, 1345, 1718, 1724, 1744, 1746, 1748, 1750, 1907},
        info(ObjKind::Slope, 60, 30));

    // --- pads -----------------------------------------------------------
    add(t, {35}, info(ObjKind::Pad, PadType::Yellow, 25, 4));
    add(t, {140}, info(ObjKind::Pad, PadType::Pink, 25, 5));
    add(t, {1332}, info(ObjKind::Pad, PadType::Red, 25, 5));
    add(t, {67}, info(ObjKind::Pad, PadType::Blue, 25, 6));
    add(t, {3005}, info(ObjKind::Pad, PadType::Spider, 25, 5));

    // --- orbs -----------------------------------------------------------
    add(t, {36}, info(ObjKind::Orb, OrbType::Yellow, 36, 36));
    add(t, {141}, info(ObjKind::Orb, OrbType::Pink, 36, 36));
    add(t, {1333}, info(ObjKind::Orb, OrbType::Red, 36, 36));
    add(t, {84}, info(ObjKind::Orb, OrbType::Blue, 36, 36));
    add(t, {1022}, info(ObjKind::Orb, OrbType::Green, 36, 36));
    add(t, {1330}, info(ObjKind::Orb, OrbType::Black, 36, 36));
    add(t, {1704, 1751}, info(ObjKind::Orb, OrbType::Dash, 36, 36));
    add(t, {3004}, info(ObjKind::Orb, OrbType::Spider, 36, 36));
    add(t, {3027}, info(ObjKind::Orb, OrbType::Teleport, 36, 36));
    add(t, {1594}, info(ObjKind::Orb, OrbType::Toggle, 36, 36));

    // --- portals --------------------------------------------------------
    add(t, {12}, info(ObjKind::GamemodePortal, Gamemode::Cube, 34, 86));
    add(t, {13}, info(ObjKind::GamemodePortal, Gamemode::Ship, 34, 86));
    add(t, {47}, info(ObjKind::GamemodePortal, Gamemode::Ball, 34, 86));
    add(t, {111}, info(ObjKind::GamemodePortal, Gamemode::Ufo, 34, 86));
    add(t, {660}, info(ObjKind::GamemodePortal, Gamemode::Wave, 34, 86));
    add(t, {745}, info(ObjKind::GamemodePortal, Gamemode::Robot, 34, 86));
    add(t, {1331}, info(ObjKind::GamemodePortal, Gamemode::Spider, 34, 86));
    add(t, {1933}, info(ObjKind::GamemodePortal, Gamemode::Swing, 34, 86));

    add(t, {10}, info(ObjKind::GravityPortal, GravityChange::Normal, 25, 75));
    add(t, {11}, info(ObjKind::GravityPortal, GravityChange::Flipped, 25, 75));
    add(t, {2926}, info(ObjKind::GravityPortal, GravityChange::Toggle, 25, 75));

    add(t, {99}, info(ObjKind::SizePortal, 0, 31, 90));
    add(t, {101}, info(ObjKind::SizePortal, 1, 31, 90));

    add(t, {200}, info(ObjKind::SpeedPortal, Speed::Half, 35, 44));
    add(t, {201}, info(ObjKind::SpeedPortal, Speed::Normal, 33, 56));
    add(t, {202}, info(ObjKind::SpeedPortal, Speed::Double, 51, 56));
    add(t, {203}, info(ObjKind::SpeedPortal, Speed::Triple, 65, 56));
    add(t, {1334}, info(ObjKind::SpeedPortal, Speed::Quadruple, 69, 56));

    add(t, {286}, info(ObjKind::DualPortal, 1, 34, 86));
    add(t, {287}, info(ObjKind::DualPortal, 0, 34, 86));
    add(t, {45, 46}, info(ObjKind::MirrorPortal, 34, 86));
    add(t, {747, 749, 2902}, info(ObjKind::TeleportPortal, 34, 86));

    // --- no effect on movement -------------------------------------------
    add(t, {31}, info(ObjKind::StartPos));
    add(t, {142, 1329}, info(ObjKind::Collectible));  // secret coin, user coin

    add(t,
        {
            29,   30,   104,  105,  221,  717,  718,  743,  744,  899,  900,  915,  // colour
            22,   23,   24,   25,   26,   27,   28,   55,   56,   57,   58,   59,   1915,  // enter effects
            32,   33,                                                               // trail on/off
            1006, 1007,                                                             // pulse, alpha
            1520,                                                                   // shake
            1612, 1613,                                                             // hide/show player
            1818, 1819,                                                             // bg effect on/off
            1913, 1914, 1916,                                                       // camera zoom/static/offset
        },
        info(ObjKind::VisualTrigger));

    // --- can change where the player goes --------------------------------
    add(t,
        {
            901,  1346, 2067,                    // move, rotate, scale
            1347, 1814, 3016, 3660, 3661,        // follow, follow player Y, advanced follow (+edits)
            1049, 1268, 1616, 3607,              // toggle, spawn, stop, sequence
            1585, 3032, 3033,                    // animate, keyframe, animate keyframe
            1595, 1611, 1811, 1817, 1815, 3609,  // touch, count, instant count, pickup, collision(s)
            1816, 3640, 3643,                    // collision block, state block, toggle block
            1812, 1912, 2068,                    // on death, random, advanced random
            1917, 2900, 2901,                    // reverse, rotate gameplay, gameplay offset
            1932, 2066, 2899,                    // player control, gravity, options
            1935, 3022, 3600,                    // timewarp, teleport, end
            3604, 3614, 3615, 3617, 3618,        // event, time, time event, time control, reset
            3619, 3620, 3641,                    // item edit, item compare, persistent item
            3006, 3007, 3008, 3011, 3012, 3013, 3024,  // area move/rotate/scale (+edits), area stop
            2069, 3645,                          // force block, force circle
        },
        info(ObjKind::GameplayTrigger));

    return t;
}

}  // namespace

const ObjectTable& ObjectTable::builtin() {
    static const ObjectTable table = make_builtin();
    return table;
}

}  // namespace gdpf
