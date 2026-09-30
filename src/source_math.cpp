// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#include "source_math.h"

#include <cmath>

#include "angles.h"

namespace headtracking::source {

float ScaleFovByWidthRatio(float fovDegrees, float ratio) {
    const float halfRad = fovDegrees * 0.5f * kDegToRad;
    return 2.0f * std::atan(std::tan(halfRad) * ratio) * kRadToDeg;
}

float UnscaleFovByWidthRatio(float fovDegrees, float ratio) {
    return ScaleFovByWidthRatio(fovDegrees, 1.0f / ratio);
}

void AngleVectors(const float* ang, float fwd[3], float right[3], float up[3]) {
    const float p = ang[0] * kDegToRad;
    const float y = ang[1] * kDegToRad;
    const float r = ang[2] * kDegToRad;
    const float sp = std::sin(p), cp = std::cos(p);
    const float sy = std::sin(y), cy = std::cos(y);
    const float sr = std::sin(r), cr = std::cos(r);
    fwd[0]   = cp * cy;            fwd[1]   = cp * sy;            fwd[2]   = -sp;
    right[0] = -sr * sp * cy + cr * sy;
    right[1] = -sr * sp * sy - cr * cy;
    right[2] = -sr * cp;
    up[0]    = cr * sp * cy + sr * sy;
    up[1]    = cr * sp * sy - sr * cy;
    up[2]    = cr * cp;
}

float ForwardXYDistance(const float* fwd) {
    return std::sqrt(fwd[0] * fwd[0] + fwd[1] * fwd[1]);
}

void BasisToAngles(const float* fwd, const float* left, const float* up, float* ang) {
    const float xyDist = ForwardXYDistance(fwd);
    ang[0] = std::atan2(-fwd[2], xyDist) * kRadToDeg;
    if (xyDist > kGimbalEpsilon) {
        ang[1] = std::atan2(fwd[1], fwd[0]) * kRadToDeg;
        ang[2] = std::atan2(left[2], up[2]) * kRadToDeg;
    } else {
        // Looking straight up or down: Source folds the whole rotation into
        // yaw and zeroes roll.
        ang[1] = std::atan2(-left[0], left[1]) * kRadToDeg;
        ang[2] = 0.0f;
    }
}

float BoundProjectedPixel(float pixel, int extent) {
    const float span = static_cast<float>(extent);
    const float margin = kProjectedPixelBoundViewports * span;
    if (pixel < -margin) return -margin;
    if (pixel > span + margin) return span + margin;
    return pixel;
}

void ApplyCameraLocalRotation(float* ang, float dpitch, float dyaw, float droll) {
    float fwd[3], right[3], up[3];
    AngleVectors(ang, fwd, right, up);

    const float head[3] = { dpitch, dyaw, droll };
    float hf[3], hr[3], hu[3];
    AngleVectors(head, hf, hr, hu);

    // AngleVectors works in an (x = forward, y = left, z = up) frame, so the
    // head vectors' components are already coordinates in the camera's own
    // frame - mapping them back out is one change of basis.
    const float camLeft[3] = { -right[0], -right[1], -right[2] };
    float outFwd[3], outUp[3], outLeft[3];
    for (int i = 0; i < 3; ++i) {
        outFwd[i]  = hf[0] * fwd[i] + hf[1] * camLeft[i] + hf[2] * up[i];
        outUp[i]   = hu[0] * fwd[i] + hu[1] * camLeft[i] + hu[2] * up[i];
        // BasisToAngles wants the left column, which is the negated right one.
        outLeft[i] = -(hr[0] * fwd[i] + hr[1] * camLeft[i] + hr[2] * up[i]);
    }
    BasisToAngles(outFwd, outLeft, outUp, ang);
}

namespace {

float Dot3(const float* a, const float* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

void Cross3(const float* a, const float* b, float* out) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

void Normalize3(float* v) {
    const float len = std::sqrt(Dot3(v, v));
    v[0] /= len;
    v[1] /= len;
    v[2] /= len;
}

}  // namespace

bool WeaponPassAngles(const float* renderAngles, const float* cleanAngles, float lateralRatio,
                      float* out) {
    float fwd[3], right[3], up[3];
    AngleVectors(renderAngles, fwd, right, up);
    const float left[3] = { -right[0], -right[1], -right[2] };
    const float* axes[3] = { fwd, left, up };

    float aim[3], unusedRight[3], unusedUp[3];
    AngleVectors(cleanAngles, aim, unusedRight, unusedUp);

    // The clean axis in the drawn camera's own (forward, left, up) frame.
    float drawn[3] = { Dot3(aim, fwd), Dot3(aim, left), Dot3(aim, up) };
    if (!(drawn[0] > 0.0f)) return false;
    float wanted[3] = { drawn[0], drawn[1] * lateralRatio, drawn[2] * lateralRatio };
    Normalize3(drawn);
    Normalize3(wanted);

    // The new camera frame must see the axis at `wanted`, so its axes are the
    // old ones turned by the rotation carrying `wanted` onto `drawn`. Rodrigues
    // with the unnormalised cross product, which is stable here because both
    // vectors are in front of the camera and the arc is under a quarter turn.
    float k[3];
    Cross3(wanted, drawn, k);
    const float c = Dot3(wanted, drawn);
    const float blend = 1.0f / (1.0f + c);

    float outAxes[3][3];
    for (int col = 0; col < 3; ++col) {
        const float e[3] = { col == 0 ? 1.0f : 0.0f, col == 1 ? 1.0f : 0.0f,
                             col == 2 ? 1.0f : 0.0f };
        float kxe[3];
        Cross3(k, e, kxe);
        const float kdote = Dot3(k, e);
        float local[3];
        for (int i = 0; i < 3; ++i) local[i] = e[i] * c + kxe[i] + k[i] * kdote * blend;
        for (int i = 0; i < 3; ++i) {
            outAxes[col][i] = local[0] * axes[0][i] + local[1] * axes[1][i] + local[2] * axes[2][i];
        }
    }
    BasisToAngles(outAxes[0], outAxes[1], outAxes[2], out);
    return true;
}

void CarryPose(const float* fromOrigin, const float* fromAngles, const float* toOrigin,
               const float* toAngles, float* origin, float* angles) {
    float fromFwd[3], fromRight[3], fromUp[3], toFwd[3], toRight[3], toUp[3];
    AngleVectors(fromAngles, fromFwd, fromRight, fromUp);
    AngleVectors(toAngles, toFwd, toRight, toUp);

    // A direction keeps its components along the camera's forward, right and
    // up; a point does the same with its offset from the camera.
    const auto carry = [&](const float* v, float* out) {
        const float f = Dot3(v, fromFwd), r = Dot3(v, fromRight), u = Dot3(v, fromUp);
        for (int i = 0; i < 3; ++i) out[i] = toFwd[i] * f + toRight[i] * r + toUp[i] * u;
    };

    const float offset[3] = { origin[0] - fromOrigin[0], origin[1] - fromOrigin[1],
                              origin[2] - fromOrigin[2] };
    float moved[3];
    carry(offset, moved);
    for (int i = 0; i < 3; ++i) origin[i] = toOrigin[i] + moved[i];

    float fwd[3], right[3], up[3];
    AngleVectors(angles, fwd, right, up);
    const float left[3] = { -right[0], -right[1], -right[2] };
    float outFwd[3], outLeft[3], outUp[3];
    carry(fwd, outFwd);
    carry(left, outLeft);
    carry(up, outUp);
    BasisToAngles(outFwd, outLeft, outUp, angles);
}

}  // namespace headtracking::source
