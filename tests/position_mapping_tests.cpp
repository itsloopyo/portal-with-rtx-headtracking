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

void TestVerticalLimitIsSymmetric() {
    std::printf("PositionSettings vertical limit\n");

    // The INI exposes a single LimitY, so it must reach both bounds. The core
    // struct has a separate limit_y_down that defaults to 0.20 independently:
    // leaving it unset pinned downward travel at 0.20m no matter what the user
    // configured, so LimitY = 0.40 gave 0.40 up and 0.20 down.
    Config c;
    c.pos_limit_y = 0.40f;
    const auto raised = MakePositionSettings(c);
    Check(raised.limit_y == 0.40f, "LimitY raises the upward bound");
    Check(raised.limit_y_down == 0.40f, "LimitY raises the downward bound too");

    c.pos_limit_y = 0.05f;
    const auto tightened = MakePositionSettings(c);
    Check(tightened.limit_y == 0.05f, "LimitY lowers the upward bound");
    Check(tightened.limit_y_down == 0.05f, "LimitY lowers the downward bound too");

    const auto defaults = MakePositionSettings(Config{});
    Check(defaults.limit_y == headtracking::kDefaultPosLimitY
              && defaults.limit_y_down == headtracking::kDefaultPosLimitY,
          "default LimitY reaches both bounds");
}

void TestForwardLeanKeepsTheGenerousBound() {
    std::printf("PositionSettings Z asymmetry\n");

    // Z is the axis that stays asymmetric on purpose: the processor clamps to
    // [-limit_z, +limit_z_back], so the generous allowance has to sit on
    // limit_z (leaning in) and the tight one on limit_z_back (pulling back).
    const auto ps = MakePositionSettings(Config{});
    Check(ps.limit_z == headtracking::kDefaultPosLimitZ, "LimitZ maps to the forward bound");
    Check(ps.limit_z_back == headtracking::kDefaultPosLimitZBack,
          "LimitZBack maps to the backward bound");
    Check(ps.limit_z > ps.limit_z_back, "forward lean keeps the generous allowance");
}

void TestInversionNeverReachesTheProcessor() {
    std::printf("PositionSettings inversion\n");

    // The tracker-to-Source axis signs are applied at the engine boundary,
    // AFTER the asymmetric Z clamp. Letting an inversion through here would
    // invert BEFORE the clamp, which swaps the 0.40m forward allowance onto the
    // backward lean - direction fixed, travel quietly broken.
    const auto ps = MakePositionSettings(Config{});
    Check(!ps.invert_x && !ps.invert_y && !ps.invert_z,
          "the processor's own inversion stays off");
}

void TestSensitivityAndLimitsMapStraightThrough() {
    std::printf("PositionSettings sensitivity and limits\n");

    Config c;
    c.pos_sens_x = 1.5f;
    c.pos_sens_y = 2.0f;
    c.pos_sens_z = 2.5f;
    c.pos_limit_x = 0.11f;
    c.pos_limit_z = 0.22f;
    c.pos_limit_z_back = 0.33f;
    const auto ps = MakePositionSettings(c);
    Check(ps.sensitivity_x == 1.5f && ps.sensitivity_y == 2.0f && ps.sensitivity_z == 2.5f,
          "per-axis sensitivity maps straight through");
    Check(ps.limit_x == 0.11f && ps.limit_z == 0.22f && ps.limit_z_back == 0.33f,
          "per-axis limits map straight through");
}

}  // namespace

int RunPositionMappingTests() {
    std::printf("\nPosition mapping\n================\n");
    TestVerticalLimitIsSymmetric();
    TestForwardLeanKeepsTheGenerousBound();
    TestInversionNeverReachesTheProcessor();
    TestSensitivityAndLimitsMapStraightThrough();
    return g_failures;
}
