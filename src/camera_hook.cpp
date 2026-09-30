// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
// Render-view injection for Portal with RTX (Source Engine, client.dll).
//
// Hook target: CViewRender::RenderView(CViewSetup* view, int clearFlags,
// int whatToDraw)  [__thiscall].  Discovered via MSVC RTTI: the CViewRender
// vftable, slot identified by the telemetry marker naming
// "CViewRender::RenderView". The arity is not a guess: the
// function ends in `ret 0xC`, so it pops exactly three stack arguments, and a
// detour declaring a fourth would leave the render thread's stack four bytes
// out on every frame.
//
// RenderView runs in the render phase, after the game has already produced the
// frame's CUserCmd / view angles (which drive aim, traces and weapon fire).
// We mutate the CViewSetup the renderer is about to consume - its angles,
// origin and FOV only - so the player sees the head-tracked view while the
// game's own camera (the player's eye angles) is untouched. Look and aim stay
// decoupled for free.
//
// Engagement is gated on a PE-fingerprint build-profile registry (append-only;
// see the "Maintain compatibility across new patches" doctrine and
// builds/build_registry.h). On any client.dll the registry does not recognise,
// the hook is never installed and the game runs vanilla.

#include "camera_hook.h"

#include <Windows.h>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "aim_state.h"
#include "angles.h"
#include "builds/build_registry.h"
#include "cameraunlock/hooks/hook_manager.h"
#include "cameraunlock/memory/pe_fingerprint.h"
#include "config.h"
#include "debug_log.h"
#include "detour.h"
#include "fov_override.h"
#include "game_state.h"
#include "log_throttle.h"
#include "plugin.h"
#include "source_math.h"
#include "tracker_axes.h"
#include "view_setup.h"

namespace headtracking {

namespace {

// ----- Resolved-at-load state ----------------------------------------------
const builds::BuildProfile* g_profile = nullptr;

using RenderViewFn = void(__fastcall*)(void* ecx, void* edx, void* view, int clearFlags,
                                       int whatToDraw);
RenderViewFn g_originalRenderView = nullptr;

// void C_BaseViewModel::CalcViewModelView(C_BasePlayer* owner,
//     const Vector& eyePosition, const QAngle& eyeAngles)  [__thiscall]
using CalcViewModelViewFn = void(__fastcall*)(void* viewModel, void* edx, void* owner,
                                              const float* eyePosition, const float* eyeAngles);
CalcViewModelViewFn g_originalCalcViewModelView = nullptr;

// C_BaseViewModel* C_BasePlayer::GetViewModel(int index, bool observerOk)  [__thiscall]
using GetViewModelFn = void*(__fastcall*)(void* player, void* edx, int index, bool observerOk);
// void C_BaseEntity::SetLocalOrigin(const Vector&) and SetLocalAngles(const QAngle&)
using SetLocalVectorFn = void(__fastcall*)(void* entity, void* edx, const float* value);
// void C_BaseAnimating::InvalidateBoneCache()
using InvalidateBoneCacheFn = void(__fastcall*)(void* entity, void* edx);
// C_BasePlayer* C_BasePlayer::GetLocalPlayer()
using LocalPlayerFn = void*(__cdecl*)();

GetViewModelFn        g_getViewModel = nullptr;
SetLocalVectorFn      g_setLocalOrigin = nullptr;
SetLocalVectorFn      g_setLocalAngles = nullptr;
InvalidateBoneCacheFn g_invalidateBoneCache = nullptr;
LocalPlayerFn         g_localPlayer = nullptr;

// What the pose pipeline contributed to this frame's view, carried to the
// diagnostic line so it can report the delta alongside the resulting camera.
struct TrackingDelta {
    bool applied = false;
    float pitch = 0.0f, yaw = 0.0f, roll = 0.0f;  // degrees, Source sign
    float x = 0.0f, y = 0.0f, z = 0.0f;           // Source units, camera basis
};

void Copy3(float* dst, const float* src) {
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

// ----- Diagnostics ----------------------------------------------------------

// Dense at first (the first frames, then ~every 200), because that is where
// install-time faults show; then one line per ~2000 frames for the rest of the
// session. See log_throttle.h for why the schedule has that shape.
constexpr int kBurstLines           = 6;     // opening frames logged unconditionally
constexpr int kEarlyLines           = 30;    // lines still treated as install-time
constexpr int kEarlyIntervalFrames  = 200;
constexpr int kSteadyIntervalFrames = 2000;

// Confirms the hook fires, the offsets resolve to a sane camera, and the
// head-tracking delta is being applied. Reads the CViewSetup after the delta
// has been written, so the line reports the view the frame will render with.
//
// Both FOVs are on it because that is the mod's read of the camera's field of
// view: a wrong CViewSetup::fov offset shows up here as a number that is not
// the player's fov_desired widened for their window, and there is nothing else
// in the log that would catch it.
void DiagnosticLog(const ViewSetup& view, const TrackingDelta& delta) {
    static LogThrottle s_throttle(kBurstLines, kEarlyLines, kEarlyIntervalFrames,
                                  kSteadyIntervalFrames);
    if (!s_throttle.ShouldLog()) return;

    const float* org = view.Origin();
    const float* ang = view.Angles();
    HT_LOG("[view] rect=%dx%d org=(%.1f,%.1f,%.1f) ang=(p%.2f y%.2f r%.2f) fov=%.2f/%.2f "
           "| track=%d delta=(p%.2f y%.2f r%.2f) pos=(%.2f,%.2f,%.2f)",
           view.RectWidth(), view.RectHeight(), org[0], org[1], org[2], ang[0], ang[1], ang[2],
           view.Fov(), view.FovViewmodel(), delta.applied ? 1 : 0,
           delta.pitch, delta.yaw, delta.roll, delta.x, delta.y, delta.z);
}

// ----- Pose injection -------------------------------------------------------

// Shifts the render origin in the CLEAN view basis - built from the angles
// before the head delta - so the lean follows the body rather than the
// head-rotated view. `delta` receives the applied offset in Source units for
// the diagnostic line. The basis and the axis signs are in tracker_axes.h.
void ApplyPositionalLean(const Plugin& plugin, const float* cleanAngles, float* org,
                         float zoomFactor, TrackingDelta& delta) {
    if (!plugin.GetPositionOffset(delta.x, delta.y, delta.z)) return;
    // A head offset seen at depth D lands at d / (2 * D * tan(fov/2)) of the
    // frame, so its displacement on screen is linear in the offset and the zoom
    // factor multiplies it directly. Applied after the lean envelope, which is
    // the clamp on how far the HEAD may move; this is how much of that the
    // rendered eye spends to put the picture in the same place.
    delta.x *= zoomFactor;
    delta.y *= zoomFactor;
    delta.z *= zoomFactor;
    ApplyHorizonLockedLean(cleanAngles, org, delta.x, delta.y, delta.z);
    delta.x *= kPosXSign;
    delta.y *= kPosYSign;
    delta.z *= kPosZSign;
}

// Composes the head rotation onto the render view's QAngle, in the yaw mode the
// player has selected. `delta` receives the applied rotation in Source degrees.
void ApplyRotationDelta(const Plugin& plugin, float yawRad, float pitchRad, float rollRad,
                        float zoomFactor, float* ang, TrackingDelta& delta) {
    // Yaw and pitch translate the picture across the frame, so both are scaled
    // for whatever FOV this frame is being rendered at. Roll rotates it about
    // the view axis by the same angle at every FOV there is, so roll is not.
    const float yawDeg   = ScaleRotationForZoom(yawRad   * kRadToDeg, zoomFactor);
    const float pitchDeg = ScaleRotationForZoom(pitchRad * kRadToDeg, zoomFactor);
    const float rollDeg  = rollRad  * kRadToDeg;

    delta.yaw  = yawDeg  * kYawSign;
    delta.roll = rollDeg * kRollSign;

    if (plugin.IsWorldSpaceYaw()) {
        // The pitch saturation lives inside the world-space composition only:
        // there the rendered pitch is exactly ang[0] + delta, which is the
        // quantity the clamp bounds. What comes back is the pitch actually
        // applied, so the diagnostic line reports the rotation the frame got.
        delta.pitch = ComposeWorldSpaceDelta(ang, yawDeg, pitchDeg, rollDeg);
    } else {
        delta.pitch = pitchDeg * kPitchSign;
        source::ApplyCameraLocalRotation(ang, delta.pitch, delta.yaw, delta.roll);
    }
}

// ----- The weapon -------------------------------------------------------------
//
// The weapon in the player's hands is drawn in a second pass that copies the
// render view and swaps only its FOV for fovViewmodel (54 widened to 68.38 at
// 16:9, against the world's 91.31). CalcViewModelView poses the weapon from the
// CLEAN eye, so under head tracking the angle between it and the drawn view is
// real, and the narrower projection magnifies it by tan(34.19) / tan(45.66) =
// 0.664: the weapon swings about 1.5 times as far across the frame as the world
// does and stops pointing at what it fires at.
//
// The weapon has to appear drawn from a camera of its own: the clean eye, at
// angles that put the clean aim axis where the world pass draws it
// (source::WeaponPassAngles). The pass itself is left on the render camera, as
// the stock game has it, and the viewmodel ENTITY is carried by the rigid move
// that takes that weapon camera onto the render camera. Drawn from the render
// camera, it then lands exactly where the weapon camera would have drawn it -
// and so does everything hung off it: its attachments, the particle effects
// placed from them, and the claw glows and barrel flare RTX Remix lights from
// the weapon, which Remix places from the render camera on the assumption that
// the weapon pass shares it. Giving the pass a camera of its own instead fixes
// the gun and leaves those glows behind, measured at about 70 px at a 15 degree
// head turn.
//
// The clean eye is also why a lean leaves the weapon where it is on screen: it
// hangs a third of a metre from the eye, a lean is most of that, and the stock
// game never moves it relative to the eye either. Roll is left as drawn - it
// turns the picture by the same angle at every FOV.
//
// The ratio comes from the render view's two FOV fields, which are exactly the
// numbers the engine builds both projections from: both horizontal degrees,
// both widened for the same viewport, so the aspect cancels and horizontal and
// vertical give the same ratio. The [View] overrides are already in them.
//
// Only a viewmodel CalcViewModelView posed since the last RenderView is moved.
// Its pose is then the clean one the move is defined against, and it is moved
// once: the next frame poses it afresh from the eye. The pointers recorded are
// only ever compared with the ones GetViewModel returns now, never followed, so
// one left behind by a level change cannot be written through.

constexpr int kMaxViewModels = 2;  // GetViewModel's index runs 0..MAX_VIEWMODELS-1

std::atomic<bool> g_viewModelCarryInstalled{false};
void* g_posedViewModels[kMaxViewModels] = {};
int   g_posedViewModelCount = 0;

constexpr int kWeaponBurstLines           = 3;
constexpr int kWeaponEarlyLines           = 15;
constexpr int kWeaponEarlyIntervalFrames  = 600;
constexpr int kWeaponSteadyIntervalFrames = 2000;

void LogWeaponCarry(const AimState& aim, const float* weaponAngles, float world, float weaponFov,
                    float ratio, int carried) {
    static LogThrottle s_throttle(kWeaponBurstLines, kWeaponEarlyLines,
                                  kWeaponEarlyIntervalFrames, kWeaponSteadyIntervalFrames);
    if (!s_throttle.ShouldLog()) return;
    HT_LOG("[weapon] fov=%.2f/%.2f ratio=%.4f drawn=(p%.2f y%.2f r%.2f) "
           "weapon=(p%.2f y%.2f r%.2f) carried=%d", world, weaponFov, ratio,
           aim.render_angles[0], aim.render_angles[1], aim.render_angles[2],
           weaponAngles[0], weaponAngles[1], weaponAngles[2], carried);
}

bool IsRenderableFov(float fov) { return std::isfinite(fov) && fov > 0.0f && fov < 179.0f; }

float TanHalfDegrees(float fov) { return std::tan(fov * 0.5f * kDegToRad); }

bool WasPosedThisFrame(const void* viewModel) {
    for (int i = 0; i < g_posedViewModelCount; ++i) {
        if (g_posedViewModels[i] == viewModel) return true;
    }
    return false;
}

// Moves the local player's freshly posed viewmodels for a frame whose render
// camera is final. Nothing moves on an untracked frame, a frame with no weapon
// FOV (a scripted camera renders 0 there), or an aim axis turned out of the
// drawn view, where the weapon is off the frame anyway.
void CarryViewModels(const ViewSetup& view, const AimState& aim) {
    if (!g_viewModelCarryInstalled.load(std::memory_order_acquire) || !aim.applied ||
        g_posedViewModelCount == 0) {
        return;
    }
    const float world = view.Fov();
    const float weaponFov = view.FovViewmodel();
    if (!IsRenderableFov(world) || !IsRenderableFov(weaponFov)) return;

    const float ratio = TanHalfDegrees(weaponFov) / TanHalfDegrees(world);
    float weaponAngles[3];
    if (!source::WeaponPassAngles(aim.render_angles, aim.clean_angles, ratio, weaponAngles)) {
        return;
    }

    void* const player = g_localPlayer();
    if (!player) return;

    const builds::ViewModelOffsets& off = g_profile->offsets.view_model;
    int carried = 0;
    for (int i = 0; i < kMaxViewModels; ++i) {
        void* const viewModel = g_getViewModel(player, nullptr, i, true);
        if (!viewModel || !WasPosedThisFrame(viewModel)) continue;

        const uint8_t* const fields = static_cast<const uint8_t*>(viewModel);
        float origin[3], angles[3];
        std::memcpy(origin, fields + off.local_origin, sizeof(origin));
        std::memcpy(angles, fields + off.local_angles, sizeof(angles));
        source::CarryPose(aim.clean_origin, weaponAngles, aim.render_origin, aim.render_angles,
                          origin, angles);
        g_setLocalOrigin(viewModel, nullptr, origin);
        g_setLocalAngles(viewModel, nullptr, angles);
        g_invalidateBoneCache(viewModel, nullptr);
        ++carried;
    }
    LogWeaponCarry(aim, weaponAngles, world, weaponFov, ratio, carried);
}

void __fastcall Hook_CalcViewModelView(void* viewModel, void* edx, void* owner,
                                       const float* eyePosition, const float* eyeAngles) {
    g_originalCalcViewModelView(viewModel, edx, owner, eyePosition, eyeAngles);
    if (viewModel && !WasPosedThisFrame(viewModel) && g_posedViewModelCount < kMaxViewModels) {
        g_posedViewModels[g_posedViewModelCount++] = viewModel;
    }
}

// ----- The hook -------------------------------------------------------------

// The tracking work, separated from the detour so the original call can sit
// outside the try.
void ApplyTracking(const ViewSetup& view) {
    Plugin& plugin = GetPlugin();
    // Unconditional, including in menus, so the pose cache and the connection
    // logging stay live while the player is in a menu - a tracker that
    // disconnects there is still worth a log line, and the pose is ready the
    // frame gameplay resumes.
    plugin.Update();

    const bool active = plugin.IsEnabled() && GetGameState().IsGameplayActive();

    float* org = view.Origin();
    float* ang = view.Angles();

    AimState aim;
    Copy3(aim.clean_origin, org);
    Copy3(aim.clean_angles, ang);

    TrackingDelta delta;
    if (active) {
        // The FOV is not gated on tracker data - it is a view setting, not a
        // pose, and a player whose tracker is asleep still wants the frame they
        // configured. It IS gated on the tracking toggle, so End leaves a
        // completely vanilla view behind rather than a vanilla view at a
        // modded FOV.
        const Config& config = plugin.GetConfig();
        // First, because it settles what this frame's FOV is - both the
        // override's write and the factor the pose is scaled by come out of it,
        // and the pose has to be scaled for the FOV the frame ACTUALLY renders
        // at, override included.
        const float zoom = PrepareFrameFov(view, config.fov_override,
                                           config.fov_viewmodel_override);

        float yawRad, pitchRad, rollRad;
        if (plugin.GetRotationRadians(yawRad, pitchRad, rollRad)) {
            delta.applied = true;
            // Position first: it reads the clean angles, which the rotation
            // below overwrites in place.
            ApplyPositionalLean(plugin, aim.clean_angles, org, zoom, delta);
            ApplyRotationDelta(plugin, yawRad, pitchRad, rollRad, zoom, ang, delta);
        }
    }

    // Published unconditionally, including the untracked case: a stale state
    // left behind after tracking stops would hold the crosshair off-centre with
    // nothing moving the view any more.
    aim.applied = delta.applied;
    Copy3(aim.render_origin, org);
    Copy3(aim.render_angles, ang);
    PublishAimState(aim);
    CarryViewModels(view, aim);

    DiagnosticLog(view, delta);
}

// The state a frame that applied no tracking leaves behind: clean and render
// cameras identical, nothing applied, so every consumer draws at centre.
void PublishUntrackedAimState(const ViewSetup& view) {
    AimState aim;
    Copy3(aim.clean_origin, view.Origin());
    Copy3(aim.clean_angles, view.Angles());
    Copy3(aim.render_origin, view.Origin());
    Copy3(aim.render_angles, view.Angles());
    aim.applied = false;
    PublishAimState(aim);
}

void __fastcall Hook_RenderView(void* ecx, void* edx, void* view, int clearFlags,
                                int whatToDraw) {
    if (view) {
        const ViewSetup setup(view, g_profile->offsets.view_setup);
        try {
            ApplyTracking(setup);
        } catch (...) {
            // Caught to reach the original call below: a C++ exception unwound
            // out of a __fastcall detour, through a MinHook trampoline and into
            // client.dll frames would skip it, which is a black screen and then
            // a terminate. This does NOT catch an access violation - the build
            // is /EHsc, where a structured exception is not a C++ one - so a
            // wrong CViewSetup offset still takes the process down rather than
            // hiding here.
            //
            // The aim state has to be republished, because ApplyTracking throws
            // before it publishes: leaving the last good frame's state up would
            // hold the crosshair compensated against a camera that is no longer
            // on screen. Cleared, so the crosshair falls back to centre.
            PublishUntrackedAimState(setup);
            static bool s_faultLogged = false;
            LogDetourFaultOnce(s_faultLogged, "hook", "RenderView");
        }
    }

    // Consumed or not, a pose belongs to this frame only.
    g_posedViewModelCount = 0;

    g_originalRenderView(ecx, edx, view, clearFlags, whatToDraw);
}

// ----- Installation ---------------------------------------------------------

// client.dll is loaded long after the ASI, so the bootstrap thread waits for it
// rather than giving up on the first miss.
constexpr int   kClientWaitAttempts   = 200;
constexpr DWORD kClientWaitIntervalMs = 100;

HMODULE WaitForClientModule() {
    for (int i = 0; i < kClientWaitAttempts; ++i) {
        if (HMODULE client = GetModuleHandleA("client.dll")) return client;
        Sleep(kClientWaitIntervalMs);
    }
    return nullptr;
}

// Fingerprints the running client.dll and returns its profile, or nullptr -
// which is the dormant path: the game runs vanilla and the log says why.
const builds::BuildProfile* ResolveBuildProfile(HMODULE client) {
    cameraunlock::memory::PeFingerprint fp{};
    if (!cameraunlock::memory::ReadPeFingerprint(client, fp)) {
        HT_LOG("[hook] could not read client.dll fingerprint");
        return nullptr;
    }
    HT_LOG("[hook] client.dll fingerprint TimeDateStamp=0x%08X SizeOfImage=0x%08X CheckSum=0x%08X",
           fp.TimeDateStamp, fp.SizeOfImage, fp.CheckSum);

    const builds::BuildProfile* profile = builds::MatchProfile(fp);
    if (!profile) {
        builds::LogUnrecognisedBuild(fp);
        return nullptr;
    }
    if (!profile->IsComplete()) {
        HT_LOG("[hook] build profile '%s' is a placeholder (hook target not yet rederived) "
               "- staying dormant", profile->name);
        return nullptr;
    }
    HT_LOG("[hook] matched build profile '%s'", profile->name);
    return profile;
}

// This is the mod's first hook, so it is where MinHook itself is brought up.
bool InstallRenderViewDetour(void* target) {
    using cameraunlock::hooks::HookManager;
    using cameraunlock::hooks::HookStatus;

    if (HookManager::Instance().Initialize() != HookStatus::Ok) {
        HT_LOG("[hook] MinHook init failed");
        return false;
    }
    return InstallDetour("hook", "RenderView", target,
                         reinterpret_cast<void*>(&Hook_RenderView),
                         reinterpret_cast<void**>(&g_originalRenderView));
}

// After RenderView, which is what moves the viewmodels this records. Optional:
// without it head tracking runs and the weapon keeps the magnified swing.
void InstallViewModelCarry(uintptr_t base) {
    constexpr const char* kWeaponNote =
        " - the weapon in your hands swings further than the view when you turn your head "
        "(head tracking is unaffected)";
    if (!g_profile->HasViewModelCarry()) {
        HT_LOG("[weapon] build profile has no viewmodel addresses%s", kWeaponNote);
        return;
    }
    const builds::ViewModelOffsets& off = g_profile->offsets.view_model;
    g_getViewModel = reinterpret_cast<GetViewModelFn>(base + off.get_view_model_rva);
    g_setLocalOrigin = reinterpret_cast<SetLocalVectorFn>(base + off.set_local_origin_rva);
    g_setLocalAngles = reinterpret_cast<SetLocalVectorFn>(base + off.set_local_angles_rva);
    g_invalidateBoneCache =
        reinterpret_cast<InvalidateBoneCacheFn>(base + off.invalidate_bone_cache_rva);
    g_localPlayer = reinterpret_cast<LocalPlayerFn>(base + g_profile->offsets.aim.local_player_rva);
    if (InstallDetour("weapon", "CalcViewModelView",
                      reinterpret_cast<void*>(base + off.calc_view_model_view_rva),
                      reinterpret_cast<void*>(&Hook_CalcViewModelView),
                      reinterpret_cast<void**>(&g_originalCalcViewModelView), kWeaponNote)) {
        g_viewModelCarryInstalled.store(true, std::memory_order_release);
    }
}

}  // namespace

bool CameraHook::Install() {
    HMODULE client = WaitForClientModule();
    if (!client) {
        HT_LOG("[hook] client.dll never loaded");
        return false;
    }

    // Published before the detour is armed: the very first RenderView can land
    // inside EnableHook, and it dereferences this.
    g_profile = ResolveBuildProfile(client);
    if (!g_profile) return false;

    // Before the detour too, and fatal if it fails: the gate is what keeps the
    // pose out of the menu backdrop and out of a multiplayer session, so a hook
    // installed without one is worse than no hook at all.
    if (!GetGameState().Resolve()) return false;

    ResolveFovConVars(client, *g_profile);

    const auto base = reinterpret_cast<uintptr_t>(client);
    if (!InstallRenderViewDetour(reinterpret_cast<void*>(base + g_profile->offsets.render_view_rva))) {
        return false;
    }
    InstallViewModelCarry(base);
    return true;
}

}  // namespace headtracking
