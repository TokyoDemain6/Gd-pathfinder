// Small enums shared by the parser and the simulator.
#pragma once

#include <cstdint>
#include <string_view>

namespace gdpf {

// Order matches the level-settings key kA2.
enum class Gamemode : std::uint8_t { Cube, Ship, Ball, Ufo, Wave, Robot, Spider, Swing };
inline constexpr int kGamemodeCount = 8;

// Ordered slowest to fastest. Note this is NOT the order used by kA4, which
// stores 0 for 1x and 1 for 0.5x; see speed_from_setting().
enum class Speed : std::uint8_t { Half, Normal, Double, Triple, Quadruple };
inline constexpr int kSpeedCount = 5;

constexpr std::string_view name(Gamemode m) {
    constexpr std::string_view names[] = {"cube", "ship", "ball", "ufo",
                                          "wave", "robot", "spider", "swing"};
    return names[static_cast<int>(m)];
}

constexpr std::string_view name(Speed s) {
    constexpr std::string_view names[] = {"0.5x", "1x", "2x", "3x", "4x"};
    return names[static_cast<int>(s)];
}

constexpr Speed speed_from_setting(int kA4) {
    switch (kA4) {
        case 1: return Speed::Half;
        case 2: return Speed::Double;
        case 3: return Speed::Triple;
        case 4: return Speed::Quadruple;
        default: return Speed::Normal;
    }
}

constexpr Gamemode gamemode_from_setting(int kA2) {
    return (kA2 >= 0 && kA2 < kGamemodeCount) ? static_cast<Gamemode>(kA2) : Gamemode::Cube;
}

}  // namespace gdpf
