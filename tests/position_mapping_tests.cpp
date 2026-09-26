// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
// Tests for the config -> core PositionSettings mapping (src/position_mapping.h).
//
// Every assertion here covers a mapping whose failure is silent in game: the
// camera still moves along the right axis, only the travel is wrong. That is
// the one class of positional bug a playtest does not catch, and it has bitten
// this fleet more than once.

#include <cstdio>

#include "position_mapping.h"
#include "test_check.h"

namespace {

using headtracking::Config;
using headtracking::MakePositionSettings;

void TestVerticalLimitsMapToTheirOwnBounds() {
    std::printf("PositionSettings vertical limits\n");

    // PositionLimitY bounds upward travel and PositionLimitYDown downward, and
    // neither takes its value from the other.
    Config c;
    c.position.limit_y = 0.40f;
    c.position.limit_y_down = 0.05f;
    const auto ps = MakePositionSettings(c);
    Check(ps.limit_y == 0.40f, "PositionLimitY maps to the upward bound");
    Check(ps.limit_y_down == 0.05f, "PositionLimitYDown maps to the downward bound");
}

void TestForwardLeanKeepsTheGenerousBound() {
    std::printf("PositionSettings Z asymmetry\n");

    // Z is the axis that stays asymmetric on purpose: the processor clamps to
    // [-limit_z, +limit_z_back], so the generous allowance has to sit on
    // limit_z (leaning in) and the tight one on limit_z_back (pulling back).
    const auto ps = MakePositionSettings(Config{});
    Check(ps.limit_z == 0.40f, "PositionLimitZ maps to the forward bound");
    Check(ps.limit_z_back == 0.10f, "PositionLimitZBack maps to the backward bound");
    Check(ps.limit_z > ps.limit_z_back, "forward lean keeps the generous allowance");
}

void TestNothingShapesThePose() {
    std::printf("PositionSettings sensitivity and inversion\n");

    // The tracker-to-Source axis signs are applied at the engine boundary,
    // AFTER the asymmetric Z clamp. Letting an inversion through here would
    // invert BEFORE the clamp, which swaps the 0.40m forward allowance onto the
    // backward lean - direction fixed, travel quietly broken. The gain is the
    // tracker's, so it stays at identity.
    Config c;
    c.position.sensitivity_x = 2.0f;
    c.position.invert_z = true;
    const auto ps = MakePositionSettings(c);
    Check(!ps.invert_x && !ps.invert_y && !ps.invert_z,
          "the processor's own inversion stays off");
    Check(ps.sensitivity_x == 1.0f && ps.sensitivity_y == 1.0f && ps.sensitivity_z == 1.0f,
          "position sensitivity stays at identity");
}

void TestLimitsMapStraightThrough() {
    std::printf("PositionSettings limits\n");

    Config c;
    c.position.limit_x = 0.11f;
    c.position.limit_z = 0.22f;
    c.position.limit_z_back = 0.33f;
    const auto ps = MakePositionSettings(c);
    Check(ps.limit_x == 0.11f && ps.limit_z == 0.22f && ps.limit_z_back == 0.33f,
          "per-axis limits map straight through");
}

}  // namespace

int RunPositionMappingTests() {
    std::printf("\nPosition mapping\n================\n");
    TestVerticalLimitsMapToTheirOwnBounds();
    TestForwardLeanKeepsTheGenerousBound();
    TestNothingShapesThePose();
    TestLimitsMapStraightThrough();
    return g_failures;
}
