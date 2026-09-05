#!/usr/bin/env pwsh
#Requires -Version 5.1
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 itsloopyo / CameraUnlock
# Dev deploy: puts the freshly built .asi and the vendored ASI loader into the
# game's bin folder.
#
# bin, not the folder holding hl2.exe. Source's launcher loads bin\launcher.dll
# with LOAD_WITH_ALTERED_SEARCH_PATH, which makes bin the first directory
# searched for everything downstream of it - including tier0.dll's winmm.dll
# import, which is the slot the loader takes over. A copy next to hl2.exe is
# never looked at.

param(
    [Parameter(Position = 0)]
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [Parameter(Position = 1)]
    [string]$GivenPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir   = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $scriptDir
Import-Module (Join-Path $scriptDir 'GamePath.psm1') -Force

$gameDir = Get-PortalWithRtxPath -GivenPath $GivenPath
$binDir  = Join-Path $gameDir 'bin'

$asi = Join-Path $projectRoot "bin\$Configuration\PortalWithRTXHeadTracking.asi"
if (-not (Test-Path $asi)) { throw "Not built: $asi" }

$loaderTarget = Join-Path $binDir 'winmm.dll'
if (-not (Test-Path $loaderTarget)) {
    Copy-Item (Join-Path $projectRoot 'vendor\ultimate-asi-loader\dinput8.dll') $loaderTarget
    Write-Host "Installed ASI loader: $loaderTarget" -ForegroundColor Green
} else {
    Write-Host "ASI loader already present: $loaderTarget" -ForegroundColor Yellow
}

Copy-Item $asi (Join-Path $binDir 'PortalWithRTXHeadTracking.asi') -Force
Write-Host "Deployed PortalWithRTXHeadTracking.asi to: $binDir" -ForegroundColor Green
Write-Host "Controls: End=toggle tracking, PgUp=cycle 6DOF/rotation/position, PgDn=toggle yaw mode." -ForegroundColor Gray
