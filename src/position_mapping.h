// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#pragma once

#include "cameraunlock/data/position_settings.h"
#include "config.h"

namespace headtracking {

// Config -> core PositionSettings. Pure and header-only so tests/ can lock it:
// a limit landing on the wrong bound is silent when wrong (the camera still
// moves, only the travel is off), which is exactly the failure no in-game
// playtest catches.
//
// Only the five limits come across. The sensitivities stay at identity: the
// tracker shapes the pose. The processor's own inversion stays off: the
// tracker-to-Source axis signs are a fixed conversion applied at the engine
// boundary in tracker_axes.h, AFTER the asymmetric Z clamp below. Inverting
// here would happen before it, which silently swaps the generous 0.40m forward
// allowance onto the backward lean and leaves 0.10m for leaning in - the
// direction looks fixed while the travel quietly breaks.
inline cameraunlock::PositionSettings MakePositionSettings(const Config& c) {
    cameraunlock::PositionSettings ps;
    ps.limit_x = c.position.limit_x;
    ps.limit_y = c.position.limit_y;
    ps.limit_y_down = c.position.limit_y_down;
    // Z stays asymmetric: leaning in wants far more room than pulling back
    // (player-model clipping). The processor clamps z to
    // [-limit_z, +limit_z_back].
    ps.limit_z = c.position.limit_z;
    ps.limit_z_back = c.position.limit_z_back;

    // local_smoothing / remote_smoothing are left at their defaults on purpose:
    // the session owns that pair and writes it into these settings on every
    // update (see TrackerFeed::Start).
    return ps;
}

}  // namespace headtracking
