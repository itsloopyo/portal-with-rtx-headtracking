// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#include "crosshair_hook.h"

#include <Windows.h>
#include <cmath>
#include <cstdint>

#include "aim_point.h"
#include "builds/build_registry.h"
#include "debug_log.h"
#include "detour.h"

namespace headtracking {

namespace {

// void GetDrawPosition(float* pX, float* pY, bool* pbBehindCamera,
//                      QAngle angleCrosshairOffset)   [__cdecl]
//
// __cdecl, not __thiscall: the function ends in a bare `ret`, so the caller
// cleans the stack and there is no `this` in ecx - it takes the crosshair's
// state from globals rather than from an object. The QAngle is taken by value,
// which on x86 means its three floats are pushed individually, so they arrive
// here as three separate parameters. Both details are load-bearing; a detour
// with the wrong convention or arity unbalances the stack on every HUD paint.
using GetDrawPositionFn = void(__cdecl*)(float* x, float* y, bool* behindCamera,
                                         float offsetPitch, float offsetYaw, float offsetRoll);

// void CHudPortalCrosshair::Paint() / void CHUDQuickInfo::Paint()  [__thiscall]
using PaintFn = void(__fastcall*)(void* ecx, void* edx);

// void CHudTexture::DrawSelf(int x, int y, int w, int h, const Color& clr)  [__thiscall]
using DrawSelfFn = void(__fastcall*)(void* ecx, void* edx, int x, int y, int w, int h,
                                     const void* colour);

// Appended to every failure line for the two hard-centred elements, so each one
// says what the user loses rather than only what did not install.
constexpr const char* kCentredNote =
    " - Portal's own centred crosshair elements stay at screen centre "
    "(head tracking is unaffected)";

GetDrawPositionFn g_originalGetDrawPosition = nullptr;
PaintFn           g_originalPortalPaint = nullptr;
PaintFn           g_originalQuickInfoPaint = nullptr;
DrawSelfFn        g_originalDrawSelf = nullptr;

// Armed for the whole of one centred element's Paint, so EVERY texture that
// element draws moves by the same pixel delta and the crosshair stays one
// object. Render-thread only: a Paint and the draws inside it are one call
// stack, and every other HUD element's draw happens outside it.
bool g_shiftActive = false;
int  g_shiftX = 0;
int  g_shiftY = 0;

// The crosshair offset angle is a weapon's own aim correction, expressed
// relative to the view angles. Vanilla adds it to CurrentViewAngles(), which is
// the head-tracked camera; the decoupled version has to add it to the clean aim
// instead, or a weapon that uses one would have its correction measured from
// the wrong camera.
void __cdecl Hook_GetDrawPosition(float* x, float* y, bool* behindCamera,
                                  float offsetPitch, float offsetYaw, float offsetRoll) {
    g_originalGetDrawPosition(x, y, behindCamera, offsetPitch, offsetYaw, offsetRoll);

    // Guarded for the same reason the render-view detour is (see
    // camera_hook.cpp): the correction allocates - it logs through
    // std::string - and an exception unwinding out of a detour, through the
    // trampoline and into client.dll's HUD paint, ends the process. A frame
    // drawn with the engine's own centred crosshair does not.
    try {
        const float offset[3] = { offsetPitch, offsetYaw, offsetRoll };
        float px = 0.0f, py = 0.0f;
        bool behind = false;
        // False is the untracked frame and the unresolvable trace, and both
        // mean the same thing here: leave the engine's own answer alone.
        if (!ComputeReticlePosition(offset, px, py, behind)) return;

        *x = px;
        *y = py;
        *behindCamera = behind;
    } catch (...) {
        // Swallowed, because the engine's own answer is already in place - but
        // said once, so a crosshair that never moves has a reason in the log.
        static bool s_faultLogged = false;
        LogDetourFaultOnce(s_faultLogged, "crosshair", "GetDrawPosition");
    }
}

// Shared by both hard-centred elements: work out this frame's shift, run the
// element's own Paint with it armed, disarm.
void PaintShifted(PaintFn original, void* ecx, void* edx) {
    bool skipDraw = false;
    try {
        float dx = 0.0f, dy = 0.0f;
        bool behind = false;
        if (ComputeReticleOffsetFromCentre(dx, dy, behind)) {
            // Behind the rendered view - a head turn far enough that the aim
            // point is off the back of the frame. Clamping it to an edge would
            // be a crosshair that lies about where the shot goes, so the element
            // does not paint this frame. That is the same answer the shared
            // crosshair reaches through its behind-camera flag.
            skipDraw = behind;
            // Floored, not truncated. The offset already carries the engine's
            // own round-to-nearest half-pixel, so floor is the integer the
            // shared crosshair element lands on; truncation rounds negative
            // offsets toward zero instead, which puts the dotted crosshair and
            // the portal brackets a pixel apart from it on every leftward and
            // upward head movement, and gives a two-pixel dead band across
            // centre. The whole point of the shift is that the cluster stays
            // one object.
            g_shiftX = static_cast<int>(std::floor(dx));
            g_shiftY = static_cast<int>(std::floor(dy));
            g_shiftActive = true;
        }
    } catch (...) {
        g_shiftActive = false;
        static bool s_faultLogged = false;
        LogDetourFaultOnce(s_faultLogged, "crosshair", "Paint");
    }

    if (!skipDraw) original(ecx, edx);
    g_shiftActive = false;
}

void __fastcall Hook_PortalCrosshairPaint(void* ecx, void* edx) {
    PaintShifted(g_originalPortalPaint, ecx, edx);
}

void __fastcall Hook_QuickInfoPaint(void* ecx, void* edx) {
    PaintShifted(g_originalQuickInfoPaint, ecx, edx);
}

void __fastcall Hook_DrawSelf(void* ecx, void* edx, int x, int y, int w, int h,
                              const void* colour) {
    if (g_shiftActive) {
        x += g_shiftX;
        y += g_shiftY;
    }
    g_originalDrawSelf(ecx, edx, x, y, w, h, colour);
}

}  // namespace

bool CrosshairHook::Install() {
    // ResolveAimPoint is the gate on both the build profile and its aim
    // addresses, so a profile is guaranteed by the time it returns true.
    if (!ResolveAimPoint()) return false;
    const builds::BuildProfile* profile = builds::ActiveProfile();

    HMODULE client = GetModuleHandleA("client.dll");
    if (!client) {
        HT_LOG("[crosshair] client.dll not loaded");
        return false;
    }
    const auto base = reinterpret_cast<uintptr_t>(client);
    const builds::AimOffsets& off = profile->offsets.aim;

    const bool sharedOk = InstallDetour(
        "crosshair", "GetDrawPosition",
        reinterpret_cast<void*>(base + off.draw_position_rva),
        reinterpret_cast<void*>(&Hook_GetDrawPosition),
        reinterpret_cast<void**>(&g_originalGetDrawPosition),
        " - the crosshair stays centred (head tracking is unaffected)");

    if (!profile->HasCentredCrosshairElements()) {
        HT_LOG("[crosshair] build profile has no centred-element addresses%s", kCentredNote);
        return sharedOk;
    }

    // The draw hook goes in first, and the two Paint hooks only if it took. The
    // shift is applied there and armed by them, so a Paint hook without it would
    // arm a flag nothing reads.
    if (!InstallDetour("crosshair", "CHudTexture::DrawSelf",
                       reinterpret_cast<void*>(base + off.hud_texture_draw_self_rva),
                       reinterpret_cast<void*>(&Hook_DrawSelf),
                       reinterpret_cast<void**>(&g_originalDrawSelf), kCentredNote)) {
        return sharedOk;
    }

    const bool portalOk = InstallDetour(
        "crosshair", "CHudPortalCrosshair::Paint",
        reinterpret_cast<void*>(base + off.portal_crosshair_paint_rva),
        reinterpret_cast<void*>(&Hook_PortalCrosshairPaint),
        reinterpret_cast<void**>(&g_originalPortalPaint), kCentredNote);

    const bool quickInfoOk = InstallDetour(
        "crosshair", "CHUDQuickInfo::Paint",
        reinterpret_cast<void*>(base + off.quick_info_paint_rva),
        reinterpret_cast<void*>(&Hook_QuickInfoPaint),
        reinterpret_cast<void**>(&g_originalQuickInfoPaint), kCentredNote);

    return sharedOk && portalOk && quickInfoOk;
}

}  // namespace headtracking
