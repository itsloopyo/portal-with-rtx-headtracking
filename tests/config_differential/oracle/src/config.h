// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#pragma once

#include <cstdint>

#include "cameraunlock/data/position_settings.h"
#include "cameraunlock/math/smoothing_utils.h"

#include "hotkeys.h"

namespace headtracking {

// The one home for every shipped default. The struct initializers below, the
// generated HeadTracking.ini and the fallback used when a key is missing or
// malformed all read from here, so a default cannot drift between the file a
// user is handed and the value the mod actually runs with.
constexpr uint16_t kDefaultPort = 4242;
constexpr bool     kDefaultEnableOnStartup = true;

constexpr float kDefaultSensitivity = 1.0f;
// The documented band for a rotation sensitivity. Outside it the value is
// refused rather than used: zero silently kills the axis while every other
// setting still reports healthy, and a negative one is an axis inversion the
// user cannot see they asked for, at whatever gain they typed.
constexpr float kMinSensitivity = 0.1f;
constexpr float kMaxSensitivity = 3.0f;

constexpr float kDefaultLocalSmoothing  = static_cast<float>(cameraunlock::math::kDefaultLocalSmoothing);
constexpr float kDefaultRemoteSmoothing = static_cast<float>(cameraunlock::math::kDefaultRemoteSmoothing);

constexpr bool  kDefaultPosEnabled     = true;
constexpr float kDefaultPosSensitivity = 1.0f;
constexpr float kMinPosSensitivity     = 0.0f;
constexpr float kMaxPosSensitivity     = 5.0f;
constexpr float kDefaultPosLimitX      = cameraunlock::PositionSettings{}.limit_x;
constexpr float kDefaultPosLimitY      = cameraunlock::PositionSettings{}.limit_y;
constexpr float kDefaultPosLimitZ      = cameraunlock::PositionSettings{}.limit_z;
constexpr float kDefaultPosLimitZBack  = cameraunlock::PositionSettings{}.limit_z_back;
// A lean limit is in metres of head travel. Past half a metre the render origin
// leaves the player's own volume, which puts the camera through world geometry
// and draws the frame from inside the void; at zero 6DOF is silently dead.
constexpr float kMinPosLimit = 0.01f;
constexpr float kMaxPosLimit = 0.5f;

// Source world units per metre of head travel: 1 unit is 1 inch, so this is the
// engine's units-per-metre conversion and not a tuning knob. Scaling lean is
// what SensX/SensY/SensZ are for, and they apply before the limits so the
// envelope keeps meaning metres.
constexpr float kSourceUnitsPerMetre = 39.37f;

constexpr bool  kDefaultWorldSpaceYaw = true;
constexpr float kDefaultFovOverride   = 0.0f;
// On by default. Off, there is no log file at all, and a "no head tracking"
// report cannot be answered without first asking the user to turn this on and
// play again. The log is truncated per launch (one previous generation kept),
// and both render-path diagnostics - [view] and [aim] - are throttled to one
// line per two thousand frames each, which is roughly 70 KB an hour at 120 fps.
constexpr bool  kDefaultLogToFile     = true;

// An override outside this band is refused: the renderer builds a projection
// from tan(fov/2), so a value at or past 180 has none at all and one near it
// stretches the frame into uselessness.
constexpr float kMinFovOverride = 30.0f;
constexpr float kMaxFovOverride = 150.0f;

struct Config {
    uint16_t port = kDefaultPort;
    bool enabled_on_startup = kDefaultEnableOnStartup;

    float sens_yaw = kDefaultSensitivity;
    float sens_pitch = kDefaultSensitivity;
    float sens_roll = kDefaultSensitivity;

    // Smoothing is chosen per connection: local for a tracker on this machine
    // (loopback), remote for a device on the network. Both cover rotation and
    // position.
    float local_smoothing = kDefaultLocalSmoothing;
    float remote_smoothing = kDefaultRemoteSmoothing;

    // Positional (6DOF) tracking. Head displacement is applied to the render
    // view origin only - the player's own eye position and angles are
    // untouched, so bullets, traces and physics are unaffected. See
    // camera_hook.cpp.
    bool  pos_enabled    = kDefaultPosEnabled;
    float pos_sens_x     = kDefaultPosSensitivity;
    float pos_sens_y     = kDefaultPosSensitivity;
    float pos_sens_z     = kDefaultPosSensitivity;
    // Head-movement envelope in metres, clamped before the conversion to Source
    // units. Z is asymmetric: pos_limit_z = forward lean (generous), z_back =
    // backward.
    float pos_limit_x      = kDefaultPosLimitX;
    float pos_limit_y      = kDefaultPosLimitY;
    float pos_limit_z      = kDefaultPosLimitZ;
    float pos_limit_z_back = kDefaultPosLimitZBack;

    int toggle_vk     = hotkeys::kVkEnd;
    int yaw_mode_vk   = hotkeys::kVkPageDown;
    // Page Up: cycles 6DOF -> rotation -> position.
    int mode_cycle_vk = hotkeys::kVkPageUp;

    // true  = horizon-locked yaw (yaw around world up axis, default)
    // false = camera-local yaw (yaw composed with pitch/roll)
    bool world_space_yaw = kDefaultWorldSpaceYaw;

    // Field of view in the same units as the game's own `fov_desired` cvar:
    // horizontal degrees referenced to a 4:3 screen, which the mod widens for
    // the actual viewport exactly as the engine does. 0 = leave the game's FOV
    // alone. Written straight into the render view the frame is built from,
    // which is also what keeps the crosshair reprojection consistent with it -
    // the reticle is projected through the engine's own matrices for that view.
    float fov_override = kDefaultFovOverride;

    // The weapon is drawn through a second FOV (CViewSetup::fovViewmodel). A
    // wider world FOV leaves the gun looking oversized against it; LOWER this
    // to shrink the gun. Same units, same 0 = leave alone.
    float fov_viewmodel_override = kDefaultFovOverride;

    // Reads [Debug] LogToFile on its own, before the rest of the config: the
    // log has to be open for the bootstrap lines that precede a full load, and
    // opening it is exactly what LogToFile=0 asks us not to do. Returns the
    // default when the ini does not exist yet (it is written later, with the
    // default in it).
    static bool FileLoggingRequested();

    // Reads <game folder>\HeadTracking.ini, writing it with the defaults above
    // first if it is not there yet. Every value is validated on the way in, so
    // nothing downstream re-checks one.
    static Config LoadOrCreateDefault();
};

}  // namespace headtracking
