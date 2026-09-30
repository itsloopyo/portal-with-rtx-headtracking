// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
// Characterization tests for the Source Engine camera maths the render-view
// detour applies (src/source_math.cpp).
//
// These lock BEHAVIOUR, not a derivation: the expected values were captured
// from the shipped implementation, which composes the view exactly as the
// engine's own basis maths does - an injected view that composes differently
// does not line up, so there is only one set of results that works. A change
// that moves any of them changes what the player sees, and no in-game
// playtest catches a combined-axis drift automatically.

#include <cmath>
#include <cstdio>

#include "source_math.h"
#include "test_check.h"

namespace {

using namespace headtracking::source;

bool NearEqual(float a, float b, float eps = 1e-3f) {
    return std::fabs(a - b) <= eps;
}

void CheckTriple(const float actual[3], float e0, float e1, float e2, const char* name,
                 float eps = 1e-3f) {
    const bool ok = NearEqual(actual[0], e0, eps) && NearEqual(actual[1], e1, eps)
                    && NearEqual(actual[2], e2, eps);
    if (!ok) {
        std::printf("    expected (%.6g, %.6g, %.6g) got (%.6g, %.6g, %.6g)\n", e0, e1, e2,
                    actual[0], actual[1], actual[2]);
    }
    Check(ok, name);
}

// The width ratio for a viewport, exactly as camera_hook.cpp computes it.
float WidthRatio(int w, int h) {
    return (static_cast<float>(w) / static_cast<float>(h)) * kReferenceAspectInverse;
}

void TestScaleFovByWidthRatio() {
    std::printf("ScaleFovByWidthRatio\n");
    Check(NearEqual(ScaleFovByWidthRatio(90.0f, 1.0f), 90.0f), "4:3 viewport leaves FOV alone");
    // Measured in game: fov_desired 90 in a 1280x800 window arrives at
    // RenderView as 100.39.
    Check(NearEqual(ScaleFovByWidthRatio(90.0f, WidthRatio(1280, 800)), 100.388855f),
          "90 at 1280x800 widens to 100.39");
    Check(NearEqual(ScaleFovByWidthRatio(90.0f, WidthRatio(1920, 1080)), 106.260201f),
          "90 at 16:9 widens to 106.26");
    Check(NearEqual(ScaleFovByWidthRatio(75.0f, WidthRatio(1280, 800)), 85.2772598f),
          "75 at 1280x800 widens to 85.28");
    Check(NearEqual(ScaleFovByWidthRatio(120.0f, WidthRatio(1280, 800)), 128.613235f),
          "120 at 1280x800 widens to 128.61");
    // Ultra-wide: still finite and below the 179 degree refusal threshold.
    Check(ScaleFovByWidthRatio(120.0f, WidthRatio(1280, 400)) < 179.0f,
          "120 on a 32:10 tile stays renderable");
}

void TestAngleVectors() {
    std::printf("AngleVectors\n");
    float fwd[3], right[3], up[3];

    const float identity[3] = { 0.0f, 0.0f, 0.0f };
    AngleVectors(identity, fwd, right, up);
    CheckTriple(fwd, 1.0f, 0.0f, 0.0f, "identity forward is +x");
    CheckTriple(right, 0.0f, -1.0f, 0.0f, "identity right is -y (Source is y-left)");
    CheckTriple(up, 0.0f, 0.0f, 1.0f, "identity up is +z");

    const float yaw90[3] = { 0.0f, 90.0f, 0.0f };
    AngleVectors(yaw90, fwd, right, up);
    CheckTriple(fwd, 0.0f, 1.0f, 0.0f, "yaw 90 forward is +y");
    CheckTriple(right, 1.0f, 0.0f, 0.0f, "yaw 90 right is +x");

    // Positive pitch looks DOWN in Source, so forward's z goes negative.
    const float pitch30[3] = { 30.0f, 0.0f, 0.0f };
    AngleVectors(pitch30, fwd, right, up);
    CheckTriple(fwd, 0.866025388f, 0.0f, -0.5f, "pitch 30 tilts forward downward");
    CheckTriple(up, 0.5f, 0.0f, 0.866025388f, "pitch 30 tilts up forward");

    const float roll45[3] = { 0.0f, 0.0f, 45.0f };
    AngleVectors(roll45, fwd, right, up);
    CheckTriple(fwd, 1.0f, 0.0f, 0.0f, "roll leaves forward alone");
    CheckTriple(right, 0.0f, -0.707106769f, -0.707106769f, "roll 45 tilts right");
    CheckTriple(up, 0.0f, -0.707106769f, 0.707106769f, "roll 45 tilts up");

    const float combined[3] = { 15.0f, 90.0f, 10.0f };
    AngleVectors(combined, fwd, right, up);
    CheckTriple(fwd, 0.0f, 0.965925813f, -0.258819044f, "combined pose forward");
    CheckTriple(right, 0.98480773f, -0.0449434109f, -0.167731255f, "combined pose right");
    CheckTriple(up, 0.173648164f, 0.254886985f, 0.951251209f, "combined pose up");
}

// Runs a QAngle out to a basis and back, which is what the camera-local yaw
// path does either side of its change of basis.
void RoundTrip(float pitch, float yaw, float roll, float e0, float e1, float e2,
               const char* name) {
    const float ang[3] = { pitch, yaw, roll };
    float fwd[3], right[3], up[3];
    AngleVectors(ang, fwd, right, up);
    const float left[3] = { -right[0], -right[1], -right[2] };
    float out[3];
    BasisToAngles(fwd, left, up, out);
    CheckTriple(out, e0, e1, e2, name);
}

void TestBasisToAngles() {
    std::printf("BasisToAngles\n");
    RoundTrip(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, "identity round-trips");
    RoundTrip(30.0f, 0.0f, 0.0f, 30.0f, 0.0f, 0.0f, "pitch round-trips");
    RoundTrip(0.0f, 90.0f, 0.0f, 0.0f, 90.0f, 0.0f, "yaw round-trips");
    RoundTrip(0.0f, 0.0f, 45.0f, 0.0f, 0.0f, 45.0f, "roll round-trips");
    RoundTrip(15.0f, 90.0f, 10.0f, 15.0f, 90.0f, 10.0f, "combined pose round-trips");
    RoundTrip(-89.9f, 45.0f, 0.0f, -89.9f, 45.0f, 0.0f,
              "just short of vertical stays on the normal branch");

    // Straight up / down: yaw and roll are the same axis, so Source folds the
    // whole rotation into yaw and zeroes roll. Yaw 30 + roll 20 comes back as
    // yaw 50.
    RoundTrip(90.0f, 45.0f, 0.0f, 90.0f, 45.0f, 0.0f, "straight down folds into yaw");
    RoundTrip(-90.0f, 30.0f, 20.0f, -90.0f, 50.0f, 0.0f,
              "straight up folds roll into yaw and zeroes roll");
}

void TestBoundProjectedPixel() {
    std::printf("BoundProjectedPixel\n");

    // A pixel anywhere on, or just off, the frame is the answer the engine's own
    // projection gave, and must survive untouched.
    Check(BoundProjectedPixel(0.0f, 1920) == 0.0f, "the left edge is left alone");
    Check(BoundProjectedPixel(960.5f, 1920) == 960.5f, "a centred pixel is left alone");
    Check(BoundProjectedPixel(1920.0f, 1920) == 1920.0f, "the right edge is left alone");
    Check(BoundProjectedPixel(-500.0f, 1920) == -500.0f, "just off the left edge is left alone");
    Check(BoundProjectedPixel(2500.0f, 1920) == 2500.0f, "just off the right edge is left alone");

    // ScreenTransform multiplies by 100000 for a point behind the rendered view,
    // and divides by a near-zero w for one close to its plane. Both are finite,
    // so the finiteness check passes them through; the pixel they produce does
    // not fit an int, and the consumers convert to one.
    const float behind = BoundProjectedPixel(9.6e10f, 1920);
    Check(behind == 1920.0f + kProjectedPixelBoundViewports * 1920.0f,
          "a coordinate from a point behind the camera is bounded");
    Check(BoundProjectedPixel(-9.6e10f, 1920) == -kProjectedPixelBoundViewports * 1920.0f,
          "the same on the negative side");

    // What the bound is for: the crosshair shift converts the bounded pixel to
    // an int and adds it to a draw coordinate. Both have to stay inside the
    // range, on the widest viewport anyone runs.
    const float wide = BoundProjectedPixel(1e30f, 7680);
    Check(wide < 1.0e6f, "the bound leaves room for the int conversion");
    Check(static_cast<double>(wide) + 7680.0 < 2147483647.0,
          "a bounded pixel plus a draw coordinate stays inside an int");

    // A non-finite input is the caller's to reject - it is checked before this
    // is reached - so the bound must not quietly turn one into a real pixel.
    const float nan = std::sqrt(-1.0f);
    Check(std::isnan(BoundProjectedPixel(nan, 1920)), "a NaN is passed through, not bounded");
}

void CheckCameraLocal(float basePitch, float baseYaw, float baseRoll, float dpitch, float dyaw,
                      float droll, float e0, float e1, float e2, const char* name) {
    float ang[3] = { basePitch, baseYaw, baseRoll };
    ApplyCameraLocalRotation(ang, dpitch, dyaw, droll);
    CheckTriple(ang, e0, e1, e2, name);
}

void TestApplyCameraLocalRotation() {
    std::printf("ApplyCameraLocalRotation\n");
    CheckCameraLocal(0, 0, 0, 0, 0, 0, 0, 0, 0, "zero delta on identity is a no-op");
    CheckCameraLocal(30, 45, 0, 0, 0, 0, 30, 45, 0, "zero delta preserves the base pose");

    // At the horizon the camera's own axes line up with the world's, so a
    // camera-local delta lands exactly where the world-space path would.
    CheckCameraLocal(0, 90, 0, 0, 25, 0, 0, 115, 0, "level camera: yaw adds like world yaw");
    CheckCameraLocal(0, 90, 0, 15, 0, 0, 15, 90, 0, "level camera: pitch adds");
    CheckCameraLocal(0, 90, 0, 0, 0, 10, 0, 90, 10, "level camera: roll adds");

    // Steeply pitched, which is the case the world-space path gets wrong: the
    // delta is composed about the camera's axes, so it feeds back into all
    // three angles. These values were checked by hand against the change of
    // basis.
    CheckCameraLocal(70, 90, 0, 0, 25, 0, 58.3916664f, 143.741272f, 49.26408f,
                     "steep pitch: yaw spreads across pitch, yaw and roll");
    CheckCameraLocal(70, 90, 0, 15, 0, 0, 85.0f, 90.0f, 0.0f,
                     "steep pitch: pitch still adds straight on");
    CheckCameraLocal(30, 45, 0, 15, 25, 10, 41.4416428f, 77.9945374f, 26.3727093f,
                     "moderate pitch with a combined delta");
    CheckCameraLocal(-60, 180, 0, -20, -30, -5, -61.1373672f, 103.25769f, 58.7720413f,
                     "negative pitch with a combined delta");
}

// The FOV override composes Unscale then Scale (fov_override.cpp): the unscale
// puts the frame's rendered FOV back into the 4:3 reference fov_desired is
// expressed in, so the player's ratio means what they asked for. Dropping the
// reciprocal leaves every non-4:3 display rendering at the wrong FOV in the same
// direction every frame, which reads in game as "the mod changed my FOV" rather
// than as a bug, and no other assertion in this file would move.
void TestFovScaleRoundTrip() {
    std::printf("FOV scale round trip\n");

    const float ratios[] = {
        1.0f,                      // 4:3
        (16.0f / 10.0f) * 0.75f,   // 16:10
        (16.0f / 9.0f) * 0.75f,    // 16:9
        (32.0f / 10.0f) * 0.75f,   // 32:10
    };
    const float fovs[] = { 54.0f, 75.0f, 90.0f, 120.0f };
    for (float ratio : ratios) {
        for (float fov : fovs) {
            const float there = ScaleFovByWidthRatio(fov, ratio);
            const float back = UnscaleFovByWidthRatio(there, ratio);
            Check(NearEqual(back, fov, 1e-3f), "Unscale undoes Scale at the same ratio");
        }
    }

    // Direction, not just invertibility: a wider-than-4:3 viewport widens the
    // rendered FOV, so unscaling one has to narrow it.
    const float wide = (16.0f / 9.0f) * 0.75f;
    Check(UnscaleFovByWidthRatio(90.0f, wide) < 90.0f,
          "unscaling a 16:9 frame narrows it back toward the 4:3 reference");
}

// The gimbal branch of BasisToAngles is reachable in ordinary play through the
// camera-local path - a floor portal puts the clean pitch near 89, and one
// degree of head pitch lands inside the epsilon. The two triples either side of
// it describe the SAME rotation (yaw and roll are one axis at the pole), which
// is why the branch is benign; nothing pinned that, and it is one sign change
// away from a 20-degree yaw snap no playtest would reliably reproduce.
void TestCameraLocalRotationIsContinuousAtThePole() {
    std::printf("camera-local rotation at the pole\n");

    float justOutside[3] = { 89.0f, 45.0f, 0.0f };
    float justInside[3]  = { 89.0f, 45.0f, 0.0f };
    ApplyCameraLocalRotation(justOutside, 0.9f, 0.0f, 20.0f);
    ApplyCameraLocalRotation(justInside, 0.95f, 0.0f, 20.0f);

    float fwdA[3], rightA[3], upA[3];
    float fwdB[3], rightB[3], upB[3];
    AngleVectors(justOutside, fwdA, rightA, upA);
    AngleVectors(justInside, fwdB, rightB, upB);

    // Assert the straddle rather than assume it. These two poses sit either
    // side of kGimbalEpsilon by a factor of about two, so retuning it by a
    // fraction of a degree of head pitch would silently put both on the same
    // side - and the test would keep passing while covering nothing.
    Check(ForwardXYDistance(fwdA) > kGimbalEpsilon,
          "the first pose is outside the gimbal branch");
    Check(ForwardXYDistance(fwdB) < kGimbalEpsilon,
          "and the second is inside it");
    for (int i = 0; i < 3; ++i) {
        Check(NearEqual(fwdA[i], fwdB[i], 0.01f), "forward is continuous across the gimbal epsilon");
        Check(NearEqual(upA[i], upB[i], 0.01f), "up is continuous across the gimbal epsilon");
    }
}

// Where the clean aim axis lands in a pass drawn with `angles` through a
// projection of horizontal FOV `fovDeg`, in NDC (x right, y up). The screen
// position is what the player sees, so it is what the weapon pass is held to.
void AxisNdc(const float* angles, const float* clean, float fovDeg, float& x, float& y) {
    float fwd[3], right[3], up[3], aim[3], r2[3], u2[3];
    AngleVectors(angles, fwd, right, up);
    AngleVectors(clean, aim, r2, u2);
    const float f = aim[0] * fwd[0] + aim[1] * fwd[1] + aim[2] * fwd[2];
    const float r = aim[0] * right[0] + aim[1] * right[1] + aim[2] * right[2];
    const float u = aim[0] * up[0] + aim[1] * up[1] + aim[2] * up[2];
    const float t = std::tan(fovDeg * 0.5f * 3.14159265f / 180.0f);
    x = r / f / t;
    y = u / f / t;
}

void TestWeaponPassAngles() {
    std::printf("WeaponPassAngles\n");
    // The pair this game renders at 1920x1080: fov_desired 75 and viewmodel_fov
    // 54, both widened for 16:9 (the [view] line reads fov=91.31/68.38).
    const float worldFov = 91.31f;
    const float weaponFov = 68.38f;
    const float ratio = std::tan(weaponFov * 0.5f * 3.14159265f / 180.0f)
                        / std::tan(worldFov * 0.5f * 3.14159265f / 180.0f);

    const float clean[3] = { 12.0f, 40.0f, 0.0f };
    float out[3];

    Check(WeaponPassAngles(clean, clean, ratio, out), "coincident cameras are corrected");
    CheckTriple(out, clean[0], clean[1], clean[2], "coincident cameras leave the pass alone");

    const float turned[3] = { 4.0f, 58.0f, 7.0f };
    Check(WeaponPassAngles(turned, clean, 1.0f, out), "a ratio of 1 is corrected");
    CheckTriple(out, turned[0], turned[1], turned[2], "a ratio of 1 leaves the drawn view");

    // The whole point: the axis lands where the world pass puts it, on every
    // combination of yaw, pitch and roll, not only on a single axis.
    const float poses[][3] = {
        { 12.0f, 60.0f, 0.0f },   // yaw only
        { -8.0f, 40.0f, 0.0f },   // pitch only
        { 0.0f, 22.0f, 9.0f },    // yaw and roll
        { 30.0f, 15.0f, -12.0f }, // all three
        { -40.0f, 75.0f, 20.0f }, // large
    };
    for (const auto& pose : poses) {
        Check(WeaponPassAngles(pose, clean, ratio, out), "an in-front axis is corrected");
        float wx, wy, gx, gy, ux, uy;
        AxisNdc(pose, clean, worldFov, wx, wy);
        AxisNdc(out, clean, weaponFov, gx, gy);
        AxisNdc(pose, clean, weaponFov, ux, uy);
        const bool ok = NearEqual(wx, gx, 1e-4f) && NearEqual(wy, gy, 1e-4f);
        if (!ok) {
            std::printf("    pose (%g, %g, %g): world (%.5f, %.5f) weapon (%.5f, %.5f)\n",
                        pose[0], pose[1], pose[2], wx, wy, gx, gy);
        }
        Check(ok, "the weapon pass draws the axis where the world pass does");
        Check(std::fabs(ux) > std::fabs(wx) * 1.4f || std::fabs(uy) > std::fabs(wy) * 1.4f,
              "and the uncorrected pass does not (the fault this fixes)");
    }

    // Roll alone moves no axis off centre, so the drawn roll is kept as it is.
    const float rolled[3] = { clean[0], clean[1], 15.0f };
    Check(WeaponPassAngles(rolled, clean, ratio, out), "a rolled view is corrected");
    CheckTriple(out, rolled[0], rolled[1], rolled[2], "a pure roll is left alone");

    const float behind[3] = { 0.0f, clean[1] + 120.0f, 0.0f };
    const float sentinel[3] = { 1.0f, 2.0f, 3.0f };
    float untouched[3] = { sentinel[0], sentinel[1], sentinel[2] };
    Check(!WeaponPassAngles(behind, clean, ratio, untouched), "an axis behind the view is refused");
    CheckTriple(untouched, sentinel[0], sentinel[1], sentinel[2], "and the output is left alone");
}

// A world point's screen position in a camera, in NDC.
void PointNdc(const float* point, const float* origin, const float* angles, float fovDeg,
              float& x, float& y) {
    float fwd[3], right[3], up[3];
    AngleVectors(angles, fwd, right, up);
    const float d[3] = { point[0] - origin[0], point[1] - origin[1], point[2] - origin[2] };
    const float f = d[0] * fwd[0] + d[1] * fwd[1] + d[2] * fwd[2];
    const float t = std::tan(fovDeg * 0.5f * 3.14159265f / 180.0f);
    x = (d[0] * right[0] + d[1] * right[1] + d[2] * right[2]) / f / t;
    y = (d[0] * up[0] + d[1] * up[1] + d[2] * up[2]) / f / t;
}

// A point fixed to a pose, a given distance along each of its axes.
void PointOnPose(const float* origin, const float* angles, float f, float r, float u,
                 float* out) {
    float fwd[3], right[3], up[3];
    AngleVectors(angles, fwd, right, up);
    for (int i = 0; i < 3; ++i) out[i] = origin[i] + fwd[i] * f + right[i] * r + up[i] * u;
}

void TestCarryPose() {
    std::printf("CarryPose\n");
    const float eye[3] = { -960.0f, -128.0f, 210.6f };
    const float clean[3] = { 5.0f, 90.0f, 0.0f };
    // The gun as CalcViewModelView leaves it: at the eye, turned a little by bob.
    const float gunOrigin[3] = { eye[0] + 0.3f, eye[1] - 0.2f, eye[2] - 0.4f };
    const float gunAngles[3] = { 6.0f, 91.0f, 0.5f };

    float origin[3] = { gunOrigin[0], gunOrigin[1], gunOrigin[2] };
    float angles[3] = { gunAngles[0], gunAngles[1], gunAngles[2] };
    CarryPose(eye, clean, eye, clean, origin, angles);
    CheckTriple(origin, gunOrigin[0], gunOrigin[1], gunOrigin[2], "one camera leaves the origin");
    CheckTriple(angles, gunAngles[0], gunAngles[1], gunAngles[2], "one camera leaves the angles");

    // Head tracking's two cameras: the frame drawn from a leaned, turned eye,
    // the weapon camera on the clean eye at its corrected angles. Carried from
    // the second to the first, every point of the gun has to land on the pixel
    // the weapon camera would have drawn it at - the claw tip and the muzzle
    // as well as the gun's own origin.
    const float leaned[3] = { eye[0] + 7.87f, eye[1] - 2.0f, eye[2] + 1.5f };
    const float turned[3] = { 15.0f, 75.0f, 4.0f };
    const float weaponFov = 68.38f;
    const float ratio = std::tan(weaponFov * 0.5f * 3.14159265f / 180.0f)
                        / std::tan(91.31f * 0.5f * 3.14159265f / 180.0f);
    float weapon[3];
    Check(WeaponPassAngles(turned, clean, ratio, weapon), "the weapon camera resolves");
    CarryPose(eye, weapon, leaned, turned, origin, angles);

    const float parts[][3] = { { 14.0f, 6.0f, -5.0f }, { 22.0f, 4.0f, -3.0f }, { 8.0f, 9.0f, -9.0f } };
    for (const auto& part : parts) {
        float before[3], after[3];
        PointOnPose(gunOrigin, gunAngles, part[0], part[1], part[2], before);
        PointOnPose(origin, angles, part[0], part[1], part[2], after);
        float bx, by, ax, ay;
        PointNdc(before, eye, weapon, weaponFov, bx, by);
        PointNdc(after, leaned, turned, weaponFov, ax, ay);
        const bool same = NearEqual(bx, ax, 1e-4f) && NearEqual(by, ay, 1e-4f);
        if (!same) {
            std::printf("    weapon camera (%.5f, %.5f) carried (%.5f, %.5f)\n", bx, by, ax, ay);
        }
        Check(same, "a point on the gun lands where the weapon camera would draw it");
    }
}

}  // namespace

int RunSourceMathTests() {
    std::printf("\nSource math\n===========\n");
    TestScaleFovByWidthRatio();
    TestAngleVectors();
    TestBasisToAngles();
    TestApplyCameraLocalRotation();
    TestCameraLocalRotationIsContinuousAtThePole();
    TestFovScaleRoundTrip();
    TestBoundProjectedPixel();
    TestWeaponPassAngles();
    TestCarryPose();
    return g_failures;
}
