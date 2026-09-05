// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#include "config.h"

#include <cmath>
#include <filesystem>
#include <string>
#include <system_error>

#include "cameraunlock/config/ini_reader.h"
#include "cameraunlock/os/module_paths.h"
#include "debug_log.h"

namespace headtracking {

namespace {

using Reader = cameraunlock::IniReader;

// ----- Key readers ----------------------------------------------------------
//
// The INI is a system boundary: every value below is whatever a user typed, so
// each one is validated here and nothing downstream re-checks it. Reading and
// sanitizing are one step so a key's default is named once - passing one
// default to the reader and a different one to the check is exactly the drift
// this shape prevents.

// Every magnitude the ini carries - sensitivities and lean limits - is bounded,
// not merely checked for a sign. Each one reaches the render view unclamped by
// anything downstream, so an out-of-band number is a camera driven through world
// geometry (a lean limit in the hundreds) or an axis that is quietly dead. Each
// band is stated with its key in the generated ini; refusing a value loudly is
// the only way the user learns which of their keys the mod is not honouring.
float ReadInRange(const Reader& r, const char* section, const char* key,
                  float lo, float hi, float fallback) {
    const float value = r.ReadFloat(section, key, fallback);
    if (std::isfinite(value) && value >= lo && value <= hi) return value;
    HT_LOG("[config] [%s] %s %.3f is outside %.2f to %.2f - using %.3f",
           section, key, value, lo, hi, fallback);
    return fallback;
}

float ReadSmoothing(const Reader& r, const char* key, float fallback) {
    const float value = r.ReadFloat("Smoothing", key, fallback);
    if (!std::isfinite(value)) return fallback;
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

// Remapping is allowed to any real virtual key - that is the user's call -
// except the Ctrl+Shift chord letters, which are already registered.
int ValidHotkeyOr(int vk, int fallback, const char* name) {
    for (int letter : hotkeys::kChordLetters) {
        if (vk == letter) {
            HT_LOG("[config] hotkey %s (0x%02X) collides with a Ctrl+Shift chord "
                   "letter - using default 0x%02X", name, vk, fallback);
            return fallback;
        }
    }
    if (vk < 0x01 || vk > 0xFE) {
        HT_LOG("[config] hotkey %s (0x%02X) is not a virtual-key code "
               "- using default 0x%02X", name, vk, fallback);
        return fallback;
    }
    return vk;
}

float ReadFovOverride(const Reader& r, const char* key) {
    const float value = r.ReadFloat("View", key, kDefaultFovOverride);
    if (std::isfinite(value)
        && (value == 0.0f || (value >= kMinFovOverride && value <= kMaxFovOverride))) {
        return value;
    }
    HT_LOG("[config] [View] %s %.2f is out of range - leaving the game's FOV alone. "
           "Valid values are %.0f to %.0f (degrees, as fov_desired), or 0 for off.",
           key, value, kMinFovOverride, kMaxFovOverride);
    return kDefaultFovOverride;
}

// The old value is deliberately NOT migrated into the new keys. The single
// smoothing value carried a hidden 0.15 floor, so the number in an existing
// config does not mean what it used to: copying it across would hand a local
// user smoothing they never chose under the new semantics, and copying it into
// only one of the two keys would be a guess about which connection they were on.
void WarnRetiredSmoothingKey(const Reader& reader, const char* section, const char* key) {
    if (reader.ReadString(section, key, "").empty()) return;
    HT_LOG(
        "Config key [%s] %s has been retired and is IGNORED. Smoothing is now two "
        "keys: LocalSmoothing (default 0, applies to a tracker on this machine) and "
        "RemoteSmoothing (default 0.15, applies to a tracker on the network). The "
        "old value is not migrated because the semantics changed - it carried a "
        "hidden 0.15 floor that no longer exists. Set the two new keys.",
        section, key);
}

// ----- Section loaders ------------------------------------------------------

// ReadInt yields 0 for a present-but-unparseable value rather than the
// fallback, so the range check below is what catches a typo too. Logged
// because this is the one config error whose symptom is "no head tracking at
// all": the mod binds the default port, the tracker keeps sending to the one
// in the file, and nothing else in the log says why the packets never arrive.
uint16_t ReadPort(const Reader& r, const char* key, uint16_t fallback) {
    const int port = r.ReadInt("Network", key, fallback);
    if (port < 1 || port > 65535) {
        HT_LOG("[config] [Network] %s %d is not a valid port - using %u. "
               "Point your tracker app at %u, or set a port in 1-65535.",
               key, port, fallback, fallback);
        return fallback;
    }
    return static_cast<uint16_t>(port);
}

void LoadNetwork(const Reader& r, Config& c) {
    c.port = ReadPort(r, "Port", kDefaultPort);
    c.enabled_on_startup = r.ReadBool("Network", "EnableOnStartup", kDefaultEnableOnStartup);
}

void LoadSensitivity(const Reader& r, Config& c) {
    c.sens_yaw =
        ReadInRange(r, "Sensitivity", "Yaw", kMinSensitivity, kMaxSensitivity, kDefaultSensitivity);
    c.sens_pitch = ReadInRange(r, "Sensitivity", "Pitch", kMinSensitivity, kMaxSensitivity,
                               kDefaultSensitivity);
    c.sens_roll = ReadInRange(r, "Sensitivity", "Roll", kMinSensitivity, kMaxSensitivity,
                              kDefaultSensitivity);
}

void LoadSmoothing(const Reader& r, Config& c) {
    c.local_smoothing  = ReadSmoothing(r, "LocalSmoothing",  kDefaultLocalSmoothing);
    c.remote_smoothing = ReadSmoothing(r, "RemoteSmoothing", kDefaultRemoteSmoothing);
    WarnRetiredSmoothingKey(r, "Smoothing", "Amount");
    WarnRetiredSmoothingKey(r, "Position", "Smoothing");
}

void LoadPosition(const Reader& r, Config& c) {
    c.pos_enabled = r.ReadBool("Position", "Enabled", kDefaultPosEnabled);

    c.pos_sens_x = ReadInRange(r, "Position", "SensX", kMinPosSensitivity, kMaxPosSensitivity,
                               kDefaultPosSensitivity);
    c.pos_sens_y = ReadInRange(r, "Position", "SensY", kMinPosSensitivity, kMaxPosSensitivity,
                               kDefaultPosSensitivity);
    c.pos_sens_z = ReadInRange(r, "Position", "SensZ", kMinPosSensitivity, kMaxPosSensitivity,
                               kDefaultPosSensitivity);

    c.pos_limit_x =
        ReadInRange(r, "Position", "LimitX", kMinPosLimit, kMaxPosLimit, kDefaultPosLimitX);
    c.pos_limit_y =
        ReadInRange(r, "Position", "LimitY", kMinPosLimit, kMaxPosLimit, kDefaultPosLimitY);
    c.pos_limit_z =
        ReadInRange(r, "Position", "LimitZ", kMinPosLimit, kMaxPosLimit, kDefaultPosLimitZ);
    c.pos_limit_z_back = ReadInRange(r, "Position", "LimitZBack", kMinPosLimit, kMaxPosLimit,
                                     kDefaultPosLimitZBack);
}

// Two actions on one key both fire on a single press, which reads as the toggle
// being broken rather than as a config error. All three go back to their
// defaults rather than just the colliding one: dropping one key onto its default
// can land it on the key that was already there, and the three defaults are the
// only set guaranteed to be distinct.
void RejectCollidingHotkeys(Config& c) {
    using namespace hotkeys;
    if (c.toggle_vk != c.yaw_mode_vk && c.toggle_vk != c.mode_cycle_vk &&
        c.yaw_mode_vk != c.mode_cycle_vk) {
        return;
    }
    HT_LOG("[config] [Hotkeys] Toggle 0x%02X, YawMode 0x%02X and ModeCycle 0x%02X are not three "
           "distinct keys - using the defaults 0x%02X, 0x%02X and 0x%02X",
           c.toggle_vk, c.yaw_mode_vk, c.mode_cycle_vk, kVkEnd, kVkPageDown, kVkPageUp);
    c.toggle_vk     = kVkEnd;
    c.yaw_mode_vk   = kVkPageDown;
    c.mode_cycle_vk = kVkPageUp;
}

void LoadHotkeys(const Reader& r, Config& c) {
    using namespace hotkeys;
    c.toggle_vk =
        ValidHotkeyOr(r.ReadHex("Hotkeys", "Toggle", kVkEnd), kVkEnd, "Toggle");
    c.yaw_mode_vk =
        ValidHotkeyOr(r.ReadHex("Hotkeys", "YawMode", kVkPageDown), kVkPageDown, "YawMode");
    c.mode_cycle_vk =
        ValidHotkeyOr(r.ReadHex("Hotkeys", "ModeCycle", kVkPageUp), kVkPageUp, "ModeCycle");

    RejectCollidingHotkeys(c);
}

void LoadView(const Reader& r, Config& c) {
    c.world_space_yaw = r.ReadBool("View", "WorldSpaceYaw", kDefaultWorldSpaceYaw);
    c.fov_override           = ReadFovOverride(r, "Fov");
    c.fov_viewmodel_override = ReadFovOverride(r, "FovViewmodel");
}

// ----- Writing the default ini ---------------------------------------------
//
// One writer per section, paired with the loader above it: a key that gains a
// validation rule and a key that gains a comment sit next to each other, so
// neither half can quietly stop describing the other.

void WriteNetwork(cameraunlock::IniWriter& w) {
    w.WriteSection("Network");
    w.WriteInt("Port", kDefaultPort);
    w.WriteBool("EnableOnStartup", kDefaultEnableOnStartup);
}

void WriteSensitivity(cameraunlock::IniWriter& w) {
    w.WriteSection("Sensitivity");
    w.WriteComment(" Scales the tracker's rotation before it reaches the view. 1 is 1:1,");
    w.WriteComment(" and the accepted range is 0.1 to 3. A value outside it is refused and");
    w.WriteComment(" noted in the log. Shape the pose in your tracker app instead where you");
    w.WriteComment(" can - a profile there behaves the same in every game.");
    w.WriteDouble("Yaw", kDefaultSensitivity);
    w.WriteDouble("Pitch", kDefaultSensitivity);
    w.WriteDouble("Roll", kDefaultSensitivity);
}

void WriteSmoothing(cameraunlock::IniWriter& w) {
    w.WriteSection("Smoothing");
    w.WriteComment(" Picked per connection from the tracker's source address, and applied");
    w.WriteComment(" to both rotation and position. 0 = no smoothing, 1 = heavy.");
    w.WriteComment(" LocalSmoothing: tracker runs on this machine (loopback)");
    w.WriteDouble("LocalSmoothing", kDefaultLocalSmoothing);
    w.WriteComment(" RemoteSmoothing: tracker is a remote device on the network");
    w.WriteDouble("RemoteSmoothing", kDefaultRemoteSmoothing);
}

void WritePosition(cameraunlock::IniWriter& w) {
    w.WriteSection("Position");
    w.WriteComment(" 6DOF head position, applied to the render view origin only");
    w.WriteBool("Enabled", kDefaultPosEnabled);
    w.WriteComment(" Scales head travel before the limits below, so the envelope keeps");
    w.WriteComment(" meaning metres. 1 is 1:1 with your real head movement, and the");
    w.WriteComment(" accepted range is 0 to 5.");
    w.WriteDouble("SensX", kDefaultPosSensitivity);
    w.WriteDouble("SensY", kDefaultPosSensitivity);
    w.WriteDouble("SensZ", kDefaultPosSensitivity);
    w.WriteComment(" Movement envelope in metres, each accepted from 0.01 to 0.5. Z is");
    w.WriteComment(" asymmetric on purpose: leaning in gets more room than pulling back,");
    w.WriteComment(" which would clip the player model.");
    w.WriteDouble("LimitX", kDefaultPosLimitX);
    w.WriteDouble("LimitY", kDefaultPosLimitY);
    w.WriteDouble("LimitZ", kDefaultPosLimitZ);
    w.WriteDouble("LimitZBack", kDefaultPosLimitZBack);
}

void WriteHotkeys(cameraunlock::IniWriter& w) {
    w.WriteSection("Hotkeys");
    w.WriteHex("Toggle", hotkeys::kVkEnd);
    w.WriteHex("YawMode", hotkeys::kVkPageDown);
    w.WriteComment(" Page Up: cycle 6DOF -> rotation-only -> position-only");
    w.WriteHex("ModeCycle", hotkeys::kVkPageUp);
}

void WriteView(cameraunlock::IniWriter& w) {
    w.WriteSection("View");
    w.WriteComment(" true = horizon-locked yaw (default), false = camera-local yaw");
    w.WriteBool("WorldSpaceYaw", kDefaultWorldSpaceYaw);
    w.WriteComment(" Field of view, same units as the game's fov_desired cvar (horizontal");
    w.WriteComment(" degrees at 4:3; the mod widens it for your real aspect ratio as the");
    w.WriteComment(" engine does). Written into the render view rather than the cvar, so it");
    w.WriteComment(" is not bound by fov_desired's own range. Accepted from 30 to 150, or");
    w.WriteComment(" 0 to leave the game's FOV alone. Applies only while tracking is");
    w.WriteComment(" enabled (End).");
    w.WriteDouble("Fov", kDefaultFovOverride);
    w.WriteComment(" The weapon is drawn with its own FOV. Widening Fov leaves the gun");
    w.WriteComment(" looking oversized against the wider world: LOWER this to shrink it.");
    w.WriteComment(" 0 = leave the game's viewmodel FOV alone.");
    w.WriteDouble("FovViewmodel", kDefaultFovOverride);
}

void WriteDebug(cameraunlock::IniWriter& w) {
    w.WriteSection("Debug");
    w.WriteComment(" Writes HeadTracking.log next to hl2.exe, fresh every launch (the");
    w.WriteComment(" previous session is kept as HeadTracking.prev.log, and nothing else). It");
    w.WriteComment(" records the build profile, the tracker connection and the pose being");
    w.WriteComment(" applied. That is the file to attach to a bug report - leave it on.");
    w.WriteBool("LogToFile", kDefaultLogToFile);
}

// ----- Paths ----------------------------------------------------------------

// The core resolver rather than a local GetModuleFileNameA: that call truncates
// a long install path instead of failing, and best-fit ANSI narrowing can map a
// directory onto the name of a DIFFERENT one that exists, which would read and
// write the config somewhere the user never looks. Both are refused there.
//
// An unresolvable directory leaves the name relative to the process working
// directory, and that is said out loud. Silently, it is the worst failure the
// config has: a default ini is written somewhere the user will never find, read
// straight back without error, and every setting they edited is ignored while
// the log reports a healthy load.
std::string IniPath() {
    const std::string dir = cameraunlock::os::HostExeDirectoryNarrow();
    if (dir.empty()) {
        HT_LOG("[config] could not resolve the game directory - reading and writing "
               "HeadTracking.ini relative to the working directory, which is probably not "
               "next to hl2.exe");
        return "HeadTracking.ini";
    }
    return dir + "\\HeadTracking.ini";
}

void WriteDefaultIni(const std::string& path) {
    cameraunlock::IniWriter w;
    if (!w.Open(path)) {
        HT_LOG("[config] failed to write default ini at %s", path.c_str());
        return;
    }
    w.WriteComment(" Portal with RTX head tracking - default config");

    void (*const sections[])(cameraunlock::IniWriter&) = {
        WriteNetwork, WriteSensitivity, WriteSmoothing, WritePosition,
        WriteHotkeys, WriteView,        WriteDebug,
    };
    for (auto section : sections) {
        w.WriteBlankLine();
        section(w);
    }
}

}  // namespace

bool Config::FileLoggingRequested() {
    // Open() is the existence check: it stats the path and returns false when
    // the file is not there, which is the same answer for a config that has not
    // been written yet and one that cannot be read. std::filesystem::exists
    // would be a second stat that THROWS on anything but a plain "not found",
    // and this runs on the bootstrap thread before the log is even open, where
    // an escaping exception is a terminated game.
    cameraunlock::IniReader r;
    if (!r.Open(IniPath())) return kDefaultLogToFile;
    return r.ReadBool("Debug", "LogToFile", kDefaultLogToFile);
}

Config Config::LoadOrCreateDefault() {
    const std::string path = IniPath();
    // The error_code overload, not the throwing one: see FileLoggingRequested.
    // A path that cannot be queried reads as absent, so the default is written
    // (and a failed write says so) rather than unwinding out of the thread.
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        WriteDefaultIni(path);
    }

    cameraunlock::IniReader r;
    Config c;
    if (!r.Open(path)) {
        HT_LOG("[config] could not open %s, using defaults", path.c_str());
        return c;
    }

    LoadNetwork(r, c);
    LoadSensitivity(r, c);
    LoadSmoothing(r, c);
    LoadPosition(r, c);
    LoadHotkeys(r, c);
    LoadView(r, c);
    return c;
}

}  // namespace headtracking
