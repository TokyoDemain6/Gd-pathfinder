// The physics simulator: a static world built from a level, and a player
// state that World::step() advances one 1/240 s tick at a time.
//
// PlayerState is a plain value with no pointers, so a search can snapshot and
// restore it by copying. The World never changes after it is built.
//
// Anything the simulator does not implement sets PlayerState::fault and stops
// the run. It never guesses its way through an unsupported mechanic, so a run
// that reaches the end without a fault only used implemented mechanics.
#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <vector>

#include "gdpf/level.hpp"
#include "gdpf/objects.hpp"
#include "gdpf/types.hpp"

namespace gdpf {

enum class Fault : std::uint8_t {
    None,
    Slope,           // touched a slope
    RotatedSolid,    // touched a solid rotated off the 90-degree grid
    SpiderJump,      // clicked as the spider (its teleport is not implemented)
    DashOrb,
    SpiderOrb,
    TeleportOrb,
    ToggleOrb,
    SpiderPad,
    TeleportPortal,
    DualPortal,
    UnknownBoost,    // pad or orb whose strength is not known for this gamemode
};

std::string_view describe(Fault fault);

struct PlayerState {
    double x = 0;        // centre of the player
    double y = 0;
    double y_vel = 0;    // game units; see constants.hpp
    double floor_y = 0;  // bounds of the current gamemode's corridor
    double ceil_y = 0;
    double robot_boost = 0;

    std::uint32_t tick = 0;

    // Recently activated pads, orbs and portals (collider index + 1), so each
    // fires once. A small ring is enough because the player only moves forward.
    std::array<std::uint32_t, 16> used{};
    std::uint8_t used_next = 0;

    Gamemode mode = Gamemode::Cube;
    Speed speed = Speed::Normal;
    bool mini = false;
    bool upside_down = false;
    bool on_ground = false;

    bool button = false;       // raw input from the previous step
    bool jump_held = false;    // input as the physics sees it (a ball jump clears it)
    bool fresh_click = false;  // a press not yet used by a jump or an orb

    bool boosted = false;      // rising after a jump, pad or orb
    bool accelerating = false; // flying past the usual velocity limits (never set yet)
    bool touched_pad = false;

    bool dead = false;
    bool finished = false;
    Fault fault = Fault::None;

    bool running() const { return !dead && !finished && fault == Fault::None; }
};
static_assert(std::is_trivially_copyable_v<PlayerState>);

struct Collider {
    ObjKind kind = ObjKind::Unknown;
    std::uint8_t sub = 0;
    bool axis_aligned = true;
    float cx = 0, cy = 0;       // centre
    float hw = 0, hh = 0;       // half extents (hw is the radius for a saw)
    float sin_r = 0, cos_r = 1; // rotation, only used when !axis_aligned
    float min_x = 0, max_x = 0; // horizontal extent, for the broad phase
    std::uint32_t object_index = 0;  // index into Level::objects
};

class World {
public:
    // Builds the collision world. Objects the table does not know, and objects
    // that cannot affect movement, are left out.
    explicit World(const Level& level, const ObjectTable& table = ObjectTable::builtin());

    PlayerState initial_state() const { return initial_; }

    // Advances one tick. `hold` is whether the jump button is down during it.
    // Does nothing once the run has ended (dead, finished or faulted).
    void step(PlayerState& p, bool hold) const;

    double end_x() const { return end_x_; }
    const std::vector<Collider>& colliders() const { return colliders_; }

private:
    void collide(PlayerState& p) const;
    void activate(PlayerState& p, const Collider& c) const;

    std::vector<Collider> colliders_;
    std::vector<std::uint32_t> first_bucket_;          // per collider
    std::vector<std::vector<std::uint32_t>> buckets_;  // collider indices per 100-unit column
    PlayerState initial_;
    double end_x_ = 0;
};

// Counters for how much collision work World::step() does, read by the
// benchmark in bench/. They only exist when the library is compiled with
// GDPF_STATS (the gdpf_stats CMake target), so the normal library carries no
// counting code. One set of counters for the whole process; not thread-safe.
#ifdef GDPF_STATS
struct SimStats {
    std::uint64_t collider_visits = 0;  // colliders the broad phase handed to a pass
    std::uint64_t overlap_tests = 0;    // hitbox tests run on them
};
SimStats& sim_stats();
#endif

// Why the simulator cannot handle this kind of object, or an empty string if
// it can. Used by analyze() to reject levels up front.
std::string_view unsupported_reason(const ObjInfo& info);

// Half the side length of the player's hitbox in its current state.
double player_half_size(const PlayerState& p);

// The game keeps y velocity to three decimals; this is that rounding.
double round_velocity(double v);

}  // namespace gdpf
