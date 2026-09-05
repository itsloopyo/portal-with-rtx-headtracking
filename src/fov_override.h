// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#pragma once

#include <Windows.h>

#include "builds/build_profile.h"
#include "view_setup.h"

namespace headtracking {

// The [View] Fov / FovViewmodel override, applied to the render view the frame
// is about to be built from.
//
// Separate from the head pose on purpose: it is a view SETTING, not a pose, so
// it neither needs tracker data nor has anything to do with the tracker->Source
// axis mapping the camera hook owns. The only thing the two share is the
// CViewSetup they both write.

// Reads the running client.dll's fov_desired and viewmodel_fov ConVars, which
// the override is expressed relative to. Called once, after the build profile
// has matched. A profile without the ConVars, or a pair that does not read
// back, leaves the override off and the rest of the mod running.
void ResolveFovConVars(HMODULE client, const builds::BuildProfile& profile);

// Scales the render view's two FOVs by the ratio the config asks for (a zero
// override for a field leaves that field as the game rendered it), then reports
// the factor the head pose has to be scaled by to displace the picture as far
// as it would have at the un-zoomed FOV.
//
// The two answers come from one call because they come from one number. The
// pose factor is measured against what an UN-ZOOMED frame renders at, and after
// the override has run that is the configured FOV rather than the player's
// fov_desired - so a second entry point would have to re-derive whether the
// override applied to this frame, and could disagree.
//
// 1.0 is both "the frame is at the un-zoomed FOV" and "there is no live FOV to
// measure against", which are the same instruction to the caller: apply the
// pose as it arrived. A guessed base would be a silent constant multiplier on
// all of normal play, so the unreadable case says so in the log and scales
// nothing.
float PrepareFrameFov(const ViewSetup& view, float worldFov, float viewmodelFov);

}  // namespace headtracking
