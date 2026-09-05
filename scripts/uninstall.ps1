#!/usr/bin/env pwsh
#Requires -Version 5.1
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 itsloopyo / CameraUnlock
# Removes the dev-deployed .asi. The ASI loader is left in place: another mod
# may be using the same winmm.dll slot, and it is inert on its own.

param([Parameter(Position = 0)][string]$GivenPath)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Import-Module (Join-Path $PSScriptRoot 'GamePath.psm1') -Force

$binDir = Join-Path (Get-PortalWithRtxPath -GivenPath $GivenPath) 'bin'
foreach ($name in @('PortalWithRTXHeadTracking.asi')) {
    $path = Join-Path $binDir $name
    if (Test-Path $path) {
        Remove-Item $path -Force
        Write-Host "Removed $path" -ForegroundColor Green
    } else {
        Write-Host "Not present: $path" -ForegroundColor Yellow
    }
}
