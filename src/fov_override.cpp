// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
//
// CViewSetup::fov is the horizontal FOV, in degrees, the frame is actually
// rendered with, and it is the mod's answer to "what is the camera's FOV".
// CViewRender::SetUpView has already taken the player's fov_desired - which
// Source defines against a 4:3 screen - and widened it for the real viewport by
// the time RenderView sees the struct, and it has already applied whatever the
// game itself is doing to the FOV that frame (a zoomed weapon, a scripted
// sequence, the suit zoom). So this one float is the live value, not a setting.
//
// It is also the right place to CHANGE it. Everything the frame is built from
// comes out of this struct, including the world-to-screen matrix the crosshair
// projection goes through, so an override written here needs nothing kept in
// sync with it: widen the FOV and the reticle lands on the same world point it
// did before, because both come from the same matrix (see crosshair_hook.cpp).
//
// The game's own knobs are the fov_desired and viewmodel_fov cvars. Read out of
// this client.dll's ConVar registrations: fov_desired is default 75, min 75,
// max 120, flags 0x280 (FCVAR_ARCHIVE | FCVAR_USERINFO), and viewmodel_fov is
// default 54, flags 0x4000 (FCVAR_CHEAT). So the world FOV is adjustable in
// game but only from 75 to 120, and it rides along in the player's userinfo;
// the viewmodel FOV is not adjustable at all without sv_cheats. A write into
// the render view has none of those three properties, which is what this
// override buys.
//
// The override is a RATIO against those cvars, not a value written over the top
// of whatever the frame happens to hold. CViewSetup::fov is not always the
// player's FOV: a suit zoom, a weapon zoom and a scripted camera each write
// their own, and the trainstation opening alone sweeps it from 6 degrees up to
// the player's. Writing the configured number in unconditionally would flatten
// every one of them, so the zoom key would visibly do nothing. Scaling by
// (configured / cvar) instead shows exactly the configured number on a frame at
// the player's own FOV, scales a zoom by the same factor as everything else,
// and is continuous through the transition, so there is no frame where the
// override snaps in or out.
//
// The scaling happens in the cvars' own 4:3 reference rather than on the
// rendered value: the widening is a tangent scaling, so a ratio applied to the
// widened number is not the ratio the player asked for.
//
// The same struct answers the second FOV question the mod has, which is what
// the HEAD POSE has to be scaled by. A narrow field of view magnifies the whole
// frame, head tracking with it, so the same head angle sweeps further across
// the screen the moment the game zooms and the player reads that as the mod's
// sensitivity jumping. The correction is the ratio between the FOV being
// rendered and the one an un-zoomed frame would be - see ZoomFactor below, and
// tracker_axes.h for what is done with it.
//
// In this game that factor is 1.0 on every frame of ordinary play. No shipped
// map sets an FOV: the entity lumps of all 27 BSPs contain no fov keyvalue, no
// env_zoom, and none of the four point_viewcontrols has an fov key. The
// portalgun has no sights and no zoom. What CAN move it is a console `fov` with
// sv_cheats on, a future map, and the [View] Fov key here - and the last of
// those is why the factor is measured against the OVERRIDDEN un-zoomed FOV
// rather than the player's fov_desired. A player who sets Fov=110 has chosen a
// wider ordinary view, not a permanent 0.54x on their head tracking.

#include "fov_override.h"

#include <Windows.h>

#include <cmath>
#include <cstdint>
#include <cstring>

#include "angles.h"
#include "cameraunlock/camera/zoom_compensation.h"
#include "cameraunlock/memory/pe_fingerprint.h"
#include "cameraunlock/memory/safe_memory.h"
#include "debug_log.h"
#include "source_math.h"

namespace headtracking {

namespace {

// At and past this the projection degenerates - tan(fov/2) runs away - so a
// frame that scales into it is left as the game rendered it.
constexpr float kMaxRenderableFov = 179.0f;

// The live values of the two cvars, read every frame rather than latched: the
// player can change fov_desired from the console mid-session, and the override
// is defined relative to it.
const float* g_fovDesired = nullptr;
const float* g_viewmodelFov = nullptr;

// True when both ConVar fields land inside the loaded image. The RVAs are
// hand-derived per build profile, and the PE fingerprint proves the binary, not
// the offsets - so a mistyped one passes the gate and then hands strcmp an
// arbitrary 32-bit value read out of nowhere, faulting during Install and
// killing the game before it reaches the menu. The trace offsets are already
// bounded for exactly this reason (TraceFieldsFitBuffer); this is the same
// discipline on the same class of hand-entered number.
bool ConVarFieldsFitImage(HMODULE client, const builds::FovConVarOffsets& off, uint32_t rva) {
    cameraunlock::memory::PeFingerprint fp{};
    if (!cameraunlock::memory::ReadPeFingerprint(client, fp)) return false;
    const uint32_t image = fp.SizeOfImage;
    const uint32_t widest = (off.convar_name > off.convar_value) ? off.convar_name
                                                                 : off.convar_value;
    // Subtraction rather than addition so an RVA near the top cannot wrap past
    // the check.
    if (rva >= image) return false;
    const uint32_t room = image - rva;
    return widest < room && sizeof(void*) <= room - widest;
}

// A ConVar object whose name reads back as the expected string proves the rest
// of the layout fits. Three plausible-looking floats prove nothing, which is
// exactly the failure this check exists to catch.
const float* ResolveConVar(HMODULE client, const builds::FovConVarOffsets& off, uint32_t rva,
                           const char* expectedName) {
    if (!ConVarFieldsFitImage(client, off, rva)) {
        HT_LOG("[view] the %s ConVar offsets in this build profile do not fit client.dll "
               "(rva 0x%X) - the [View] Fov keys are inert this session; head tracking is "
               "unaffected", expectedName, rva);
        return nullptr;
    }

    const uintptr_t object = reinterpret_cast<uintptr_t>(client) + rva;
    const char* name = nullptr;
    if (!cameraunlock::memory::SafeRead(object + off.convar_name, name)) {
        HT_LOG("[view] client.dll+0x%X is not readable as a %s ConVar - the [View] Fov keys "
               "are inert this session; head tracking is unaffected", rva, expectedName);
        return nullptr;
    }
    // The name pointer came out of the object, so it is only as trustworthy as
    // the offset that produced it. Copied through the guarded reader before it
    // reaches strcmp, one byte at a time, so a bad pointer is a false rather
    // than a fault.
    char probe[32] = {};
    if (name) {
        for (size_t i = 0; i < sizeof(probe) - 1; ++i) {
            uint8_t ch = 0;
            if (!cameraunlock::memory::SafeReadU8(reinterpret_cast<uintptr_t>(name) + i, ch)) {
                name = nullptr;
                break;
            }
            probe[i] = static_cast<char>(ch);
            if (ch == 0) break;
        }
    }
    if (name) name = probe;

    if (!name || std::strcmp(name, expectedName) != 0) {
        HT_LOG("[view] client.dll+0x%X does not read as the %s ConVar (its name is '%s') - the "
               "[View] Fov keys are inert this session; head tracking is unaffected",
               rva, expectedName, name ? name : "(null)");
        return nullptr;
    }
    return reinterpret_cast<const float*>(object + off.convar_value);
}

// Per-field diagnostic state. Owned by the caller so the two FOV fields report
// independently: `logged_factor` fires once per factor - when the player edits
// the cvar, not on every frame of a zoom - and `refusal_logged` once per field.
// One shared latch let a refusal on the world FOV silence the viewmodel one for
// the whole session.
struct FieldLog {
    float logged_factor = 0.0f;
    bool refusal_logged = false;
};

// The rendered viewport, with the width ratio the engine widens fov_desired by
// derived from it rather than passed alongside it - the two cannot disagree.
struct Viewport {
    Viewport(int width, int height)
        : w(width),
          h(height),
          ratio((static_cast<float>(width) / static_cast<float>(height))
                * source::kReferenceAspectInverse) {}

    int w;
    int h;
    float ratio;
};

// Scales one of the view's FOV fields. True when the field was actually
// rewritten, which is what tells the caller whether an un-zoomed frame now
// renders at the configured FOV or still at the game's own.
bool ScaleField(float& target, float desired, float base, const Viewport& viewport,
                const char* what, FieldLog& log) {
    if (desired <= 0.0f) return false;
    // The game renders a zero FOV on the frames where the field is not in
    // use - the viewmodel one, during a scripted camera. There is nothing
    // to scale, and a ratio against a zero cvar has no meaning either.
    if (!(base > 0.0f) || !std::isfinite(target) || target <= 0.0f) return false;

    const float factor = desired / base;
    const float rendered = source::ScaleFovByWidthRatio(
        source::UnscaleFovByWidthRatio(target, viewport.ratio) * factor, viewport.ratio);
    if (!std::isfinite(rendered) || rendered <= 0.0f || rendered >= kMaxRenderableFov) {
        // This frame only. The game is rendering something wide enough that
        // the player's factor takes it past having a projection at all - a
        // cinematic, not a broken offset - so the honest answer is the frame
        // the game built, not an override disabled for the rest of the run.
        if (!log.refusal_logged) {
            log.refusal_logged = true;
            HT_LOG("[view] %s FOV left alone on a frame the game rendered at %.2f: scaling "
                   "it by %.3f gives %.2f degrees, which has no projection",
                   what, target, factor, rendered);
        }
        return false;
    }
    if (log.logged_factor != factor) {
        log.logged_factor = factor;
        HT_LOG("[view] %s FOV override: %.1f over the game's %.1f = x%.3f, so a normal "
               "frame renders at %.2f in a %dx%d viewport", what, desired, base, factor,
               source::ScaleFovByWidthRatio(desired, viewport.ratio), viewport.w, viewport.h);
    }
    target = rendered;
    return true;
}

// ----- Zoom compensation ----------------------------------------------------

// A frame at the un-zoomed FOV. The pose factor is 1.0 there, which is the
// whole of ordinary play in this game.
constexpr float kNoZoomScaling = 1.0f;

// How far the factor has to move before it is worth another line. Wide enough
// that the float noise between the engine's widening and ours is not a zoom -
// the two compute the same tangent scaling from the same fov_desired with the
// same operations in a different order, so they agree to about a part in a
// million rather than exactly - and narrow enough that a zoom is reported while
// it is still sweeping.
constexpr float kZoomLogBand = 0.02f;

// Zero, not 1.0, so the opening frame always logs: that line is the gate, and a
// factor that starts at exactly 1.0 is the case it most needs to prove.
struct ZoomLog {
    float logged_factor = 0.0f;
};

float TanHalf(float fovDegrees) { return std::tan(fovDegrees * 0.5f * kDegToRad); }

// Every term of the factor on one line, because a factor that is wrong by a
// constant reads exactly like a factor that is right. The units are on it for
// the same reason: the way this goes wrong is a live FOV and a base measured on
// different axes, which leaves normal play running at a fixed fraction of the
// pose with no symptom but head tracking feeling weak everywhere.
//
// Written off the camera rather than off the pose, so the basis is visible with
// no tracker connected and without loading a save, and again whenever the FOV
// being rendered moves away from what was last reported.
//
// THE GATE IS THAT THE FIRST LINE READS x1.0000.
void LogZoomBasis(const Viewport& viewport, float unzoomed4x3, float base, float live,
                  float factor, ZoomLog& log) {
    if (std::fabs(factor - log.logged_factor) <= kZoomLogBand) return;
    log.logged_factor = factor;
    HT_LOG("[view] head pose zoom factor x%.4f: rendering at %.2f horizontal degrees against "
           "an un-zoomed %.2f (%.1f at 4:3, widened by x%.4f for %dx%d) - %s",
           factor, live, base, unzoomed4x3, viewport.ratio, viewport.w, viewport.h,
           std::fabs(factor - kNoZoomScaling) > kZoomLogBand
               ? "the frame is not at the un-zoomed FOV, so yaw, pitch and lean are scaled "
                 "to displace the picture by as much as they would have been"
               : "the head pose is applied as it arrived");
}

// The factor the head pose scales by, from the FOV the frame is rendering at
// and the one an un-zoomed frame would.
//
// Both are CViewSetup::fov's own units - horizontal degrees, already widened for
// this viewport - because the base is built by putting the 4:3 reference through
// the same widening the engine applied to the live one. Same axis on both sides
// is the whole requirement: the widening is a tangent scaling, so it cancels out
// of the ratio, and the factor is the same number whether it is measured
// horizontally or vertically. Measuring the two sides on DIFFERENT axes does not
// cancel, and is a constant multiplier on the pose that nothing else would show.
float ZoomFactor(const Viewport& viewport, float unzoomed4x3, float live, ZoomLog& log) {
    if (!(unzoomed4x3 > 0.0f)) return kNoZoomScaling;
    if (!std::isfinite(live) || live <= 0.0f || live >= kMaxRenderableFov) {
        return kNoZoomScaling;
    }
    const float base = source::ScaleFovByWidthRatio(unzoomed4x3, viewport.ratio);
    if (!std::isfinite(base) || base <= 0.0f || base >= kMaxRenderableFov) {
        return kNoZoomScaling;
    }

    const float tanLive = TanHalf(live);
    const float tanBase = TanHalf(base);
    if (!(tanLive > 0.0f) || !(tanBase > 0.0f)) return kNoZoomScaling;

    const float factor = cameraunlock::camera::FovZoomFactor(tanLive, tanBase);
    if (!std::isfinite(factor) || factor <= 0.0f) return kNoZoomScaling;

    LogZoomBasis(viewport, unzoomed4x3, base, live, factor, log);
    return factor;
}

}  // namespace

void ResolveFovConVars(HMODULE client, const builds::BuildProfile& profile) {
    if (!profile.HasFovConVars()) {
        HT_LOG("[view] build profile has no FOV cvar addresses - the [View] Fov keys are inert "
               "(head tracking is unaffected)");
        return;
    }
    const builds::FovConVarOffsets& off = profile.offsets.fov;
    const float* world = ResolveConVar(client, off, off.fov_desired_rva, "fov_desired");
    const float* viewmodel = ResolveConVar(client, off, off.viewmodel_fov_rva, "viewmodel_fov");
    if (!world || !viewmodel) return;

    g_fovDesired = world;
    g_viewmodelFov = viewmodel;
    HT_LOG("[view] FOV cvars resolved: fov_desired=%.1f viewmodel_fov=%.1f", *world, *viewmodel);
}

// A viewport that does not read as a rect disables both jobs for the rest of the
// session rather than being retried every frame: it means the profile's
// CViewSetup offsets do not fit this client.dll, and a projection built from the
// NaN that follows is a black screen, not a cosmetic fault.
float PrepareFrameFov(const ViewSetup& view, float worldFov, float viewmodelFov) {
    static bool s_disabled = false;
    if (s_disabled) return kNoZoomScaling;
    // Without fov_desired there is nothing to call un-zoomed, so there is no
    // override to apply and no factor to measure. ResolveFovConVars has already
    // said why.
    if (!g_fovDesired) return kNoZoomScaling;

    const int w = view.RectWidth();
    const int h = view.RectHeight();
    if (w <= 0 || h <= 0) {
        s_disabled = true;
        HT_LOG("[view] FOV override and zoom compensation disabled: viewport reads as %dx%d - "
               "the build profile's CViewSetup offsets do not fit this client.dll", w, h);
        return kNoZoomScaling;
    }
    const Viewport viewport(w, h);

    static FieldLog s_world;
    static FieldLog s_viewmodel;
    const bool overridden = ScaleField(view.Fov(), worldFov, *g_fovDesired, viewport, "world",
                                       s_world);
    ScaleField(view.FovViewmodel(), viewmodelFov, *g_viewmodelFov, viewport, "viewmodel",
               s_viewmodel);

    // What an un-zoomed frame renders at, which is the configured FOV exactly
    // when this frame went through the override and the player's own otherwise.
    // Reading it off the same call keeps the two in step through the frames the
    // override refuses.
    static ZoomLog s_zoom;
    return ZoomFactor(viewport, overridden ? worldFov : *g_fovDesired, view.Fov(), s_zoom);
}

}  // namespace headtracking
