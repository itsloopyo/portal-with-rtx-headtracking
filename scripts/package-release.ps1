#!/usr/bin/env pwsh
#Requires -Version 5.1
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 itsloopyo / CameraUnlock
<#
.SYNOPSIS
    Builds the two release ZIPs for Portal with RTX Head Tracking.

.DESCRIPTION
    Installer ZIP (GitHub release, consumed by install.cmd and by Lopari):
      install.cmd, uninstall.cmd, shared/, plugins/, vendor/, docs.

    Nexus ZIP (extract over the game folder): the deploy subtree only -
    bin\PortalWithRTXHeadTracking.asi plus the notices that have to travel
    with a binary distribution.

    Runs unattended. Every failure is a throw with the fix in the message.

.EXAMPLE
    pixi run package
#>
param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Release')

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$binDir   = Join-Path $repoRoot "bin\$Configuration"
$outDir   = Join-Path $repoRoot 'release'

$modName = 'PortalWithRTXHeadTracking'
$modSlug = 'portal-with-rtx-headtracking'
$asi     = 'PortalWithRTXHeadTracking.asi'

# src/version.h is the canonical version: release.ps1 writes it first and
# mirrors it into install.cmd and CMakeLists.txt, and release.yml reads the
# same regex out of it to name the GitHub release.
$versionHeader = Join-Path $repoRoot 'src\version.h'
$match = (Select-String -Path $versionHeader -Pattern 'HEADTRACKING_VERSION_STRING\s+"([^"]+)"').Matches
if (-not $match) { throw "Could not parse HEADTRACKING_VERSION_STRING from $versionHeader" }
$version = $match[0].Groups[1].Value

$asiPath = Join-Path $binDir $asi
if (-not (Test-Path $asiPath)) { throw "Missing build output: $asiPath. Run: pixi run build-release" }

$vendorDir = Join-Path $repoRoot 'vendor\ultimate-asi-loader'
if (-not (Test-Path (Join-Path $vendorDir 'dinput8.dll'))) {
    throw "Missing vendored loader: $vendorDir\dinput8.dll. Run: pixi run update-deps"
}

# The installer ZIP redistributes that binary, and the upstream x86 loader
# carries binkw32.dll (RAD Game Tools, proprietary), wndmode.dll and
# vorbisfile.dll as RCDATA resources. None of the three is ours to ship, so a
# loader that still has them never reaches a release. See
# vendor/ultimate-asi-loader/README.md.
& (Join-Path $PSScriptRoot 'strip-loader-payload.ps1') -Path (Join-Path $vendorDir 'dinput8.dll') -VerifyOnly

Import-Module (Join-Path $repoRoot 'cameraunlock-core\powershell\ReleaseWorkflow.psm1') -Force

if (-not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir | Out-Null }

# ---------- Installer ZIP ----------
$installerStage = Join-Path $outDir "$modSlug-installer-stage"
if (Test-Path $installerStage) { Remove-Item $installerStage -Recurse -Force }
New-Item -ItemType Directory -Path $installerStage | Out-Null

# install.cmd copies plugins\ into <game>\bin. HeadTracking.ini is not shipped:
# the mod writes its own defaults next to hl2.exe on first launch, and a seeded
# copy would reset whatever the user tuned on every update.
$pluginsDir = Join-Path $installerStage 'plugins'
New-Item -ItemType Directory -Path $pluginsDir | Out-Null
Copy-Item $asiPath $pluginsDir

# Vendored Ultimate ASI Loader, consumed exactly as committed - refreshing it
# belongs to `pixi run update-deps`, which has a reviewed diff behind it.
# dinput8.dll and LICENSE are both mandatory: the loader is MIT and its notice
# has to travel with the binary. README.md is the vendoring provenance note.
$vendorStage = Join-Path $installerStage 'vendor\ultimate-asi-loader'
New-Item -ItemType Directory -Path $vendorStage | Out-Null
foreach ($f in @('dinput8.dll', 'LICENSE')) {
    $src = Join-Path $vendorDir $f
    if (-not (Test-Path $src)) { throw "Missing vendored loader file: $src" }
    Copy-Item $src $vendorStage
}
$vendorReadme = Join-Path $vendorDir 'README.md'
if (Test-Path $vendorReadme) { Copy-Item $vendorReadme $vendorStage }

# cmd.exe needs CRLF: an LF-only batch file mis-parses labels and parenthesised
# blocks, and both wrappers are built out of those, so the failure is an
# installer that exits having done nothing. .gitattributes marks these
# eol=crlf, but the packager copies from the WORKING TREE, which is what a file
# written by an LF-defaulting tool ships as. Checked rather than repaired,
# because a wrapper that reached this point with LF endings was edited by
# something that will do it again.
foreach ($script in @('install.cmd', 'uninstall.cmd')) {
    $path = Join-Path $repoRoot "scripts\$script"
    if ([IO.File]::ReadAllText($path) -notmatch "`r`n") {
        throw "scripts\$script has LF line endings. cmd.exe needs CRLF - run: unix2dos scripts/$script"
    }
    Copy-Item $path $installerStage
}

# shared/: the install and uninstall bodies both wrappers call, plus the game
# detection the bodies use. Their dev-tree fallback (..\cameraunlock-core) does
# not exist inside a release ZIP, so this is load-bearing, not documentation.
Copy-SharedBundle -StagingDir $installerStage

# The launcher reads launcher-manifest.json from the ZIP root to route the
# install, and it is the contract between this package and Lopari. A missing one
# is fatal rather than a warning: the ZIP still builds, still passes CI's
# "an installer ZIP exists" check, and is rejected by the launcher at install
# time, which is the furthest possible point from the mistake.
$manifestPath = Join-Path $repoRoot 'launcher-manifest.json'
if (-not (Test-Path $manifestPath)) {
    throw "Missing launcher-manifest.json at the repo root. The launcher cannot install a package without it - see the declare-a-launcher-package skill for the schema."
}
$manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
$manifest.mod_info.version = $version

# Every declared source has to exist in the staged tree. A manifest naming a
# path the packager never produced deploys nothing and reports success, which is
# how sibling mods shipped broken. The loader block is walked too even though
# this mod has none today: an ASI mod references its loader through files[], but
# a seed or an archive added later would otherwise slip past a check whose
# comment says it covers everything.
#
# cameraunlock-core/scripts/validate-manifest.mjs is the canonical version of
# this check and covers more; it is not invoked here because it needs node, and
# packaging must work on a clean checkout with only the C++ toolchain.
function Test-ManifestSources {
    param($Entries, [string]$Label, [string]$StageDir)
    if (-not $Entries) { return }
    foreach ($entry in $Entries) {
        $source = $entry.PSObject.Properties['source']
        if (-not $source -or [string]::IsNullOrWhiteSpace($source.Value)) {
            throw "launcher-manifest.json has a $Label entry with no 'source'. Every deployed file is declared explicitly."
        }
        $staged = Join-Path $StageDir ($source.Value.Replace('/', '\'))
        if (-not (Test-Path $staged)) {
            throw "launcher-manifest.json declares $Label source '$($source.Value)', which the packager did not stage. Fix the manifest or the staging step."
        }
    }
}

if (-not $manifest.PSObject.Properties['files'] -or -not $manifest.files) {
    throw "launcher-manifest.json declares no files[]. A manifest-mode package that deploys nothing installs nothing."
}
Test-ManifestSources -Entries $manifest.files -Label 'files[]' -StageDir $installerStage
if ($manifest.PSObject.Properties['loader'] -and $manifest.loader) {
    if ($manifest.loader.PSObject.Properties['archives']) {
        Test-ManifestSources -Entries $manifest.loader.archives -Label 'loader.archives[]' -StageDir $installerStage
    }
}

# WriteAllText with a BOM-less encoder, not Set-Content -Encoding utf8:
# PS 5.1's utf8 emits a BOM and utf8NoBOM does not exist there.
[IO.File]::WriteAllText(
    (Join-Path $installerStage 'launcher-manifest.json'),
    ($manifest | ConvertTo-Json -Depth 10),
    (New-Object System.Text.UTF8Encoding $false))

# LICENSE and THIRD-PARTY-NOTICES.md are not optional documentation: the
# vendored loader's MIT and MinHook's BSD-2-Clause both require their notice to
# accompany these binaries. Copy-LicenceNotices throws on a missing one.
Copy-LicenceNotices -StagingDir $installerStage -ProjectRoot $repoRoot
foreach ($doc in @('README.md', 'CHANGELOG.md')) {
    $src = Join-Path $repoRoot $doc
    if (Test-Path $src) { Copy-Item $src $installerStage }
}

$installerZip = Join-Path $outDir "$modName-v$version-installer.zip"
if (Test-Path $installerZip) { Remove-Item $installerZip -Force }
Compress-Archive -Path "$installerStage\*" -DestinationPath $installerZip
Remove-Item $installerStage -Recurse -Force
Write-Host "Packaged installer: $installerZip" -ForegroundColor Green

# ---------- Nexus ZIP ----------
# Deploy subtree only. bin\, not the folder holding hl2.exe: Source loads
# bin\launcher.dll with LOAD_WITH_ALTERED_SEARCH_PATH, so bin\ is where the
# proxy DLL and everything downstream of it are searched for. Nexus users bring
# their own ASI loader, so none is bundled here.
$nexusStage = Join-Path $outDir "$modSlug-nexus-stage"
if (Test-Path $nexusStage) { Remove-Item $nexusStage -Recurse -Force }
$nexusBin = Join-Path $nexusStage 'bin'
New-Item -ItemType Directory -Path $nexusBin -Force | Out-Null
Copy-Item $asiPath $nexusBin

# The .asi statically links MinHook (BSD-2-Clause) and cameraunlock-core (MIT).
# This ZIP is a binary distribution in its own right, so the notices ship in it
# too, at the root rather than scattered into the engine's own directories.
Copy-LicenceNotices -StagingDir $nexusStage -ProjectRoot $repoRoot
$nexusReadme = Join-Path $repoRoot 'README.md'
if (Test-Path $nexusReadme) { Copy-Item $nexusReadme $nexusStage }

$nexusZip = Join-Path $outDir "$modName-v$version-nexus.zip"
if (Test-Path $nexusZip) { Remove-Item $nexusZip -Force }
Compress-Archive -Path "$nexusStage\*" -DestinationPath $nexusZip
Remove-Item $nexusStage -Recurse -Force
Write-Host "Packaged nexus:     $nexusZip" -ForegroundColor Green
