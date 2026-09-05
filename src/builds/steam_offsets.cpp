// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
// Steam Win32 build profiles for Portal with RTX's client.dll. Append-only: a
// new patch gets a new entry here and a new line at the top of kKnownProfiles in
// build_registry.cpp. Nothing in this file is ever edited in place - a user who
// has not taken the patch keeps matching their old profile by fingerprint.
//
// A new build's fingerprint comes from `pixi run check-fingerprint`. Its
// addresses are measured against that build: the client names its own functions
// in the telemetry strings it ships, so "CViewRender::RenderView" is named by
// the string that spells it out.

#include "builds/build_registry.h"

namespace headtracking::builds {

// CViewSetup is 200 bytes on this branch: RenderView copies it as 50 dwords
// before handing it to DrawMonitors, which is what fixes the size.
//
// Every field below is pinned by CViewRender::DrawOneMonitor, which builds a
// CViewSetup from scratch for a point_camera and so writes the ones that matter
// at known semantics: it stores the camera entity's GetAbsOrigin() at +0x40 and
// its GetAbsAngles() at +0x4C, its GetFOV() at +0x38, and the monitor's pixel
// width and height at +0x10 and +0x18. fovViewmodel is the float between fov and
// origin - the declaration order is fov, fovViewmodel, origin, angles, and the
// gap between the two measured fields is exactly one float wide.
//
// The rect ints are doubled (x/unscaledX, y/unscaledY, width/unscaledWidth),
// which is why width is at +0x10 rather than +0x08; RenderView passes
// (+0x00, +0x08, +0x10, +0x18) to the viewport calls, matching.
constexpr ViewSetupOffsets kViewSetupLayout_20250518 = {
    0x40u,  // origin (Vector x, y, z)
    0x4Cu,  // angles (QAngle pitch, yaw, roll)
    0x38u,  // fov, horizontal degrees, already widened for this viewport
    0x3Cu,  // fovViewmodel - the float straight after fov
    0x10u,  // rect width
    0x18u,  // rect height
};

// The first five are the functions CHudCrosshair::GetDrawPosition itself calls,
// in the order it calls them: it takes the viewport, asks for the local player,
// traces MASK_SHOT (0x46004003) along the aim, and projects the impact point
// through ScreenTransform. Reusing them is what stops our projection
// disagreeing with the frame the engine drew.
//
// GetDrawPosition is __cdecl here, not __thiscall - it never touches ecx - and
// it takes its QAngle by value, so its detour sees six stack arguments and no
// `this`.
//
// trace_t::endpos at 12 and fraction at 44 are CBaseTrace's. They are fixed by
// the same function rather than assumed: GetDrawPosition hands the trace
// buffer's base to UTIL_TraceLine and then passes base+12 to ScreenTransform as
// the world point, and the buffer it reserves is 84 bytes, which is sizeof
// (trace_t) on this branch.
//
// The last three are the two hard-centred Portal elements and the texture-draw
// primitive their corrections shift - see build_profile.h. DrawSelf is the
// five-argument (x, y, w, h, colour) form rather than the three-argument one,
// because that is the single call every draw in both elements ends in: the short
// form is a one-line forwarder to it.
constexpr AimOffsets kAimLayout_20250518 = {
    0x154300u,  // CHudCrosshair::GetDrawPosition
    0x0881B0u,  // UTIL_TraceLine
    0x1CD9C0u,  // ScreenTransform
    0x1C1070u,  // GetFullscreenViewport(&w, &h)
    0x0C4620u,  // C_BasePlayer::GetLocalPlayer
    12u,        // trace_t::endpos
    44u,        // trace_t::fraction
    0x232AB0u,  // CHudPortalCrosshair::Paint
    0x229EE0u,  // CHUDQuickInfo::Paint
    0x143230u,  // CHudTexture::DrawSelf(x, y, w, h, colour)
};

// The IVEngineClient* client.dll itself calls through, at client.dll+0x4DB0E4:
// ScreenTransform reads WorldToScreenMatrix off it, through slot 36, which is
// where that pointer was identified.
//
// The slot numbers are read off engine.dll's own VEngineClient014 object rather
// than carried over from a sibling mod. Its vtable holds, at 21, a bare load of
// the max-clients global; at 26, `signon == 6`; at 27, `signon >= 2`; at 28, a
// bare load of a bool. 51 returns "Dedicated Server", "" or cl.m_szLevelFileName
// depending on the same signon counter, which is GetLevelName outright. 84 tail-
// calls CClientState::IsPaused, and 87 loads a bool out of the server object,
// which is IsLevelMainMenuBackground. All six agree with slot 36 sitting where
// client.dll's own use of the interface says it does.
constexpr EngineStateOffsets kEngineState_20250518 = {
    0x4DB0E4u,
    "VEngineClient014",
    26u,  // IsInGame
    84u,  // IsPaused
    87u,  // IsLevelMainMenuBackground
    28u,  // IsDrawingLoadingImage
    21u,  // GetMaxClients
    51u,  // GetLevelName
};

// The FOV ConVars, located from their name strings: each name is pushed as the
// first argument of its ConVar constructor with the object itself in ecx, so the
// instruction pair names the object outright. fov_desired registers with flags
// 0x280 (FCVAR_ARCHIVE | FCVAR_USERINFO) and viewmodel_fov with 0x4000
// (FCVAR_CHEAT) - the reason the mod carries its own override at all.
//
// The two field offsets are the standard ConCommandBase / ConVar layout for
// 32-bit MSVC: vtable, m_pNext, m_bRegistered, then m_pszName at 0x0C, and past
// ConVar's second vtable and its parent/default/string members to m_fValue at
// 0x2C. Confirmed at load by reading the name back off the object.
constexpr FovConVarOffsets kFovConVars_20250518 = {
    0x50F458u,  // fov_desired
    0x503AA8u,  // viewmodel_fov
    0x0Cu,      // ConCommandBase::m_pszName
    0x2Cu,      // ConVar::m_fValue
};

// portal_rtx\bin\client.dll dated 2025-05-18, the build shipped with Steam app
// 2012840 at PatchVersion 1745010. RenderView is slot 6 of the CViewRender
// vftable at rva 0x3D1FF0, and its own Telemetry marker names
// "CViewRender::RenderView" at viewrender.cpp:1923.
extern const BuildProfile kSteamProfile_20250518 = {
    "steam-win32-20250518",
    { 0x6829867Du, 0x005C9000u, 0x00000000u },
    { 0x1DE520u, kViewSetupLayout_20250518, kAimLayout_20250518, kEngineState_20250518,
      kFovConVars_20250518 },
};

}  // namespace headtracking::builds
