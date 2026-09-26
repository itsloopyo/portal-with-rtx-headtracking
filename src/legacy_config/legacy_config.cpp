// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#include "legacy_config/legacy_config.h"

#include <cmath>

#include "cameraunlock/config/ini_reader.h"
#include "debug_log.h"

namespace headtracking::legacy {

namespace {

using Reader = cameraunlock::IniReader;

constexpr uint16_t kDefaultPort = 4242;
constexpr bool     kDefaultEnableOnStartup = true;

constexpr float kDefaultSensitivity = 1.0f;
constexpr float kMinSensitivity = 0.1f;
constexpr float kMaxSensitivity = 3.0f;

constexpr float kDefaultLocalSmoothing  = 0.0f;
constexpr float kDefaultRemoteSmoothing = 0.15f;

constexpr bool  kDefaultPosEnabled     = true;
constexpr float kDefaultPosSensitivity = 1.0f;
constexpr float kMinPosSensitivity     = 0.0f;
constexpr float kMaxPosSensitivity     = 5.0f;
constexpr float kDefaultPosLimitX      = 0.30f;
constexpr float kDefaultPosLimitY      = 0.20f;
constexpr float kDefaultPosLimitZ      = 0.40f;
constexpr float kDefaultPosLimitZBack  = 0.10f;
constexpr float kMinPosLimit = 0.01f;
constexpr float kMaxPosLimit = 0.5f;

constexpr bool  kDefaultWorldSpaceYaw = true;
constexpr float kDefaultFovOverride   = 0.0f;
constexpr bool  kDefaultLogToFile     = true;

constexpr float kMinFovOverride = 30.0f;
constexpr float kMaxFovOverride = 150.0f;

constexpr int kVkEnd      = 0x23;
constexpr int kVkPageDown = 0x22;
constexpr int kVkPageUp   = 0x21;
// The Ctrl+Shift chord letters v0.1.0 registered, which a bare rebind could not take.
constexpr int kChordLetters[] = { 0x59, 0x47, 0x48 };  // Y, G, H

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

int ValidHotkeyOr(int vk, int fallback, const char* name) {
    for (int letter : kChordLetters) {
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

void RejectCollidingHotkeys(Config& c) {
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

}  // namespace

ReadStatus Read(const char* path, Config& c) {
    cameraunlock::IniReader r;
    if (!r.Open(path)) return ReadStatus::Absent;

    LoadNetwork(r, c);
    LoadSensitivity(r, c);
    LoadSmoothing(r, c);
    LoadPosition(r, c);
    LoadHotkeys(r, c);
    LoadView(r, c);
    c.log_to_file = r.ReadBool("Debug", "LogToFile", kDefaultLogToFile);
    return ReadStatus::Read;
}

}  // namespace headtracking::legacy
