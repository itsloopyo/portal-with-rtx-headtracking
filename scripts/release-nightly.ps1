#!/usr/bin/env pwsh
#Requires -Version 5.1
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 itsloopyo / CameraUnlock

# Thin shim. Determine version, delegate to the shared publisher.
# See cameraunlock-core/powershell/NightlyRelease.psm1 for what it does.

[CmdletBinding()]
param(
    [switch]$AllowDirty
)

$ErrorActionPreference = 'Stop'

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot '..')

Import-Module (Join-Path $ProjectRoot 'cameraunlock-core\powershell\NightlyRelease.psm1') -Force

$versionHeader = Join-Path $ProjectRoot 'src\version.h'
$versionMatch = Select-String -Path $versionHeader -Pattern 'HEADTRACKING_VERSION_STRING\s+"([^"]+)"'
if (-not $versionMatch) {
    throw "Could not extract HEADTRACKING_VERSION_STRING from $versionHeader"
}
$version = $versionMatch.Matches[0].Groups[1].Value

Publish-NightlyBuild `
    -ModId 'portal-with-rtx' `
    -ModName 'PortalWithRTXHeadTracking' `
    -Version $version `
    -ProjectRoot $ProjectRoot `
    -AllowDirty:$AllowDirty
