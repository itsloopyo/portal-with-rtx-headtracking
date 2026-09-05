// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#include "debug_log.h"

#include <string>

#include "cameraunlock/os/module_paths.h"

namespace headtracking {

// The core resolver rather than a local GetModuleFileNameW: that call truncates
// instead of failing, so a game installed under a long path would silently put
// the log somewhere else. An unresolvable directory leaves the name relative and
// the log lands in the process's working directory.
void OpenLogFile() {
    std::wstring path = cameraunlock::os::HostExeDirectory();
    if (!path.empty()) path += L'\\';
    cameraunlock::logging::Open(path + L"HeadTracking.log");
}

void LogDetourFaultOnce(bool& logged, const char* tag, const char* what) {
    if (logged) return;
    logged = true;
    // Guarded because this is called from the catch block of a detour, and an
    // exception leaving that block unwinds into the trampoline - the outcome
    // the catch exists to prevent. Line() only takes a mutex and writes to a
    // stack buffer, so this is a formality, but it is a formality that costs
    // the process when it is not observed.
    try {
        HT_LOG("[%s] %s threw - that frame was left as the game drew it, and the mod's "
               "effect is absent for as long as it repeats. Logged once.", tag, what);
    } catch (...) {
    }
}

}  // namespace headtracking
