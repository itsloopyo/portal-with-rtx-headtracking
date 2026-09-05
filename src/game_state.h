// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#pragma once

#include "builds/build_profile.h"

namespace headtracking {

// Whether the head pose should reach the camera at all this frame.
//
// The render-view hook fires for everything the engine draws a 3D world for,
// which includes the animated map behind the main menu, the frame under the
// pause menu and the last frame held on screen while a save loads. Tracking in
// any of those is wrong: nothing the player does with their head is meant to
// move a menu backdrop, and a paused game that keeps drifting looks broken.
//
// Portal with RTX has no multiplayer: its gameinfo.txt declares
// `type singleplayer_only` and the install ships no server binary or
// multiplayer campaign dir. The max-clients test below is kept anyway. It costs
// two loads a frame, it is the same latch every Source mod in the fleet carries,
// and it is what would hold the line if a listen server were ever reached from a
// client.dll this registry recognises.
class GameState {
public:
    // Resolves the engine interface the gate reads. Returns false when it
    // cannot be trusted, which is what leaves the whole mod dormant - a gate
    // that cannot say "not in gameplay" is not a gate.
    bool Resolve();

    // Re-evaluated once per rendered frame; the individual reads are a handful
    // of loads each, and the level name is only fetched when the cheaper tests
    // have already passed.
    bool IsGameplayActive();

private:
    void LogTransition(bool active, const char* why);

    // Set together by Resolve, and only on success: the slot numbers are only
    // meaningful for the interface version the same profile named, so a gate
    // holding one without the other could not be read at all.
    void* m_engine = nullptr;
    const builds::EngineStateOffsets* m_offsets = nullptr;

    bool m_lastActive = false;
    bool m_everLogged = false;
};

GameState& GetGameState();

}  // namespace headtracking
