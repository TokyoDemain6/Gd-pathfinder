// Tests for the simulator skeleton.
//
// These check that the simulator is internally consistent and that it
// reproduces a handful of reference numbers (see docs/physics.md). They do
// NOT show that it matches the game: that needs traces from the game itself.

#include <algorithm>
#include <functional>
#include <string>

#include "check.hpp"
#include "gdpf/constants.hpp"
#include "gdpf/sim.hpp"

using namespace gdpf;
using namespace gdpf::constants;

namespace {

World make_world(const std::string& level_text) {
    ParseResult r = parse_level(level_text);
    CHECK(r.ok);
    return World(r.level);
}

using Input = std::function<bool(std::uint32_t tick)>;  // tick is the step about to run (1-based)

const Input kNever = [](std::uint32_t) { return false; };
const Input kAlways = [](std::uint32_t) { return true; };

Input held_during(std::uint32_t first, std::uint32_t last) {
    return [=](std::uint32_t t) { return t >= first && t <= last; };
}

// Runs until the run ends or `ticks` steps have been taken.
PlayerState run(const World& w, const Input& input, std::uint32_t ticks) {
    PlayerState p = w.initial_state();
    while (p.running() && p.tick < ticks) w.step(p, input(p.tick + 1));
    return p;
}

// A far-away block so the level does not end during a test.
const std::string kFar = "1,1,2,90000,3,15;";

}  // namespace

int main() {
    // --- constants ----------------------------------------------------------
    // Horizontal speeds in units per second, as measured by other simulators.
    CHECK_NEAR(x_step(Speed::Half) * 240, 251.16007972, 1e-6);
    CHECK_NEAR(x_step(Speed::Normal) * 240, 311.58009371, 1e-6);
    CHECK_NEAR(x_step(Speed::Double) * 240, 387.42014040, 1e-6);
    CHECK_NEAR(x_step(Speed::Triple) * 240, 468.00013883, 1e-6);
    CHECK_NEAR(x_step(Speed::Quadruple) * 240, 576.00020058, 1e-6);

    CHECK_NEAR(round_velocity(6.870643), 6.871, 1e-12);
    CHECK_NEAR(round_velocity(11.180032), 11.18, 1e-12);
    CHECK_NEAR(round_velocity(-3.3544), -3.354, 1e-12);
    CHECK_NEAR(round_velocity(5.0), 5.0, 0);
    CHECK_NEAR(round_velocity(-15.0), -15.0, 0);

    // --- cube ---------------------------------------------------------------
    {
        const World w = make_world("kA2,0;" + kFar);
        const PlayerState start = w.initial_state();
        CHECK_NEAR(start.y, 15, 0);
        CHECK(start.mode == Gamemode::Cube);

        // Standing still: stays on the ground, moves at 1x speed.
        const PlayerState rest = run(w, kNever, 240);
        CHECK(rest.running());
        CHECK(rest.on_ground);
        CHECK_NEAR(rest.y, 15, 1e-9);
        CHECK_NEAR(rest.y_vel, 0, 0);
        CHECK_NEAR(rest.x, 311.58009371, 1e-5);

        // One jump. Reference: launch at 11.18, losing 0.216 per tick.
        PlayerState p = w.initial_state();
        w.step(p, true);
        CHECK_NEAR(p.y_vel, 11.18, 1e-12);
        CHECK(!p.on_ground);
        w.step(p, false);
        CHECK_NEAR(p.y_vel, 10.964, 1e-12);
        double apex = p.y;
        while (!p.on_ground && p.tick < 400) {
            w.step(p, false);
            apex = std::max(apex, p.y);
        }
        CHECK(p.on_ground);
        CHECK_NEAR(p.y, 15, 1e-9);
        CHECK_NEAR(apex - 15, 66.36, 0.5);           // about 2.2 blocks high
        CHECK(p.tick >= 100 && p.tick <= 108);       // about 0.43 s in the air
        CHECK_NEAR(p.x / 30.0, 4.5, 0.2);            // about 4.5 blocks long

        // Holding keeps jumping; a release does not.
        const PlayerState hopping = run(w, kAlways, 400);
        CHECK(hopping.running());
        CHECK(hopping.tick == 400);

        // Falling is capped at the terminal velocity.
        PlayerState falling = w.initial_state();
        falling.y = 1000;
        falling.on_ground = false;
        double fastest = 0;
        while (!falling.on_ground && falling.tick < 2000) {
            w.step(falling, false);
            fastest = std::min(fastest, falling.y_vel);
        }
        CHECK_NEAR(fastest, -15.0, 1e-12);
        CHECK(falling.on_ground);

        // Same inputs, same result, and copying a state forks the run.
        const Input pattern = [](std::uint32_t t) { return (t / 37) % 3 == 0; };
        const PlayerState a = run(w, pattern, 1500);
        const PlayerState b = run(w, pattern, 1500);
        CHECK(a.x == b.x && a.y == b.y && a.y_vel == b.y_vel && a.tick == b.tick);
        PlayerState fork = run(w, pattern, 700);
        const PlayerState saved = fork;
        while (fork.running() && fork.tick < 1500) w.step(fork, pattern(fork.tick + 1));
        PlayerState again = saved;
        while (again.running() && again.tick < 1500) w.step(again, pattern(again.tick + 1));
        CHECK(fork.x == again.x && fork.y == again.y && fork.y_vel == again.y_vel);
        CHECK(fork.y == a.y && fork.y_vel == a.y_vel);
    }

    // Mini cube: smaller hitbox, weaker jump.
    {
        const World w = make_world("kA2,0,kA3,1;" + kFar);
        PlayerState p = w.initial_state();
        CHECK_NEAR(p.y, 9, 0);
        w.step(p, true);
        CHECK_NEAR(p.y_vel, 8.944, 1e-12);  // 11.180032 * 0.8, rounded
    }

    // Each speed has its own jump strength and gravity.
    {
        const double jump[] = {10.62, 11.18, 11.42, 11.23, 11.23};
        const double drop[] = {0.212, 0.216, 0.215, 0.216, 0.216};
        const int setting[] = {1, 0, 2, 3, 4};  // kA4 values, slowest first
        for (int i = 0; i < 5; ++i) {
            const World w = make_world("kA2,0,kA4," + std::to_string(setting[i]) + ";" + kFar);
            PlayerState p = w.initial_state();
            w.step(p, true);
            CHECK_NEAR(p.y_vel, jump[i], 1e-12);
            w.step(p, true);
            CHECK_NEAR(p.y_vel, jump[i] - drop[i], 1e-12);
        }
    }

    // --- ship ---------------------------------------------------------------
    {
        const World w = make_world("kA2,1;" + kFar);
        PlayerState p = w.initial_state();
        CHECK_NEAR(p.ceil_y, 300, 0);

        // Reference thrust: +0.108 per tick while slow, +0.086 once rising fast.
        w.step(p, true);
        CHECK_NEAR(p.y_vel, 0.108, 1e-12);
        while (p.y_vel < 3.0 && p.tick < 200) w.step(p, true);
        const double before = p.y_vel;
        w.step(p, true);
        CHECK_NEAR(p.y_vel - before, 0.086, 1e-9);

        // Holding forever: capped at 8, stopped by the ceiling, alive.
        double fastest = 0;
        while (p.tick < 1200) {
            w.step(p, true);
            fastest = std::max(fastest, p.y_vel);
            CHECK(p.y <= 285 + 1e-9);
        }
        CHECK(fastest <= 8.0 + 1e-12);
        CHECK(p.running());
        CHECK_NEAR(p.y, 285, 1e-9);

        // Letting go: falls at no more than 6.4 and comes to rest on the floor.
        double slowest = 0;
        while (p.tick < 2400) {
            w.step(p, false);
            slowest = std::min(slowest, p.y_vel);
        }
        CHECK_NEAR(slowest, -6.4, 1e-12);
        CHECK_NEAR(p.y, 15, 1e-9);
        CHECK(p.running());
    }

    // --- ufo ----------------------------------------------------------------
    {
        // Reference: a click leaves the ufo at 6.871 after that tick.
        const World w = make_world("kA2,3;" + kFar);
        PlayerState p = w.initial_state();
        w.step(p, true);
        CHECK_NEAR(p.y_vel, 6.871, 1e-12);
        // Holding is one click, not many.
        w.step(p, true);
        CHECK(p.y_vel < 6.871);
        const PlayerState held = run(w, kAlways, 600);
        CHECK_NEAR(held.y, 15, 1e-9);

        const World mini = make_world("kA2,3,kA3,1;" + kFar);
        PlayerState m = mini.initial_state();
        mini.step(m, true);
        CHECK_NEAR(m.y_vel, 6.648, 1e-12);  // reference value for the mini ufo
    }

    // --- ball ---------------------------------------------------------------
    {
        const World w = make_world("kA2,2;" + kFar);
        PlayerState p = w.initial_state();
        CHECK_NEAR(p.ceil_y, 240, 0);
        w.step(p, true);
        CHECK(p.upside_down);
        CHECK_NEAR(p.y_vel, 3.354, 1e-6);  // reference: 181.116 units/s
        // Keeps holding: must not flip again until the button is pressed again.
        while (!p.on_ground && p.tick < 600) w.step(p, true);
        CHECK(p.on_ground);
        CHECK(p.upside_down);
        CHECK_NEAR(p.y, 225, 1e-9);  // resting on the ceiling
        for (int i = 0; i < 50; ++i) w.step(p, true);
        CHECK(p.upside_down);
        w.step(p, false);
        w.step(p, true);
        CHECK(!p.upside_down);
    }

    // --- wave ---------------------------------------------------------------
    {
        const World w = make_world("kA2,4;" + kFar);
        PlayerState p = w.initial_state();
        CHECK_NEAR(p.y, 5, 0);
        for (int i = 0; i < 40; ++i) w.step(p, true);
        CHECK_NEAR(p.y - 5, p.x, 1e-9);  // 45 degrees
        const double top = p.y;
        for (int i = 0; i < 20; ++i) w.step(p, false);
        CHECK_NEAR(top - p.y, 20 * x_step(Speed::Normal), 1e-9);
        // Slides along the floor and the ceiling without dying.
        CHECK(run(w, kNever, 600).running());
        const PlayerState up = run(w, kAlways, 600);
        CHECK(up.running());
        CHECK_NEAR(up.y, 295, 1e-9);

        const World mini = make_world("kA2,4,kA3,1;" + kFar);
        PlayerState m = mini.initial_state();
        for (int i = 0; i < 20; ++i) mini.step(m, true);
        CHECK_NEAR(m.y - 3, 2 * m.x, 1e-9);  // the mini wave is twice as steep
    }

    // --- robot --------------------------------------------------------------
    {
        const World w = make_world("kA2,5;" + kFar);
        const auto apex_of = [&](const Input& input) {
            PlayerState p = w.initial_state();
            double apex = p.y;
            do {
                w.step(p, input(p.tick + 1));
                apex = std::max(apex, p.y);
            } while (!p.on_ground && p.tick < 600);
            return apex;
        };
        const double tap = apex_of(held_during(1, 1));
        const double hold = apex_of(kAlways);
        CHECK(tap > 16);
        CHECK(hold > tap + 30);  // holding gives a much higher jump

        // Unlike the cube, holding does not jump again on landing.
        const PlayerState p = run(w, kAlways, 600);
        CHECK(p.on_ground);
    }

    // --- swing --------------------------------------------------------------
    {
        const World w = make_world("kA2,7;" + kFar);
        PlayerState p = w.initial_state();
        w.step(p, true);
        CHECK(p.upside_down);
        for (int i = 0; i < 30; ++i) w.step(p, true);
        CHECK(p.upside_down);  // holding is one click
        CHECK(p.y > 15);
        w.step(p, false);
        w.step(p, true);
        CHECK(!p.upside_down);
        const PlayerState idle = run(w, kNever, 2000);
        CHECK(idle.running());
    }

    // --- spider: the click is not implemented, and says so --------------------
    {
        const World w = make_world("kA2,6;" + kFar);
        CHECK(run(w, kNever, 300).running());
        const PlayerState p = run(w, held_during(50, 50), 300);
        CHECK(p.fault == Fault::SpiderJump);
        CHECK(p.tick == 50);
    }

    // --- hazards and solids ---------------------------------------------------
    {
        // A spike on the ground. Its hitbox is 6 wide, so the cube's leading
        // edge reaches it at x = 300 - 3 - 15.
        const World w = make_world("kA2,0;1,8,2,300,3,15;");
        const PlayerState crash = run(w, kNever, 2000);
        CHECK(crash.dead);
        CHECK_NEAR(crash.x, 282, 1.5);

        // Jumping over it and running to the end of the level.
        const PlayerState clear = run(w, held_during(179, 179), 2000);
        CHECK(clear.finished);
        CHECK(clear.x >= w.end_x());
        CHECK_NEAR(w.end_x(), 640, 0);
    }
    {
        // A block: running into its side kills, landing on top does not.
        const World w = make_world("kA2,0;1,1,2,300,3,15;");
        const PlayerState crash = run(w, kNever, 2000);
        CHECK(crash.dead);
        CHECK_NEAR(crash.x, 300 - 15 - 4.5, 1.5);

        PlayerState p = w.initial_state();
        bool stood_on_block = false;
        while (p.running() && p.tick < 2000) {
            w.step(p, p.tick + 1 == 150);
            if (p.on_ground && std::abs(p.y - 45) < 1e-9) stood_on_block = true;
        }
        CHECK(stood_on_block);
        CHECK(p.finished);
    }
    {
        // A low step (within the 10-unit clip) is walked up, not fatal: a slab
        // sunk into the ground so its top is 7 units up.
        const World step_up = make_world("kA2,0;1,40,2,300,3,0;");
        PlayerState p = step_up.initial_state();
        bool on_slab = false;
        while (p.running() && p.tick < 2000) {
            step_up.step(p, false);
            if (p.on_ground && std::abs(p.y - 22) < 1e-9) on_slab = true;
        }
        CHECK(on_slab);
        CHECK(p.finished);
    }
    {
        // A saw is a circle: a cube passing just outside its radius survives.
        const World hit = make_world("kA2,0;1,98,2,300,3,40;");   // radius 12, bottom at y = 28
        CHECK(run(hit, kNever, 2000).dead);
        const World miss = make_world("kA2,0;1,98,2,300,3,43;");  // bottom at y = 31, above the cube
        CHECK(run(miss, kNever, 2000).finished);
    }
    {
        // A spike rotated 45 degrees still kills.
        const World w = make_world("kA2,0;1,8,2,300,3,15,6,45;");
        CHECK(run(w, kNever, 2000).dead);
    }
    {
        // Ship under a row of blocks (underside at y = 90): holding presses it
        // against them and it slides along without dying.
        std::string text = "kA2,1;";
        for (int x = 240; x <= 600; x += 30) text += "1,1,2," + std::to_string(x) + ",3,105;";
        const World w = make_world(text + kFar);
        PlayerState p = w.initial_state();
        while (p.running() && p.x < 560) w.step(p, p.x > 225);
        CHECK(p.running());
        CHECK_NEAR(p.y, 75, 1e-9);
        CHECK_NEAR(p.y_vel, 0, 0);
    }

    // --- portals, pads and orbs -----------------------------------------------
    {
        const World w = make_world("kA2,0;1,13,2,150,3,45;" + kFar);  // ship portal
        const PlayerState p = run(w, kNever, 200);
        CHECK(p.mode == Gamemode::Ship);
        CHECK_NEAR(p.floor_y, 0, 0);
        CHECK_NEAR(p.ceil_y, 300, 0);
    }
    {
        // A ship portal placed high puts the corridor around it.
        const World w = make_world("kA2,1;1,13,2,150,3,15;1,13,2,600,3,285;" + kFar);
        PlayerState p = w.initial_state();
        while (p.running() && p.x < 700) w.step(p, p.y < 285);
        CHECK(p.running());
        CHECK_NEAR(p.floor_y, 120, 0);
        CHECK_NEAR(p.ceil_y, 420, 0);
    }
    {
        const World w = make_world("kA2,0;1,1334,2,150,3,15;" + kFar);  // 4x speed portal
        const PlayerState p = run(w, kNever, 200);
        CHECK(p.speed == Speed::Quadruple);
    }
    {
        const World w = make_world("kA2,0;1,101,2,150,3,15;1,99,2,400,3,15;" + kFar);  // mini, then normal
        const PlayerState small = run(w, kNever, 200);
        CHECK(small.mini);
        CHECK_NEAR(small.y, 9, 1e-9);
        const PlayerState big = run(w, kNever, 500);
        CHECK(!big.mini);
        CHECK_NEAR(big.y, 15, 1e-9);
        CHECK(big.running());
    }
    {
        // Gravity portal in the ball: flips once, the ball lands on the ceiling.
        const World w = make_world("kA2,2;1,11,2,150,3,15;" + kFar);
        const PlayerState p = run(w, kNever, 600);
        CHECK(p.upside_down);
        CHECK(p.on_ground);
        CHECK_NEAR(p.y, 225, 1e-9);
    }
    {
        // Yellow pad: launches at 16 without any input.
        const World w = make_world("kA2,0;1,35,2,150,3,2;" + kFar);
        PlayerState p = w.initial_state();
        double fastest = 0;
        double apex = 0;
        while (p.running() && p.tick < 600) {
            w.step(p, false);
            fastest = std::max(fastest, p.y_vel);
            apex = std::max(apex, p.y);
        }
        CHECK_NEAR(fastest, 16.0, 1e-12);
        CHECK(apex > 120);
        CHECK(p.on_ground);
    }
    {
        // Blue pad in the ball: flips gravity.
        const World w = make_world("kA2,2;1,67,2,150,3,3;" + kFar);
        const PlayerState p = run(w, kNever, 600);
        CHECK(p.upside_down);
        CHECK_NEAR(p.y, 225, 1e-9);
    }
    {
        // Yellow orb in mid-air: needs a fresh press while touching it.
        const std::string text = "kA2,0;1,36,2,300,3,60;" + kFar;
        const World w = make_world(text);
        // Jump so the cube passes through the orb, with no second press.
        const auto apex_with = [&](const Input& input) {
            PlayerState p = w.initial_state();
            double apex = 0;
            while (p.running() && p.tick < 700) {
                w.step(p, input(p.tick + 1));
                apex = std::max(apex, p.y);
            }
            return apex;
        };
        const Input one_jump = held_during(185, 185);
        const Input jump_then_orb = [](std::uint32_t t) { return t == 185 || (t >= 225 && t <= 226); };
        const double without = apex_with(one_jump);
        const double with = apex_with(jump_then_orb);
        CHECK(with > without + 20);
        // Holding from the jump does not count as a press on the orb.
        CHECK_NEAR(apex_with(held_during(185, 260)), without, 1e-9);
    }
    {
        // Mechanics that are not implemented stop the run with a reason.
        const World slope = make_world("kA2,0;1,289,2,300,3,15;" + kFar);
        CHECK(run(slope, kNever, 600).fault == Fault::Slope);
        const World dual = make_world("kA2,0;1,286,2,300,3,45;" + kFar);
        CHECK(run(dual, kNever, 600).fault == Fault::DualPortal);
        const World teleport = make_world("kA2,0;1,747,2,300,3,45;" + kFar);
        CHECK(run(teleport, kNever, 600).fault == Fault::TeleportPortal);
        const World tilted = make_world("kA2,0;1,1,2,300,3,15,6,30;" + kFar);
        CHECK(run(tilted, kNever, 600).fault == Fault::RotatedSolid);
        // Robot on a pad: the strength is not known, so it does not guess.
        const World robot_pad = make_world("kA2,5;1,35,2,300,3,2;" + kFar);
        CHECK(run(robot_pad, kNever, 600).fault == Fault::UnknownBoost);
    }
    {
        // Unknown objects and triggers are not part of the collision world.
        const World w = make_world("kA2,0;1,987654,2,300,3,15;1,901,2,300,3,15;1,899,2,300,3,15;");
        CHECK(w.colliders().empty());
        CHECK(run(w, kNever, 2000).finished);
    }

    return finish("sim");
}
