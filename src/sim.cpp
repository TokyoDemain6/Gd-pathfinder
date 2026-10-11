#include "gdpf/sim.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "gdpf/constants.hpp"

// Status of this file: a SKELETON. The vertical motion in update_velocity()
// follows the structure of the game's own update routine as documented in
// community decompilations, and reproduces the numbers other simulators
// measured. Collision, pads, orbs and portals are simplified. Nothing has
// been compared with the running game yet. See docs/physics.md.

// Counts one unit of collision work in the gdpf_stats build; nothing otherwise.
#ifdef GDPF_STATS
#define GDPF_COUNT(field) (++sim_stats().field)
#else
#define GDPF_COUNT(field) ((void)0)
#endif

namespace gdpf {

using namespace constants;

#ifdef GDPF_STATS
SimStats& sim_stats() {
    static SimStats stats;
    return stats;
}
#endif

namespace {

constexpr double kBucketWidth = 100.0;
constexpr double kUnknown = std::numeric_limits<double>::quiet_NaN();

bool is_flying(Gamemode m) {
    return m == Gamemode::Ship || m == Gamemode::Ufo || m == Gamemode::Wave || m == Gamemode::Swing;
}

int flip_sign(const PlayerState& p) {
    return p.upside_down ? -1 : 1;
}

void set_y_vel(PlayerState& p, double v) {
    p.y_vel = round_velocity(v);
}

void add_y_vel(PlayerState& p, double d) {
    p.y_vel = round_velocity(p.y_vel + d);
}

// The game's own "is the player falling" test. It compares against twice the
// gravity constant instead of zero, and does not mirror the comparison when
// gravity is flipped. That asymmetry is in the game: it is why an upside-down
// cube can still jump for a few ticks after running off a ledge.
bool is_falling(const PlayerState& p) {
    const double threshold = 2.0 * kGravity[static_cast<int>(p.speed)];
    return p.upside_down ? p.y_vel > threshold : p.y_vel < threshold;
}

void flip_gravity(PlayerState& p, bool upside_down) {
    if (p.upside_down == upside_down) return;
    p.upside_down = upside_down;
    p.y_vel *= kGravityFlipKeep;
    p.on_ground = false;
}

void update_flying(PlayerState& p) {
    const double dt = kDtSlow;
    int flip = flip_sign(p);

    double size = 1.0;
    double up_limit = kFlyUpLimit;
    double down_limit = kFlyDownLimit;
    if (p.mini) {
        size = kMiniFlyScale;
        up_limit = kMiniFlyUpLimit;
        down_limit = kMiniFlyDownLimit;
    }

    {
        const double rel = flip * p.y_vel;
        if ((rel >= 0 && rel < up_limit) || (rel <= 0 && rel > down_limit)) p.accelerating = false;
    }

    double lower_scale = 0.8;  // the downward limit is 0.8 of the upward one

    switch (p.mode) {
        case Gamemode::Ship: {
            const bool falling = is_falling(p);
            double pull;
            if (p.jump_held) {
                pull = (p.accelerating && flip * p.y_vel >= 0) ? 0.8 : -1.0;
            } else {
                // The reference decompilation does not show the value used
                // while accelerating and not holding; 0.8 is a placeholder.
                pull = (!p.accelerating && !falling) ? 1.2 : 0.8;
            }
            double strength = falling ? 0.5 : 0.4;
            if (!p.accelerating && !p.jump_held) strength = 0.4;
            add_y_vel(p, -pull * kFlyGravity * dt * flip * strength / size);
            break;
        }
        case Gamemode::Ufo: {
            if (p.fresh_click && p.jump_held) {
                p.fresh_click = false;
                const double jump = flip * (p.mini ? kMiniUfoJump : kUfoJump) * size;
                if (flip * jump > flip * p.y_vel) set_y_vel(p, jump);
            }
            const double pull = is_falling(p) ? 0.8 : 1.2;
            add_y_vel(p, -kFlyGravity * dt * flip * pull * 0.5 / size);
            break;
        }
        case Gamemode::Swing: {
            if (p.fresh_click && p.jump_held) {
                p.fresh_click = false;
                const double before = p.y_vel;
                flip_gravity(p, !p.upside_down);
                set_y_vel(p, before * kSwingFlipKeep);
                flip = flip_sign(p);
            }
            add_y_vel(p, -kFlyGravity * dt * flip * (p.mini ? kMiniSwingGravityScale : kSwingGravityScale));
            lower_scale = 1.0;
            size = 1.0;
            break;
        }
        case Gamemode::Wave: {
            // The wave moves diagonally at exactly its horizontal speed (twice
            // that when mini). Expressed as a y velocity so the position
            // update is the same for every gamemode.
            const double slope = p.mini ? 2.0 : 1.0;
            p.y_vel = flip * (p.jump_held ? 1.0 : -1.0) * slope * x_step(p.speed) / kDtSlow;
            return;
        }
        default: return;
    }

    if (!p.accelerating) {
        const double rel = std::clamp(flip * p.y_vel, lower_scale * -8.0 / size, 8.0 / size);
        set_y_vel(p, flip * rel);
    }
}

void update_grounded(PlayerState& p) {
    const double dt = kDtSlow;
    const int speed = static_cast<int>(p.speed);
    const bool uses_speed_gravity = p.mode == Gamemode::Cube || p.mode == Gamemode::Robot;
    const double gravity = uses_speed_gravity ? kGravity[speed] : kFlyGravity;
    const int flip = flip_sign(p);

    double scale = 1.0;
    if (p.mode == Gamemode::Ball || p.mode == Gamemode::Spider) scale = kBallGravityScale;
    else if (p.mode == Gamemode::Robot) scale = kRobotGravityScale;

    // The cube jumps for as long as the button is held. The robot needs a new
    // press each time.
    const bool wants_jump = p.jump_held && (p.fresh_click || p.mode != Gamemode::Robot);

    if (p.on_ground && wants_jump) {
        if (p.mode == Gamemode::Spider) {
            p.fault = Fault::SpiderJump;
            return;
        }
        p.on_ground = false;
        p.boosted = true;
        p.fresh_click = false;
        p.touched_pad = false;
        p.robot_boost = 0;

        double jump = kJumpVelocity[speed];
        if (p.mode == Gamemode::Robot) jump *= kRobotJumpScale;
        set_y_vel(p, flip * jump * (p.mini ? kMiniScale : 1.0));

        if (p.mode == Gamemode::Ball) {
            // The ball does not jump: it flips gravity and drifts across. The
            // game writes this velocity without rounding it.
            flip_gravity(p, !p.upside_down);
            p.jump_held = false;
            p.y_vel *= kBallJumpScale;
        }
        return;
    }

    if (p.boosted) {
        const double pull = flip * gravity * dt * scale;
        if (p.mode == Gamemode::Robot && p.jump_held && !p.touched_pad && p.robot_boost < kRobotBoostLimit) {
            // Holding cancels gravity for a while, which is the robot's
            // variable jump height.
            p.robot_boost += dt / 10.0;
            add_y_vel(p, pull);
        }
        add_y_vel(p, -pull);
        if (is_falling(p)) p.boosted = false;
        return;
    }

    if (is_falling(p)) p.on_ground = false;
    add_y_vel(p, -gravity * dt * flip * scale);
    if (p.upside_down) set_y_vel(p, std::min(p.y_vel, kTerminalVelocity));
    else set_y_vel(p, std::max(p.y_vel, -kTerminalVelocity));
}

// ------------------------------------------------------------- geometry ---

struct Box {
    double left, right, bottom, top;
};

Box box_at(double x, double y, double half) {
    return {x - half, x + half, y - half, y + half};
}

bool overlaps(const Collider& c, const Box& b) {
    GDPF_COUNT(overlap_tests);
    if (c.kind == ObjKind::Saw) {
        const double nx = std::clamp(static_cast<double>(c.cx), b.left, b.right);
        const double ny = std::clamp(static_cast<double>(c.cy), b.bottom, b.top);
        const double dx = nx - c.cx;
        const double dy = ny - c.cy;
        return dx * dx + dy * dy < static_cast<double>(c.hw) * c.hw;
    }
    if (c.axis_aligned) {
        return b.left < c.cx + c.hw && b.right > c.cx - c.hw && b.bottom < c.cy + c.hh && b.top > c.cy - c.hh;
    }
    // Separating axis test between the rotated box and the player's box.
    const double ex = (b.right - b.left) / 2;
    const double ey = (b.top - b.bottom) / 2;
    const double dx = c.cx - (b.left + b.right) / 2;
    const double dy = c.cy - (b.bottom + b.top) / 2;
    const double cs = std::abs(c.cos_r);
    const double sn = std::abs(c.sin_r);
    if (std::abs(dx) >= ex + c.hw * cs + c.hh * sn) return false;
    if (std::abs(dy) >= ey + c.hw * sn + c.hh * cs) return false;
    if (std::abs(dx * c.cos_r + dy * c.sin_r) >= c.hw + ex * cs + ey * sn) return false;
    if (std::abs(-dx * c.sin_r + dy * c.cos_r) >= c.hh + ex * sn + ey * cs) return false;
    return true;
}

bool is_effect(ObjKind k) {
    switch (k) {
        case ObjKind::Pad:
        case ObjKind::Orb:
        case ObjKind::GamemodePortal:
        case ObjKind::GravityPortal:
        case ObjKind::SizePortal:
        case ObjKind::SpeedPortal:
        case ObjKind::DualPortal:
        case ObjKind::TeleportPortal:
            return true;
        default:
            return false;
    }
}

bool was_used(const PlayerState& p, std::uint32_t index) {
    for (std::uint32_t u : p.used) {
        if (u == index + 1) return true;
    }
    return false;
}

void mark_used(PlayerState& p, std::uint32_t index) {
    p.used[p.used_next] = index + 1;
    p.used_next = static_cast<std::uint8_t>((p.used_next + 1) % p.used.size());
}

void land(PlayerState& p, double y) {
    p.y = y;
    p.y_vel = 0;
    p.on_ground = true;
    p.boosted = false;
}

void collide_solid(PlayerState& p, const Collider& c) {
    const double half = player_half_size(p);
    if (!overlaps(c, box_at(p.x, p.y, half))) return;
    if (!c.axis_aligned) {
        p.fault = Fault::RotatedSolid;
        return;
    }
    if (overlaps(c, box_at(p.x, p.y, kInnerSize / 2))) {
        p.dead = true;  // ran into it
        return;
    }

    // Work in gravity-relative coordinates, where "down" is always the way
    // the player falls.
    const int flip = flip_sign(p);
    const double rel_vel = flip * p.y_vel;
    const double p_bottom = flip * p.y - half;
    const double c_top = flip * static_cast<double>(c.cy) + c.hh;
    const double c_bottom = flip * static_cast<double>(c.cy) - c.hh;
    const bool small_clip = p.mode == Gamemode::Ship || p.mode == Gamemode::Ufo;
    const double clip = small_clip ? kLandClipFlying : kLandClip;

    if (p.mode != Gamemode::Wave && c_top - p_bottom <= clip && rel_vel <= 0) {
        land(p, flip * (c_top + half));
        return;
    }

    // Gamemodes that can slide along the underside of a block.
    const bool can_bump = p.mode == Gamemode::Ship || p.mode == Gamemode::Ufo || p.mode == Gamemode::Swing ||
                          p.mode == Gamemode::Ball || p.mode == Gamemode::Spider;
    if (can_bump) {
        const double p_top = p_bottom + 2 * half;
        if (p_top - c_bottom <= clip - 1 && rel_vel > 0) {
            p.y = flip * (c_bottom - half);
            p.y_vel = 0;
        }
    }
}

void apply_bounds(PlayerState& p) {
    const double half = player_half_size(p);
    const int flip = flip_sign(p);
    const double rel_y = flip * p.y;
    const double rel_vel = flip * p.y_vel;
    const double rel_floor = p.upside_down ? -p.ceil_y : p.floor_y;
    const double rel_ceil = p.upside_down ? -p.floor_y : p.ceil_y;

    if (rel_y - half <= rel_floor) {
        if (p.mode == Gamemode::Wave) p.y = flip * (rel_floor + half);
        else if (rel_vel <= 0) land(p, flip * (rel_floor + half));
    } else if (rel_y + half >= rel_ceil) {
        if (p.mode == Gamemode::Cube || p.mode == Gamemode::Robot) {
            p.dead = true;  // an upside-down cube hitting the ground head first
        } else {
            p.y = flip * (rel_ceil - half);
            if (rel_vel > 0 && p.mode != Gamemode::Wave) p.y_vel = 0;
        }
    }
    if (p.y > kMaxY) p.dead = true;
}

void enter_gamemode(PlayerState& p, Gamemode mode, double portal_y) {
    if (mode != p.mode) {
        p.mode = mode;
        p.y_vel *= 0.5;  // simplified: the real factor depends on both gamemodes
        p.robot_boost = 0;
        p.boosted = false;
        p.accelerating = false;
    }
    const double height = kModeHeight[static_cast<int>(mode)];
    if (std::isinf(height)) {
        p.floor_y = 0;
        p.ceil_y = kNoCeiling;
    } else {
        p.floor_y = std::max(0.0, 30.0 * std::ceil((portal_y - (height / 2 + 30.0)) / 30.0));
        p.ceil_y = p.floor_y + height;
    }
}

// Strength of each pad as a fraction of kPadVelocity, per gamemode. Negative
// means "after flipping gravity, moving toward the new floor". kUnknown
// makes the run stop with Fault::UnknownBoost.
constexpr double kPadFactor[4][kGamemodeCount] = {
    //          cube   ship   ball   ufo    wave  robot     spider    swing
    /*yellow*/ {1.00, 1.00, 0.60, 0.50, 0.0, kUnknown, kUnknown, kUnknown},
    /*pink  */ {0.65, 0.35, 0.42, 0.40, 0.0, kUnknown, kUnknown, kUnknown},
    /*red   */ {1.25, 0.63, 0.75, 0.60, 0.0, kUnknown, kUnknown, kUnknown},
    /*blue  */ {-0.40, -0.40, -0.24, -0.40, 0.0, kUnknown, kUnknown, kUnknown},
};

// Strength of each orb as a fraction of the cube's jump velocity.
constexpr double kOrbFactor[5][kGamemodeCount] = {
    //          cube   ship      ball      ufo       wave  robot     spider    swing
    /*yellow*/ {1.00, 1.00, 0.70, 1.00, 0.0, kUnknown, kUnknown, kUnknown},
    /*pink  */ {0.72, 0.37, 0.539, 0.42, 0.0, kUnknown, kUnknown, kUnknown},
    /*red   */ {1.38, 1.00, kUnknown, 1.02, 0.0, kUnknown, kUnknown, kUnknown},
    /*blue  */ {-0.40, -0.40, -0.28, -0.40, 0.0, kUnknown, kUnknown, kUnknown},
    /*green */ {1.00, kUnknown, 0.70, kUnknown, 0.0, kUnknown, kUnknown, kUnknown},
};

// Applies a pad or orb boost. `base` is the full-strength velocity and
// `factor` the fraction of it for this gamemode.
void boost(PlayerState& p, double base, double factor, bool flips_gravity) {
    if (flips_gravity) flip_gravity(p, !p.upside_down);
    if (p.mode != Gamemode::Wave) {
        if (std::isnan(factor)) {
            p.fault = Fault::UnknownBoost;
            return;
        }
        set_y_vel(p, flip_sign(p) * factor * base * (p.mini ? kMiniScale : 1.0));
    }
    p.boosted = true;
    p.on_ground = false;
}

}  // namespace

// ----------------------------------------------------------------- World ---

double round_velocity(double v) {
    const double whole = static_cast<double>(static_cast<long long>(v));
    if (v != whole) v = std::round((v - whole) * 1000.0) / 1000.0 + whole;
    return v;
}

double player_half_size(const PlayerState& p) {
    const double size = p.mode == Gamemode::Wave ? kWaveSize : kPlayerSize;
    return size * (p.mini ? kMiniSizeScale : 1.0) / 2;
}

std::string_view describe(Fault fault) {
    switch (fault) {
        case Fault::None: return "none";
        case Fault::Slope: return "touched a slope (slopes are not implemented)";
        case Fault::RotatedSolid: return "touched a solid rotated off the 90-degree grid";
        case Fault::SpiderJump: return "spider click (the spider teleport is not implemented)";
        case Fault::DashOrb: return "used a dash orb (not implemented)";
        case Fault::SpiderOrb: return "used a spider orb (not implemented)";
        case Fault::TeleportOrb: return "used a teleport orb (not implemented)";
        case Fault::ToggleOrb: return "used a toggle orb (out of scope)";
        case Fault::SpiderPad: return "touched a spider pad (not implemented)";
        case Fault::TeleportPortal: return "entered a teleport portal (not implemented)";
        case Fault::DualPortal: return "entered a dual portal (dual mode is not implemented)";
        case Fault::UnknownBoost: return "pad or orb whose strength is unknown for this gamemode";
    }
    return "?";
}

std::string_view unsupported_reason(const ObjInfo& info) {
    switch (info.kind) {
        case ObjKind::Slope: return "slopes";
        case ObjKind::TeleportPortal: return "teleport portals";
        case ObjKind::GameplayTrigger: return "gameplay triggers";
        case ObjKind::DualPortal: return info.sub != 0 ? "dual mode" : "";
        case ObjKind::GamemodePortal:
            return static_cast<Gamemode>(info.sub) == Gamemode::Spider ? "spider teleport" : "";
        case ObjKind::Pad: return static_cast<PadType>(info.sub) == PadType::Spider ? "spider pads" : "";
        case ObjKind::Orb:
            switch (static_cast<OrbType>(info.sub)) {
                case OrbType::Dash: return "dash orbs";
                case OrbType::Spider: return "spider orbs";
                case OrbType::Teleport: return "teleport orbs";
                case OrbType::Toggle: return "toggle orbs";
                default: return "";
            }
        default: return "";
    }
}

World::World(const Level& level, const ObjectTable& table) {
    double max_x = 0;
    for (std::size_t i = 0; i < level.objects.size(); ++i) {
        const LevelObject& obj = level.objects[i];
        max_x = std::max(max_x, obj.x);

        const ObjInfo* info = table.find(obj.id);
        if (info == nullptr) continue;
        switch (info->kind) {
            case ObjKind::Unknown:
            case ObjKind::StartPos:
            case ObjKind::Collectible:
            case ObjKind::MirrorPortal:
            case ObjKind::VisualTrigger:
            case ObjKind::GameplayTrigger:
                continue;
            default:
                break;
        }

        Collider c;
        c.kind = info->kind;
        c.sub = info->sub;
        c.cx = static_cast<float>(obj.x);
        c.cy = static_cast<float>(obj.y);
        c.object_index = static_cast<std::uint32_t>(i);

        float extent_x;
        if (info->kind == ObjKind::Saw) {
            c.hw = c.hh = info->width * static_cast<float>(std::abs(obj.scale_x));
            extent_x = c.hw;
        } else {
            float w = info->width * static_cast<float>(std::abs(obj.scale_x));
            float h = info->height * static_cast<float>(std::abs(obj.scale_y));
            double turns = std::fmod(obj.rotation, 180.0) / 90.0;  // in (-2, 2)
            if (turns < 0) turns += 2;
            if (std::abs(turns - 1.0) < 1e-6) {
                std::swap(w, h);
            } else if (turns > 1e-6 && std::abs(turns - 2.0) > 1e-6) {
                // The level stores rotation clockwise.
                const double radians = -obj.rotation * 3.14159265358979323846 / 180.0;
                c.axis_aligned = false;
                c.sin_r = static_cast<float>(std::sin(radians));
                c.cos_r = static_cast<float>(std::cos(radians));
            }
            c.hw = w / 2;
            c.hh = h / 2;
            extent_x = c.axis_aligned ? c.hw : c.hw * std::abs(c.cos_r) + c.hh * std::abs(c.sin_r);
        }
        c.min_x = c.cx - extent_x;
        c.max_x = c.cx + extent_x;
        colliders_.push_back(c);
    }

    end_x_ = max_x + kLevelEndPadding;

    const auto bucket_count = static_cast<std::size_t>(end_x_ / kBucketWidth) + 2;
    buckets_.resize(bucket_count);
    first_bucket_.resize(colliders_.size());
    const auto bucket_of = [&](double x) {
        const double b = std::floor(x / kBucketWidth);
        return static_cast<std::uint32_t>(std::clamp(b, 0.0, static_cast<double>(bucket_count - 1)));
    };
    for (std::uint32_t i = 0; i < colliders_.size(); ++i) {
        const std::uint32_t first = bucket_of(colliders_[i].min_x);
        const std::uint32_t last = bucket_of(colliders_[i].max_x);
        first_bucket_[i] = first;
        for (std::uint32_t b = first; b <= last; ++b) buckets_[b].push_back(i);
    }

    PlayerState s;
    s.mode = level.start.gamemode;
    s.speed = level.start.speed;
    s.mini = level.start.mini;
    s.upside_down = level.start.flip_gravity;
    s.floor_y = 0;
    s.ceil_y = kModeHeight[static_cast<int>(s.mode)];
    s.x = 0;
    const double half = player_half_size(s);
    s.y = (s.upside_down && std::isfinite(s.ceil_y)) ? s.ceil_y - half : s.floor_y + half;
    s.on_ground = true;
    initial_ = s;
}

void World::activate(PlayerState& p, const Collider& c) const {
    switch (c.kind) {
        case ObjKind::GamemodePortal:
            enter_gamemode(p, static_cast<Gamemode>(c.sub), c.cy);
            break;
        case ObjKind::GravityPortal:
            switch (static_cast<GravityChange>(c.sub)) {
                case GravityChange::Normal: flip_gravity(p, false); break;
                case GravityChange::Flipped: flip_gravity(p, true); break;
                case GravityChange::Toggle: flip_gravity(p, !p.upside_down); break;
            }
            break;
        case ObjKind::SizePortal:
            p.mini = c.sub != 0;
            break;
        case ObjKind::SpeedPortal:
            p.speed = static_cast<Speed>(c.sub);
            break;
        case ObjKind::DualPortal:
            if (c.sub != 0) p.fault = Fault::DualPortal;
            break;
        case ObjKind::TeleportPortal:
            p.fault = Fault::TeleportPortal;
            break;
        case ObjKind::Pad: {
            const auto type = static_cast<PadType>(c.sub);
            if (type == PadType::Spider) {
                p.fault = Fault::SpiderPad;
                break;
            }
            const double factor = kPadFactor[c.sub][static_cast<int>(p.mode)];
            boost(p, kPadVelocity, factor, type == PadType::Blue);
            p.touched_pad = true;
            break;
        }
        case ObjKind::Orb: {
            const auto type = static_cast<OrbType>(c.sub);
            p.fresh_click = false;
            if (p.mode == Gamemode::Ball) p.jump_held = false;
            switch (type) {
                case OrbType::Dash: p.fault = Fault::DashOrb; break;
                case OrbType::Spider: p.fault = Fault::SpiderOrb; break;
                case OrbType::Teleport: p.fault = Fault::TeleportOrb; break;
                case OrbType::Toggle: p.fault = Fault::ToggleOrb; break;
                case OrbType::Black:
                    if (p.mode != Gamemode::Wave) set_y_vel(p, -flip_sign(p) * kTerminalVelocity);
                    p.on_ground = false;
                    break;
                default: {
                    const double factor = kOrbFactor[c.sub][static_cast<int>(p.mode)];
                    const bool flips = type == OrbType::Blue || type == OrbType::Green;
                    boost(p, kJumpVelocity[static_cast<int>(p.speed)], factor, flips);
                    break;
                }
            }
            break;
        }
        default:
            break;
    }
}

void World::collide(PlayerState& p) const {
    const auto bucket_of = [&](double x) {
        const double b = std::floor(x / kBucketWidth);
        return static_cast<std::uint32_t>(std::clamp(b, 0.0, static_cast<double>(buckets_.size() - 1)));
    };
    // The widest the player ever is, so one bucket range serves all passes.
    const std::uint32_t b0 = bucket_of(p.x - kPlayerSize / 2);
    const std::uint32_t b1 = bucket_of(p.x + kPlayerSize / 2);

    // Calls fn(index, collider) once per nearby collider, in index order
    // within each bucket, until the run ends.
    const auto for_each_near = [&](auto&& fn) {
        for (std::uint32_t b = b0; b <= b1; ++b) {
            for (std::uint32_t index : buckets_[b]) {
                // A wide collider sits in several buckets; visit it once.
                if (std::max(first_bucket_[index], b0) != b) continue;
                GDPF_COUNT(collider_visits);
                fn(index, colliders_[index]);
                if (!p.running()) return;
            }
        }
    };

    // 1. Pads, orbs and portals.
    for_each_near([&](std::uint32_t index, const Collider& c) {
        if (!is_effect(c.kind)) return;
        if (!overlaps(c, box_at(p.x, p.y, player_half_size(p)))) return;
        if (was_used(p, index)) return;
        if (c.kind == ObjKind::Orb && !(p.fresh_click && p.jump_held)) return;
        mark_used(p, index);
        activate(p, c);
    });
    if (!p.running()) return;

    // 2. Solids and slopes.
    for_each_near([&](std::uint32_t, const Collider& c) {
        if (c.kind == ObjKind::Solid) {
            collide_solid(p, c);
        } else if (c.kind == ObjKind::Slope) {
            if (overlaps(c, box_at(p.x, p.y, player_half_size(p)))) p.fault = Fault::Slope;
        }
    });
    if (!p.running()) return;

    // 3. Hazards.
    for_each_near([&](std::uint32_t, const Collider& c) {
        if (c.kind != ObjKind::Hazard && c.kind != ObjKind::Saw) return;
        if (overlaps(c, box_at(p.x, p.y, player_half_size(p)))) p.dead = true;
    });
    if (!p.running()) return;

    apply_bounds(p);
}

void World::step(PlayerState& p, bool hold) const {
    if (!p.running()) return;
    ++p.tick;

    if (hold != p.button) {
        p.button = hold;
        p.jump_held = hold;
        p.fresh_click = hold;
    }

    if (is_flying(p.mode)) update_flying(p);
    else update_grounded(p);
    if (!p.running()) return;

    p.x += x_step(p.speed);
    p.y += p.y_vel * kDtSlow;
    if (is_flying(p.mode)) p.on_ground = false;

    collide(p);
    if (p.running() && p.x >= end_x_) p.finished = true;
}

}  // namespace gdpf
