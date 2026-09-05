// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#include "tracker_feed.h"

#include "angles.h"
#include "cameraunlock/math/smoothing_utils.h"
#include "debug_log.h"
#include "position_mapping.h"

namespace headtracking {

namespace {

// Sensitivity only. Inversion and deadzone are left at the processor's own
// defaults (off) and are not exposed: the tracker owns pose shaping, so a
// deadzone or a flipped axis belongs in the tracker app's profile where it
// behaves the same in every game. The engine's own axis conversion is a fixed
// sign table at the boundary in tracker_axes.h, not a user preference.
void ApplyRotationConfig(cameraunlock::TrackingProcessor& processor, const Config& c) {
    cameraunlock::SensitivitySettings s;
    s.yaw = c.sens_yaw;
    s.pitch = c.sens_pitch;
    s.roll = c.sens_roll;
    processor.SetSensitivity(s);
}

}  // namespace

void TrackerFeed::Start(const Config& config) {
    m_port = config.port;
    m_session.SetMode(config.pos_enabled
                          ? cameraunlock::TrackingMode::RotationAndPosition
                          : cameraunlock::TrackingMode::RotationOnly);

    ApplyRotationConfig(m_session.GetProcessor(), config);
    m_session.SetLocalSmoothing(config.local_smoothing);
    m_session.SetRemoteSmoothing(config.remote_smoothing);
    // Through the session, not straight onto the processor: the session owns
    // the smoothing pair and recomposes it onto the incoming struct, so the two
    // calls compose in either order rather than the settings silently carrying
    // the struct's own defaults. The session feeds the connection flag that
    // picks between them, from the receiver's source address, every update.
    m_session.SetPositionSettings(MakePositionSettings(config));
    // Our trackers report head position directly, so the core's synthetic
    // pivot-forward term (which cancels a webcam pivot) only injects phantom
    // rotation-coupled movement. Disable it.
    m_session.GetPositionProcessor().SetTrackerPivotForward(0.0f);

    m_receiver.SetLog([](const std::string& msg) { HT_LOG("[receiver] %s", msg.c_str()); });
    if (m_receiver.Start(m_port)) {
        HT_LOG("[plugin] listening on UDP %u", m_port);
    } else {
        // No cause named here. The [receiver] line immediately above carries
        // the OS's own words for this failure; "port busy" was a guess, and it
        // is the wrong one for a bind refused by a reserved port range, which
        // sends the user hunting an app that is not running.
        HT_LOG("[plugin] not listening on UDP %u yet, retrying every %dms in the background",
               m_port, cameraunlock::UdpReceiver::kRetryIntervalMs);
    }
}

void TrackerFeed::LogConnectionChange() {
    const bool isRemote = m_session.IsRemoteConnection();
    if (m_remoteConnectionKnown && isRemote == m_isRemoteConnection) return;
    m_isRemoteConnection = isRemote;
    m_remoteConnectionKnown = true;

    const double effective = cameraunlock::math::GetEffectiveSmoothing(
        m_session.GetLocalSmoothing(), m_session.GetRemoteSmoothing(), isRemote);
    HT_LOG("[plugin] tracker is %s, smoothing=%.3f", isRemote ? "remote" : "local", effective);
}

void TrackerFeed::Invalidate() {
    m_cachedValid.store(false, std::memory_order_release);
    m_cachedPosValid.store(false, std::memory_order_release);
}

// Requested here, applied in Update(). SetMode resets the position processor's
// smoothing and the interpolator, both plain float state that Update() is
// reading on the render thread, and this runs on the hotkey poller's thread. A
// press landing mid-frame could otherwise be observed half-applied, which is a
// bogus velocity term and a one-frame camera jerk.
void TrackerFeed::RequestCycleMode() { m_cycleRequested.store(true, std::memory_order_release); }

void TrackerFeed::CycleModeNow() {
    m_session.CycleMode();
    HT_LOG("[plugin] tracking mode -> %s", ModeName());
}

const char* TrackerFeed::ModeName() const {
    switch (m_session.GetMode()) {
        case cameraunlock::TrackingMode::RotationAndPosition: return "6DOF (rotation + position)";
        case cameraunlock::TrackingMode::RotationOnly:        return "rotation only";
        case cameraunlock::TrackingMode::PositionOnly:        return "position only";
    }
    return "?";
}

void TrackerFeed::Update(bool enabled) {
    if (m_cycleRequested.exchange(false, std::memory_order_acquire)) {
        m_session.CycleMode();
        // Drop whichever half the new mode no longer owns. The pose is held
        // across a tracker dropout, so without this a cycle to rotation-only
        // while the tracker is quiet would leave the frozen lean applied to the
        // render origin for as long as the gap lasts, having just logged that
        // position tracking was switched off.
        if (!m_session.IsRotationActive()) {
            m_cachedValid.store(false, std::memory_order_release);
        }
        if (!m_session.IsPositionActive()) {
            m_cachedPosValid.store(false, std::memory_order_release);
        }
        HT_LOG("[plugin] tracking mode -> %s", ModeName());
    }

    if (!enabled) {
        Invalidate();
        // The connection latch resets with the pose. Left set, the next
        // disconnect after the player toggles back on reports that it is
        // holding a pose that this very branch already dropped.
        m_wasConnected = false;
        return;
    }

    if (!m_receiver.IsReceiving()) {
        // Hold the last pose rather than dropping it. Invalidating here snaps
        // the view to the clean camera the moment a webcam loses the face or a
        // phone's wifi stalls, then snaps back when packets resume - two jumps
        // for a gap the user did not ask for. The cached pose stays published
        // and smoothing eases back in on its own when the tracker returns.
        if (m_wasConnected) {
            HT_LOG("[plugin] tracking source disconnected (no packets within timeout), "
                   "holding the last pose");
            m_wasConnected = false;
        }
        return;
    }
    if (!m_wasConnected) {
        HT_LOG("[plugin] tracking source connected on UDP %u (remote=%d)",
               m_port, m_receiver.IsRemoteConnection() ? 1 : 0);
        m_wasConnected = true;
    }

    const float dt = m_frameClock.Tick();
    // Hold on a session with nothing to give, for the same reason as the
    // freshness gate above. Before the first packet the cache is invalid
    // anyway, so this only ever holds a pose that was once real.
    if (!m_session.Update(dt)) return;
    LogConnectionChange();

    float yaw_deg = 0.0f, pitch_deg = 0.0f, roll_deg = 0.0f;
    m_session.GetRotation(yaw_deg, pitch_deg, roll_deg);
    m_cachedYaw.store(yaw_deg     * kDegToRad, std::memory_order_release);
    m_cachedPitch.store(pitch_deg * kDegToRad, std::memory_order_release);
    m_cachedRoll.store(roll_deg   * kDegToRad, std::memory_order_release);
    m_cachedValid.store(true, std::memory_order_release);

    float ox = 0.0f, oy = 0.0f, oz = 0.0f;
    if (m_session.GetPositionOffset(ox, oy, oz)) {
        m_cachedPosX.store(ox * kSourceUnitsPerMetre, std::memory_order_release);
        m_cachedPosY.store(oy * kSourceUnitsPerMetre, std::memory_order_release);
        m_cachedPosZ.store(oz * kSourceUnitsPerMetre, std::memory_order_release);
        m_cachedPosValid.store(true, std::memory_order_release);
    } else {
        m_cachedPosValid.store(false, std::memory_order_release);
    }
}

bool TrackerFeed::GetRotationRadians(float& yaw, float& pitch, float& roll) const {
    if (!m_cachedValid.load(std::memory_order_acquire)) return false;
    yaw   = m_cachedYaw.load(std::memory_order_acquire);
    pitch = m_cachedPitch.load(std::memory_order_acquire);
    roll  = m_cachedRoll.load(std::memory_order_acquire);
    return true;
}

bool TrackerFeed::GetPositionOffset(float& x, float& y, float& z) const {
    if (!m_cachedPosValid.load(std::memory_order_acquire)) return false;
    x = m_cachedPosX.load(std::memory_order_acquire);
    y = m_cachedPosY.load(std::memory_order_acquire);
    z = m_cachedPosZ.load(std::memory_order_acquire);
    return true;
}

}  // namespace headtracking
