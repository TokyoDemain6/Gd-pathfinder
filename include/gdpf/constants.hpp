// Physics constants. Every number here is UNVERIFIED against the game until
// the trace-logger mod exists; docs/physics.md records where each one came
// from and how confident we are in it.
//
// Units
//   position   level-string units (one grid block is 30). The ground's top
//              surface is y = 0.
//   y velocity "game units", as the game stores it. One unit is 54
//              position units per second.
//   time       one tick is 1/240 s. The game advances physics in steps of
//              dt = 0.25 (measured in 60ths of a second).
#pragma once

#include <limits>

#include "gdpf/types.hpp"

namespace gdpf::constants {

inline constexpr int kTicksPerSecond = 240;
inline constexpr double kDt = 0.25;                           // 60 / 240
inline constexpr double kDtSlow = kDt * 0.9;                  // step used for all vertical motion
inline constexpr double kVelToUnitsPerSecond = 60.0 * 0.9;    // 54

// Horizontal speed: a base velocity times a speed factor. The factor is a
// 32-bit float in the binary. The base velocity is a double, except that the
// 1x value behaves as a float. That mix is inferred: it is the only one that
// reproduces the reference speeds to the last digit (see tests/test_sim.cpp).
inline constexpr double kXVelocity[kSpeedCount] = {5.980002, 5.770002f, 5.870002, 6.000002, 6.000002};
inline constexpr double kSpeedFactor[kSpeedCount] = {0.7f, 0.9f, 1.1f, 1.3f, 1.6f};

constexpr double x_step(Speed s) {
    const int i = static_cast<int>(s);
    return kDt * kXVelocity[i] * kSpeedFactor[i];
}

// Gravity and jump strength for cube and robot depend on the speed. Ball,
// spider and the flying gamemodes use kFlyGravity at every speed.
inline constexpr double kGravity[kSpeedCount] = {0.940199, 0.958199, 0.957199, 0.961199, 0.961199};
inline constexpr double kJumpVelocity[kSpeedCount] = {10.620032, 11.180032, 11.420032, 11.230032, 11.230032};
inline constexpr double kFlyGravity = 0.9582;

inline constexpr double kTerminalVelocity = 15.0;  // cube, ball, robot, spider

// Gravity multiplier per grounded gamemode.
inline constexpr double kBallGravityScale = 0.6;   // ball and spider
inline constexpr double kRobotGravityScale = 0.9;

// Mini players. Grounded gamemodes scale jump strength by kMiniScale. Flying
// gamemodes use kMiniFlyScale and their own velocity limits.
inline constexpr double kMiniScale = 0.8;
inline constexpr double kMiniFlyScale = 0.85;
inline constexpr double kFlyUpLimit = 8.0;
inline constexpr double kFlyDownLimit = -6.4;
inline constexpr double kMiniFlyUpLimit = 9.4118;
inline constexpr double kMiniFlyDownLimit = -7.5294;

inline constexpr double kUfoJump = 7.0;
inline constexpr double kMiniUfoJump = 8.0;        // before kMiniFlyScale is applied
inline constexpr double kSwingGravityScale = 0.4;
inline constexpr double kMiniSwingGravityScale = 0.6;
inline constexpr double kSwingFlipKeep = 0.8;      // fraction of velocity kept on a swing click

inline constexpr double kRobotJumpScale = 0.5;
inline constexpr double kRobotBoostLimit = 1.5;    // how long holding keeps the robot rising
inline constexpr double kBallJumpScale = 0.6f;     // a float in the binary

inline constexpr double kGravityFlipKeep = 0.5;    // velocity kept when gravity flips

// Hitboxes (full width; all are squares).
inline constexpr double kPlayerSize = 30.0;
inline constexpr double kMiniSizeScale = 0.6;
inline constexpr double kWaveSize = 10.0;
inline constexpr double kInnerSize = 9.0;          // touching a solid with this box kills

// How far below a surface the player may be and still get lifted onto it.
inline constexpr double kLandClip = 10.0;
inline constexpr double kLandClipFlying = 7.0;     // ship and ufo

// Height of the corridor each bounded gamemode flies in.
inline constexpr double kNoCeiling = std::numeric_limits<double>::infinity();
inline constexpr double kModeHeight[kGamemodeCount] = {
    kNoCeiling,  // cube
    300.0,       // ship
    240.0,       // ball
    300.0,       // ufo
    300.0,       // wave
    kNoCeiling,  // robot
    270.0,       // spider  (guess)
    300.0,       // swing   (guess)
};

inline constexpr double kPadVelocity = 16.0;       // yellow pad, cube
inline constexpr double kMaxY = 1476.3;            // leaving the top of the level kills
inline constexpr double kLevelEndPadding = 340.0;  // level ends this far past the last object (guess)

}  // namespace gdpf::constants
