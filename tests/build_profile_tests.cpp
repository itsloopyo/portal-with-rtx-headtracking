// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
// Tests for the build-profile failsafes (src/builds/build_profile.h) and the
// shipped Steam profile (src/builds/steam_offsets.cpp).
//
// Everything here guards a DORMANCY decision. A profile that reports itself
// complete when it is not gets a detour installed against a stale RVA, and a
// trace offset that does not fit the buffer it indexes is read from past the
// end of a stack array. Neither has an in-game symptom short of a crash, and
// neither is visible in a diff of the offsets themselves.

#include <cstdio>

#include "builds/build_registry.h"
#include "test_check.h"

namespace {

using namespace headtracking::builds;

void TestShippedSteamProfiles() {
    std::printf("shipped Steam profiles\n");

    // Iterated out of the registry the mod actually routes on, not a mirror of
    // it: a second list is one more thing to keep in step, and the profile
    // someone forgets to copy across is exactly the freshly-appended one these
    // checks exist for.
    size_t count = 0;
    const BuildProfile* const* profiles = KnownProfiles(count);
    // An empty registry satisfies every loop below without executing one, so the
    // suite would go green on a mod that can never engage on any build.
    Check(count > 0, "the registry ships at least one profile");

    for (size_t i = 0; i < count; ++i) {
        const BuildProfile* p = profiles[i];
        std::printf("  %s\n", p->name);
        Check(p->IsComplete(), "carries a RenderView RVA");
        Check(p->HasAimOffsets(), "carries the aim addresses");
        Check(p->HasEngineState(), "carries the gameplay gate");
        Check(p->HasFovConVars(), "carries the FOV cvars");
        Check(p->HasCentredCrosshairElements(), "carries the centred-element addresses");
        Check(TraceFieldsFitBuffer(p->offsets.aim),
              "its trace_t offsets are read inside the trace buffer");
    }
}

// Routing between profiles is decided purely by fingerprint. Two profiles
// sharing one would not fail to build, and would fail no check above -
// MatchProfile walks kKnownProfiles in order and returns the first hit, so the
// shadowed build would silently be hooked with the other's RVAs. Distinctness is
// the whole basis of the routing, and this only gets easier to break as patches
// append entries.
void TestProfileFingerprintsAreDistinct() {
    std::printf("fingerprint distinctness\n");

    size_t count = 0;
    const BuildProfile* const* profiles = KnownProfiles(count);
    for (size_t i = 0; i < count; ++i) {
        for (size_t j = i + 1; j < count; ++j) {
            Check(!profiles[i]->fingerprint.Matches(profiles[j]->fingerprint), profiles[j]->name);
        }
    }
}

// The dormancy failsafe driven through the real registry rather than asserted
// about. A fingerprint no profile carries must publish nothing: ActiveProfile()
// is where the crosshair, aim and game-state hooks read their RVAs from, so a
// profile still reachable there after a failed match is a detour installed
// against a build the mod never matched.
void TestRegistryRoutesOnFingerprint() {
    std::printf("registry routing\n");

    size_t count = 0;
    const BuildProfile* const* profiles = KnownProfiles(count);
    Check(count > 0, "the registry is not empty");
    // Check() records and returns, it does not abort, so an empty registry would
    // otherwise walk off the end of the array on the next line.
    if (count == 0) return;

    // Runs before any successful match, so ActiveProfile() has never been set.
    // Asserted first for that reason: once a match publishes a profile there is
    // no way to unpublish it, and the check would then be measuring test order
    // rather than the code.
    const cameraunlock::memory::PeFingerprint alien{0xDEADBEEF, 0x00010000, 0x12345678};
    Check(MatchProfile(alien) == nullptr, "an unmatched fingerprint resolves no profile");
    Check(ActiveProfile() == nullptr, "and publishes no active profile");

    Check(MatchProfile(profiles[0]->fingerprint) == profiles[0], "a known fingerprint matches");
    Check(ActiveProfile() == profiles[0], "and is published for the later hooks");
}

void TestIncompleteProfileStaysDormant() {
    std::printf("dormancy failsafes\n");

    // A placeholder: the fingerprint of a build that has been spotted but whose
    // hook target has not been rederived. Landing one must not arm the detour.
    BuildProfile placeholder = kSteamProfile_20250518;
    placeholder.offsets.render_view_rva = 0;
    Check(!placeholder.IsComplete(), "a zero RenderView RVA reports incomplete");

    BuildProfile noAim = kSteamProfile_20250518;
    noAim.offsets.aim.trace_line_rva = 0;
    Check(!noAim.HasAimOffsets(), "a missing aim address disables the crosshair correction");

    BuildProfile noGate = kSteamProfile_20250518;
    noGate.offsets.engine.engine_ptr_rva = 0;
    Check(!noGate.HasEngineState(), "a missing engine pointer disables the gameplay gate");

    // Slot 0 is the interface's destructor on this branch, so a profile landed
    // with the pointer located but a slot left at zero calls it six times a
    // frame and reads the result as the gameplay gate.
    BuildProfile noSlot = kSteamProfile_20250518;
    noSlot.offsets.engine.slot_get_level_name = 0;
    Check(!noSlot.HasEngineState(), "a zero vtable slot disables the gameplay gate");

    BuildProfile noFov = kSteamProfile_20250518;
    noFov.offsets.fov.fov_desired_rva = 0;
    Check(!noFov.HasFovConVars(), "a missing cvar address disables the FOV override");

    BuildProfile noDrawSelf = kSteamProfile_20250518;
    noDrawSelf.offsets.aim.hud_texture_draw_self_rva = 0;
    Check(!noDrawSelf.HasCentredCrosshairElements(),
          "a missing DrawSelf address disables the centred-element correction");

    BuildProfile noQuickInfo = kSteamProfile_20250518;
    noQuickInfo.offsets.aim.quick_info_paint_rva = 0;
    Check(!noQuickInfo.HasCentredCrosshairElements(),
          "a missing CHUDQuickInfo address disables the centred-element correction");
}

void TestTraceOffsetsAreBoundsChecked() {
    std::printf("trace_t offset bounds\n");

    // endpos is three floats and fraction is one, so the last offset that fits
    // is the buffer size minus that field's own width.
    Check(TraceFieldFits(kTraceResultBufferSize - 12u, 12u), "the last in-range endpos fits");
    Check(!TraceFieldFits(kTraceResultBufferSize - 11u, 12u),
          "an endpos one byte past the end is rejected");
    Check(TraceFieldFits(kTraceResultBufferSize - 4u, 4u), "the last in-range fraction fits");
    Check(!TraceFieldFits(kTraceResultBufferSize, 4u),
          "an offset at the end of the buffer is rejected");

    // The check is a subtraction, not an addition, so an offset near the top of
    // the range cannot wrap past it and read as in-bounds.
    Check(!TraceFieldFits(0xFFFFFFF8u, 12u), "a wrap-around offset is rejected");

    BuildProfile overrun = kSteamProfile_20250518;
    overrun.offsets.aim.trace_endpos = kTraceResultBufferSize - 4u;
    Check(!TraceFieldsFitBuffer(overrun.offsets.aim),
          "a profile whose endpos runs past the buffer is refused");

    BuildProfile fractionOverrun = kSteamProfile_20250518;
    fractionOverrun.offsets.aim.trace_fraction = kTraceResultBufferSize + 4u;
    Check(!TraceFieldsFitBuffer(fractionOverrun.offsets.aim),
          "a profile whose fraction sits past the buffer is refused");
}

}  // namespace

int RunBuildProfileTests() {
    std::printf("\nBuild profiles\n==============\n");
    TestShippedSteamProfiles();
    TestProfileFingerprintsAreDistinct();
    TestRegistryRoutesOnFingerprint();
    TestIncompleteProfileStaysDormant();
    TestTraceOffsetsAreBoundsChecked();
    return g_failures;
}
