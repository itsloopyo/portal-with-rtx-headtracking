// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#pragma once

namespace headtracking::source {

// Camera maths matching the Source Engine's conventions. Our own code: no
// Valve code, header or SDK file is copied into this repository or shipped in
// its releases - see THIRD-PARTY-NOTICES.md. These are the standard
// Euler-basis formulas, and an injected view has to compose exactly the way the
// renderer's does or it will not line up, so there is only one set of results
// that works. Each is pinned against the engine's observed behaviour by the
// characterization tests in tests/.
//
// Pure functions over plain float arrays: no engine memory, no globals.
//
// The frame is Source's: x = forward, y = left, z = up, and the angle triple
// the engine calls a QAngle is (pitch, yaw, roll) in degrees.

// The aspect Source defines fov_desired against, inverted: multiplying a
// viewport's own aspect by this gives the width `ratio` below.
constexpr float kReferenceAspectInverse = 0.75f;  // 1 / (4:3)

// The engine's FOV width scaling. `ratio` is the viewport's aspect over 4:3;
// the tangent scaling keeps vertical FOV fixed while the frame gets wider.
float ScaleFovByWidthRatio(float fovDegrees, float ratio);

// The inverse: a rendered FOV back into the 4:3 reference fov_desired is
// expressed in. The tangent scaling is its own inverse under a reciprocal
// ratio, so this is the same function and not a second derivation.
float UnscaleFovByWidthRatio(float fovDegrees, float ratio);

// Builds the camera basis from an angle triple (degrees) the way the engine
// does. Named after the engine function whose behaviour it reproduces.
void AngleVectors(const float* ang, float fwd[3], float right[3], float up[3]);

// Below this the forward vector is vertical to within float noise, and yaw and
// roll collapse onto the same axis, so BasisToAngles folds the whole rotation
// into yaw and zeroes roll. Exposed because it is a behaviour boundary, not an
// implementation detail: a test that means to straddle it has to be able to see
// it, or retuning the number quietly moves both of its cases to the same side.
constexpr float kGimbalEpsilon = 0.001f;

// The inverse: recovers the angle triple from a camera basis. `left` is the
// negated right vector, matching the column order the engine stores.
void BasisToAngles(const float* fwd, const float* left, const float* up, float* ang);

// The horizontal component of a forward vector, which is what kGimbalEpsilon
// bounds. Exposed for the same reason.
float ForwardXYDistance(const float* fwd);

// How far outside the frame a projected screen coordinate is allowed to land,
// counted in whole viewports either side of it.
constexpr float kProjectedPixelBoundViewports = 8.0f;

// Bounds a projected screen coordinate to that window around a viewport axis of
// `extent` pixels.
//
// The engine's own projection answers a world point near the plane of the
// rendered camera with a coordinate thousands of viewports off screen, and one
// behind it by multiplying the normalised coordinate by 100000 outright. Both
// are finite, so a finiteness check passes them, and both are consumed as an
// int - a float past INT_MAX has no conversion, and the value MSVC produces
// from one (INT_MIN) then overflows the addition it is fed to. A pixel past
// this bound is already several screens off, so bounding it changes nothing
// that is drawn.
//
// A non-finite `pixel` is returned unchanged: the caller rejects those, and
// silently turning one into a bound would hide it.
float BoundProjectedPixel(float pixel, int extent);

// Composes a head delta about the CAMERA's own axes rather than the world's,
// in place on `ang`. Adding the delta straight onto the QAngle (the
// world-space path) yaws about world up, which is right for normal play but
// turns into a spin once the game camera looks steeply up or down.
void ApplyCameraLocalRotation(float* ang, float dpitch, float dyaw, float droll);

}  // namespace headtracking::source
