// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#pragma once

#include <cstdio>

// The one assertion the suite is built from, and the failure count each
// translation unit's Run* function returns.
//
// Deliberately an anonymous namespace in a header: every test file keeps its
// own counter, which is what lets test_main.cpp sum them and report a per-area
// total. A single shared counter would need a link-time definition and would
// make each Run* return the whole suite's failures rather than its own.
namespace {

int g_failures = 0;

void Check(bool cond, const char* name) {
    if (cond) {
        std::printf("  [PASS] %s\n", name);
    } else {
        std::printf("  [FAIL] %s\n", name);
        ++g_failures;
    }
}

}  // namespace
