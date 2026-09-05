// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
// Tests for the tracker-to-Source axis conversion (src/tracker_axes.h).
//
// A wrong sign here is invisible in a diff, invisible under /W4, and reaches the
// player as a view that turns or leans the wrong way. It is the most-repeated
// defect across the fleet, and until now the only thing pinning these six
// constants was a comment.
//
// The assertions are directional rather than numeric on purpose: what matters is
// that turning your head right turns the view right, not that a constant equals
// -1. Rewriting the mapping in some other form still has to satisfy these.

#include <cmath>
#include <cstdio>

#include "angles.h"
#include "source_math.h"
#include "test_check.h"
#include "tracker_axes.h"

namespace {

using namespace headtracking;

// Thin aliases, not re-implementations. Both call the functions the render path
// calls, so deleting the clamp from ComposeWorldSpaceDelta or rebuilding the
// lean basis from the full QAngle fails these tests rather than sailing past a
// private copy that still does the right thing.
void ComposeWorldSpace(float* ang, float yawDeg, float pitchDeg, float rollDeg) {
    ComposeWorldSpaceDelta(ang, yawDeg, pitchDeg, rollDeg);
}

void ComposeLean(const float* cleanAngles, float* org, float x, float y, float z) {
    ApplyHorizonLockedLean(cleanAngles, org, x, y, z);
}

// Source: yaw increases counter-clockwise seen from above, pitch is positive
// DOWN, roll is positive tilting right.
void TestRotationSignsFaceTheRightWay() {
    std::printf("rotation axis signs\n");

    // Head turns right. Source yaw must DECREASE (it counts anti-clockwise).
    float ang[3] = { 0.0f, 0.0f, 0.0f };
    ComposeWorldSpace(ang, 20.0f, 0.0f, 0.0f);
    Check(ang[1] < 0.0f, "turning the head right turns the view right");

    // Head looks up. Source pitch is positive down, so it must DECREASE.
    float up[3] = { 0.0f, 0.0f, 0.0f };
    ComposeWorldSpace(up, 0.0f, 20.0f, 0.0f);
    Check(up[0] < 0.0f, "raising the head raises the view");

    // Head tilts left. Source roll is positive tilting right, so it decreases.
    float roll[3] = { 0.0f, 0.0f, 0.0f };
    ComposeWorldSpace(roll, 0.0f, 0.0f, 20.0f);
    Check(roll[2] < 0.0f, "tilting the head left tilts the view left");
}

// The one the doctrine calls out by name: the processor's z is NEGATIVE for a
// forward lean, so the generous LimitZ sits on negative z. A mirrored sign here
// is the "leaning in barely moves, pulling back moves a lot" report.
void TestLeanSignsMoveTheRightWay() {
    std::printf("position axis signs\n");

    // Facing +X (yaw 0). A forward lean is negative z out of the processor and
    // must move the camera along +X.
    const float facingX[3] = { 0.0f, 0.0f, 0.0f };
    float org[3] = { 0.0f, 0.0f, 0.0f };
    ComposeLean(facingX, org, 0.0f, 0.0f, -0.3f);
    Check(org[0] > 0.0f, "leaning in moves the camera forward");

    float back[3] = { 0.0f, 0.0f, 0.0f };
    ComposeLean(facingX, back, 0.0f, 0.0f, 0.1f);
    Check(back[0] < 0.0f, "pulling back moves the camera backward");

    // Head moves right. The tracker reports +x for a move LEFT, so a rightward
    // move is negative x and must put the camera on Source's right, which is -Y
    // when facing +X.
    float right[3] = { 0.0f, 0.0f, 0.0f };
    ComposeLean(facingX, right, -0.3f, 0.0f, 0.0f);
    Check(right[1] < 0.0f, "leaning right moves the camera right");

    float upward[3] = { 0.0f, 0.0f, 0.0f };
    ComposeLean(facingX, upward, 0.0f, 0.2f, 0.0f);
    Check(upward[2] > 0.0f, "raising the head raises the camera");
}

// The basis is built from the clean YAW alone. Built from the full QAngle,
// "up" at a steep pitch resolves almost horizontal, so raising your head in the
// seat drives the camera backwards and leaning in drives it into the floor -
// and Portal looks at floors and ceilings constantly.
void TestLeanBasisIsHorizonLocked() {
    std::printf("lean basis is horizon locked\n");

    const float lookingDown[3] = { 80.0f, 0.0f, 0.0f };
    float org[3] = { 0.0f, 0.0f, 0.0f };
    ComposeLean(lookingDown, org, 0.0f, 0.2f, 0.0f);
    Check(org[2] > 0.0f, "raising the head still raises the camera at a steep pitch");
    Check(org[0] == 0.0f && org[1] == 0.0f,
          "and does not slide it horizontally");

    float forward[3] = { 0.0f, 0.0f, 0.0f };
    ComposeLean(lookingDown, forward, 0.0f, 0.0f, -0.3f);
    Check(forward[0] > 0.0f, "leaning in still moves along the ground, not into the floor");
    Check(forward[2] == 0.0f, "with no vertical component");
}

// Source clamps the player's own pitch just short of vertical because a view
// past it renders inverted and mouse yaw reads backwards. The head delta is
// added on top of an already-clamped angle, so without this it can carry the
// composed pitch over on its own - reachable whenever the player aims at a
// ceiling portal, which puts the clean pitch at the engine's limit already.
void TestPitchSaturatesAtVertical() {
    std::printf("composed pitch saturates\n");

    // Aiming at a ceiling, then raising the head.
    float ang[3] = { -89.0f, 0.0f, 0.0f };
    ComposeWorldSpace(ang, 0.0f, 25.0f, 0.0f);
    Check(ang[0] >= -kMaxComposedPitch, "an upward head delta cannot tip the view over the top");

    float down[3] = { 89.0f, 0.0f, 0.0f };
    ComposeWorldSpace(down, 0.0f, -25.0f, 0.0f);
    Check(down[0] <= kMaxComposedPitch, "nor a downward one under the bottom");

    // The view stays upright, which is the property that actually matters.
    float fwd[3], right[3], up[3];
    source::AngleVectors(ang, fwd, right, up);
    Check(up[2] > 0.0f, "the rendered up vector still points up");

    // And an ordinary pose is untouched.
    Check(ClampPitchDelta(0.0f, -20.0f, kMaxComposedPitch) == -20.0f,
          "a delta well inside the limit passes through unchanged");
    Check(ClampPitchDelta(80.0f, 20.0f, kMaxComposedPitch) == 9.0f,
          "and one that crosses it saturates exactly at the limit");

    // The clamp must never be a SOURCE of rotation. CViewSetup::angles carries
    // the player's eye angles plus punch, and a death cam or point_viewcontrol
    // sets what it likes, so a base past the limit is the game's own doing. A
    // clamp that answered "limit - base" there would move the camera while the
    // tracker sat perfectly still, which is the one property the mod rests on:
    // head still, view exactly as the game drew it.
    const float outOfBand[] = { 91.0f, 120.0f, -95.0f, -180.0f };
    for (float base : outOfBand) {
        Check(ClampPitchDelta(base, 0.0f, kMaxComposedPitch) == 0.0f,
              "a centred tracker injects nothing, whatever the game's own pitch is");
    }

    // Nor may it reverse or amplify what the head did.
    Check(ClampPitchDelta(120.0f, -10.0f, kMaxComposedPitch) == -10.0f,
          "a head movement back toward the limit is passed through in full");
    Check(ClampPitchDelta(120.0f, 10.0f, kMaxComposedPitch) == 0.0f,
          "and one that would go further past it is dropped, not reversed");
}

// The zoom scaling has to be exact where it is claimed to be exact - the image
// displacement of an angle goes as tan(angle) / tan(fov/2), so holding that
// fixed means tan(out) = tan(in) * factor - and it has to be a no-op where the
// game is not zoomed, because that is every frame of ordinary play.
void TestZoomScalingHoldsScreenDisplacement() {
    std::printf("zoom compensation\n");

    Check(ScaleRotationForZoom(20.0f, 1.0f) == 20.0f,
          "an un-zoomed frame leaves the angle exactly as it arrived");

    // Half the field of view, so the picture moves twice as far per degree and
    // the angle has to be halved in tangent to compensate.
    const float halved = ScaleRotationForZoom(20.0f, 0.5f);
    Check(halved > 0.0f && halved < 20.0f, "a zoom shrinks the angle it is given");
    Check(std::fabs(std::tan(halved * kDegToRad)
                    - 0.5f * std::tan(20.0f * kDegToRad)) < 1e-5f,
          "and shrinks it by exactly the factor, in tangent");

    // A wider frame than the un-zoomed one. The [View] Fov override does not
    // produce this, because the base widens along with it, but a map or a
    // console fov that pulls the FOV out would.
    Check(ScaleRotationForZoom(20.0f, 2.0f) > 20.0f,
          "a wider than normal frame grows the angle");

    Check(ScaleRotationForZoom(-20.0f, 0.5f) < 0.0f,
          "the scaling never reverses an axis");

    // Past a quarter turn tan() changes sign, which would send the view the
    // other way. The angle is passed through instead.
    Check(ScaleRotationForZoom(120.0f, 0.5f) == 120.0f,
          "an angle outside tan()'s domain is passed through untouched");
    Check(ScaleRotationForZoom(-120.0f, 0.5f) == -120.0f,
          "including a negative one");

    // A factor that could not be measured is a factor of nothing, not a zero.
    Check(ScaleRotationForZoom(20.0f, 0.0f) == 20.0f,
          "a factor of zero is refused rather than killing the axis");
    Check(ScaleRotationForZoom(20.0f, -1.0f) == 20.0f, "and so is a negative one");
}

}  // namespace

int RunTrackerAxesTests() {
    std::printf("\nTracker axes\n============\n");
    TestRotationSignsFaceTheRightWay();
    TestLeanSignsMoveTheRightWay();
    TestLeanBasisIsHorizonLocked();
    TestPitchSaturatesAtVertical();
    TestZoomScalingHoldsScreenDisplacement();
    return g_failures;
}
