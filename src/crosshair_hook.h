// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#pragma once

namespace headtracking {

// Puts the crosshair back on the point the player is actually shooting at.
//
// Shots already fly along the clean mouse aim - the render-view hook never
// touches the player's own eye angles - but the crosshair is drawn from
// CurrentViewAngles(), which RenderView seeds from the CViewSetup we mutate.
// So the vanilla crosshair follows the head and stops marking the shot the
// moment the two cameras differ.
//
// Portal draws the crosshair cluster through three elements, so this installs
// two kinds of correction:
//
//   - CHudCrosshair::GetDrawPosition, the one function every element built on
//     Valve's shared crosshair code asks for its screen position. The detour
//     runs the original, then overwrites the answer with the aim point
//     aim_point.cpp resolves.
//
//   - The two elements that ask nothing. CHudPortalCrosshair carries its own
//     inlined copy of that maths and draws at hard screen centre, and
//     CHUDQuickInfo (the portal-gun brackets around the crosshair) is centred
//     outright. CHudPortalCrosshair's own offset-angle branch cannot be borrowed
//     - it projects a direction one unit from the eye rather than the impact
//     point, which is wrong under a lean, and it adds the vertical term without
//     flipping NDC-y, which is wrong outright. So the detour shifts what they
//     draw instead: it arms a pixel delta for the whole of each Paint and adds it
//     inside CHudTexture::DrawSelf, so every texture an element draws moves
//     together and the crosshair stays one object rather than splitting into a
//     moving reticle and a stationary pair of brackets.
//
// Both reuse the engine's own trace, projection and viewport scaling - the three
// calls GetDrawPosition itself makes - with the clean camera substituted for the
// rendered one. That is what keeps the reticle on the camera the frame was drawn
// with, the [View] Fov override included, since the world-to-screen matrix is
// built from the same CViewSetup the override is written into.
//
// Installed only on a build profile carrying the aim addresses; without them the
// game keeps its vanilla centred crosshair and head tracking is unaffected.
class CrosshairHook {
public:
    CrosshairHook() = default;

    bool Install();
};

}  // namespace headtracking
