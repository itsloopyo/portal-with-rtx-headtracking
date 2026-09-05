// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#include "builds/build_registry.h"

#include "debug_log.h"

namespace headtracking::builds {

namespace {

// Newest first: kKnownProfiles[0] is the diagnostic primary an unrecognised
// build is compared against to word the dormant-path log line.
const BuildProfile* const kKnownProfiles[] = {
    &kSteamProfile_20250518,
};

const BuildProfile* g_active = nullptr;

}  // namespace

const BuildProfile* const* KnownProfiles(size_t& count) {
    count = sizeof(kKnownProfiles) / sizeof(kKnownProfiles[0]);
    return kKnownProfiles;
}

const BuildProfile* MatchProfile(const cameraunlock::memory::PeFingerprint& fp) {
    for (const BuildProfile* p : kKnownProfiles) {
        if (!p->fingerprint.Matches(fp)) continue;
        // A placeholder - a build spotted but not yet rederived - is returned so
        // the caller can name it in the dormant log line, but it is never
        // published. ActiveProfile() is where the crosshair, aim and game-state
        // hooks read their RVAs from, and a profile the mod declined to engage
        // on must not be reachable there: that would leave the failsafe
        // depending on the order one caller happens to run its checks in.
        if (p->IsComplete()) g_active = p;
        return p;
    }
    return nullptr;
}

const BuildProfile* ActiveProfile() { return g_active; }

void LogUnrecognisedBuild(const cameraunlock::memory::PeFingerprint& fp) {
    using cameraunlock::memory::FingerprintMismatch;
    const auto mismatch =
        cameraunlock::memory::ClassifyMismatch(fp, kKnownProfiles[0]->fingerprint);
    const char* hint =
        (mismatch == FingerprintMismatch::Newer)
            ? "game is newer than this mod knows about; check the releases page for an update"
        : (mismatch == FingerprintMismatch::Older)
            ? "game is older; let Steam finish updating"
            : "tampered/repacked client.dll; mod will not engage";
    HT_LOG("[hook] no build profile matches this client.dll - staying dormant (%s)", hint);
    for (const BuildProfile* p : kKnownProfiles) {
        HT_LOG("[hook]   known profile '%s': TimeDateStamp=0x%08X SizeOfImage=0x%08X "
               "CheckSum=0x%08X",
               p->name, p->fingerprint.TimeDateStamp, p->fingerprint.SizeOfImage,
               p->fingerprint.CheckSum);
    }
}

}  // namespace headtracking::builds
