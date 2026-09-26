// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#include "config.h"

#include <filesystem>
#include <string>
#include <system_error>

#include "cameraunlock/config/ini_reader.h"
#include "cameraunlock/os/module_paths.h"
#include "debug_log.h"
#include "legacy_config/legacy_config.h"

namespace headtracking {

namespace {

// Every value was validated by the reader, so the fields copy across as they are.
Config FromLegacy(const legacy::Config& l) {
    Config c;
    c.port = l.port;
    c.enabled_on_startup = l.enabled_on_startup;
    c.sens_yaw = l.sens_yaw;
    c.sens_pitch = l.sens_pitch;
    c.sens_roll = l.sens_roll;
    c.local_smoothing = l.local_smoothing;
    c.remote_smoothing = l.remote_smoothing;
    c.pos_enabled = l.pos_enabled;
    c.pos_sens_x = l.pos_sens_x;
    c.pos_sens_y = l.pos_sens_y;
    c.pos_sens_z = l.pos_sens_z;
    c.pos_limit_x = l.pos_limit_x;
    c.pos_limit_y = l.pos_limit_y;
    c.pos_limit_z = l.pos_limit_z;
    c.pos_limit_z_back = l.pos_limit_z_back;
    c.toggle_vk = l.toggle_vk;
    c.yaw_mode_vk = l.yaw_mode_vk;
    c.mode_cycle_vk = l.mode_cycle_vk;
    c.world_space_yaw = l.world_space_yaw;
    c.fov_override = l.fov_override;
    c.fov_viewmodel_override = l.fov_viewmodel_override;
    return c;
}

// ----- Writing the default ini ---------------------------------------------
//
// One writer per section. The reader they write for is the frozen one in
// legacy_config/.

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

    legacy::Config read;
    if (legacy::Read(path.c_str(), read) == legacy::ReadStatus::Absent) {
        HT_LOG("[config] could not open %s, using defaults", path.c_str());
    }
    return FromLegacy(read);
}

}  // namespace headtracking
