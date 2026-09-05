// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#pragma once

#include <cmath>

#include "cameraunlock/camera/zoom_compensation.h"
#include "source_math.h"

namespace headtracking {

// ----- Tracker -> Source axis mapping ---------------------------------------
//
// Every sign correction between the tracker frame and Source lives here, at the
// engine boundary, and it is applied AFTER the position processor's asymmetric
// Z clamp. It must not be expressed as an ini inversion key: the processor
// applies inversion BEFORE that clamp, so a flipped Z there silently moves the
// generous LimitZ (0.40m) allowance onto the backward lean and leaves LimitZBack
// (0.10m) for leaning in. The direction still looks right, which is why that
// shape survives testing - the only symptom is that leaning in barely moves.
//
// Header-only and free of engine state so tests/ can pin it. A wrong sign here
// is silent in a diff, silent under /W4, and reaches the player as a view that
// leans the wrong way.
//
// Tracker frame, as the pipeline delivers it:
//     yaw   > 0  = head turns right   Source yaw   > 0 = turn left   -> negate
//     pitch > 0  = head looks up      Source pitch > 0 = look down   -> negate
//     roll  > 0  = head tilts left    Source roll  > 0 = tilt right  -> negate
//     x     > 0  = head moves left    Source right vector            -> negate
//     y     > 0  = head moves up      Source up vector               -> as-is
//     z     < 0  = head leans forward Source forward vector          -> negate
constexpr float kYawSign   = -1.0f;
constexpr float kPitchSign = -1.0f;
constexpr float kRollSign  = -1.0f;
constexpr float kPosXSign  = -1.0f;
constexpr float kPosYSign  =  1.0f;
constexpr float kPosZSign  = -1.0f;

// Source clamps the player's own pitch to just short of vertical (cl_pitchup /
// cl_pitchdown) because a view past it is unusable: the frame renders inverted
// and mouse yaw then reads backwards. The head delta is added on top of an
// already-clamped view angle, so without this it can carry the composed pitch
// past vertical on its own - reachable in ordinary play, since aiming at a
// ceiling portal puts the clean pitch near the engine's own limit and any
// upward head movement is then over it.
//
// Returns the pitch delta to apply. Only ever shrinks the head's contribution
// toward zero, and never reverses its sign: the base angle is the game's to
// choose, and CViewSetup::angles carries the player's eye angles PLUS punch, so
// a base already past the limit is the engine's own doing (a death cam, a
// point_viewcontrol, weapon punch on top of a clamped 89). Returning
// `limit - basePitch` there would move the camera while the tracker sits
// centred, which breaks the one property the whole mod rests on: with the head
// still, the view is exactly what the game drew.
//
// This is exact for the world-space-yaw composition, where the rendered pitch
// really is base + delta. The camera-local path composes in the camera's own
// frame and re-extracts angles, so the sum is not the quantity being bounded -
// see ApplyRotationDelta, which applies this only where it holds.
constexpr float kMaxComposedPitch = 89.0f;

inline float ClampPitchDelta(float basePitch, float deltaPitch, float limit) {
    const float composed = basePitch + deltaPitch;
    float clamped = deltaPitch;
    if (composed > limit) clamped = limit - basePitch;
    if (composed < -limit) clamped = -limit - basePitch;

    // Same sign as the head's own movement, never larger.
    if (clamped * deltaPitch < 0.0f) return 0.0f;
    if (clamped > 0.0f && clamped > deltaPitch) return deltaPitch;
    if (clamped < 0.0f && clamped < deltaPitch) return deltaPitch;
    return clamped;
}

// ----- Zoom compensation ----------------------------------------------------
//
// A narrow field of view magnifies everything in the frame, head tracking
// included: the head still turns ten degrees and the camera still turns ten
// degrees, but the picture moves further, by the ratio between the FOV being
// rendered and the game's own un-zoomed one. Unscaled, a player reads that as
// the mod's sensitivity jumping the moment the game zooms.
//
// The factor comes from fov_override.cpp, which is the only thing that knows
// what an un-zoomed frame renders at in this game (the player's fov_desired,
// widened for the viewport, and put through the [View] Fov override when that
// frame was). What is left here is spending it, which is a boundary conversion
// like the signs above and belongs beside them.
//
// Yaw and pitch TRANSLATE the image across the frame, so both scale. A lean
// translates it too, linearly, so position scales by the factor directly - that
// is a plain multiply at the call site, not a function. Roll ROTATES the image
// about the view axis, and ten degrees of head roll rolls the picture ten
// degrees at every field of view there is, so roll is left alone.

// cameraunlock::camera::ScaleAngleForZoom goes through tan() and back, which is
// only defined inside a quarter turn, and a tracker is free to send more than
// that. Past this the angle is passed through as it arrived: the view is
// already turned clear of the screen, so what the compensation would be worth
// there is moot, while tan() past 90 degrees changes sign and would send the
// view the other way.
constexpr float kMaxZoomScaledAngle = 85.0f;

// Rescales one rotation axis so it displaces the image by as much as it would
// have at the un-zoomed FOV. Degrees in, degrees out, tracker convention: this
// runs before the signs and before the pitch saturation, so the limits below
// still bound the angle the view actually receives.
inline float ScaleRotationForZoom(float deg, float zoomFactor) {
    if (!(zoomFactor > 0.0f) || std::fabs(deg) >= kMaxZoomScaledAngle) return deg;
    return cameraunlock::camera::ScaleAngleForZoom(deg, zoomFactor);
}

// ----- Compositions ---------------------------------------------------------
//
// The two places the signs above are actually spent. They live here, beside the
// table and free of engine types, because tests/ has to be able to call the same
// code the render path calls: a test that re-implements the composition passes
// just as happily when the render path stops using it.

// The world-space-yaw path. A Source QAngle is intrinsically horizon-locked -
// yaw is about world up, pitch about the yawed right axis - so adding the head
// delta straight on IS the world-space composition. Angles in Source degrees;
// `pitchDeg`/`yawDeg`/`rollDeg` arrive in the TRACKER's convention and the signs
// are applied here.
//
// Returns the pitch actually applied, which the caller reports in its
// diagnostic line: the saturation below can be smaller than what was asked for.
inline float ComposeWorldSpaceDelta(float* ang, float yawDeg, float pitchDeg, float rollDeg) {
    const float pitch = ClampPitchDelta(ang[0], pitchDeg * kPitchSign, kMaxComposedPitch);
    ang[0] += pitch;
    ang[1] += yawDeg * kYawSign;
    ang[2] += rollDeg * kRollSign;
    return pitch;
}

// The positional lean, applied in a HORIZON-LOCKED basis: only the clean yaw is
// used, so forward is flat and up is world up. The mouse turns the body, so yaw
// belongs in the basis; pitch is the neck, and the room a head moves through
// does not tilt with it. Building the basis from the full clean QAngle instead
// makes a head raised in the seat move the camera BACKWARDS whenever the player
// is looking down, and a lean forward drive it into the floor - measured at a
// clean pitch of 80 degrees, where "up" resolved to (-0.985, 0, 0.174), almost
// pure horizontal. Portal spends most of its time looking at floors and ceilings
// for portal surfaces, so that is the common case, not the corner one.
//
// `x`/`y`/`z` arrive in the TRACKER's convention, already clamped to the lean
// envelope and converted to Source units; the signs are applied here.
inline void ApplyHorizonLockedLean(const float* cleanAngles, float* org, float x, float y,
                                   float z) {
    const float flatAngles[3] = { 0.0f, cleanAngles[1], 0.0f };
    float fwd[3], right[3], up[3];
    source::AngleVectors(flatAngles, fwd, right, up);

    const float dx = x * kPosXSign;
    const float dy = y * kPosYSign;
    const float dz = z * kPosZSign;
    for (int i = 0; i < 3; ++i) {
        org[i] += right[i] * dx + up[i] * dy + fwd[i] * dz;
    }
}

}  // namespace headtracking
