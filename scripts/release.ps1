#!/usr/bin/env pwsh
#Requires -Version 5.1
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 itsloopyo / CameraUnlock
<#
.SYNOPSIS
    Release workflow for Portal with RTX Head Tracking.

.DESCRIPTION
    1. Resolve and validate the version (semver, or major|minor|patch).
    2. Check the git preconditions: on main, clean tree, tag absent.
    3. Regenerate CHANGELOG.md from the commits since the last tag.
    4. Bump src/version.h, and mirror it into scripts/install.cmd and
       CMakeLists.txt.
    5. Build the x86 release.
    6. Commit "Release v<version>", tag it, push main then the tag. The tag
       starts .github/workflows/release.yml, which builds and publishes.

    Runs unattended: the command line is the authorization, so there is no
    confirmation step. Every precondition either passes or exits 1 with the
    reason.

.EXAMPLE
    pixi run release 1.0.0
    pixi run release patch
#>
param(
    [Parameter(Position = 0)]
    [string]$Version = '',
    # Ship a release even when every commit since the last tag was filtered as
    # noise (writes a maintenance changelog entry instead of aborting).
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectDir     = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$versionPath    = Join-Path $projectDir 'src\version.h'
$installCmdPath = Join-Path $projectDir 'scripts\install.cmd'
$cmakePath      = Join-Path $projectDir 'CMakeLists.txt'
$changelogPath  = Join-Path $projectDir 'CHANGELOG.md'

Import-Module (Join-Path $projectDir 'cameraunlock-core\powershell\ReleaseWorkflow.psm1') -Force

# THIRD-PARTY-NOTICES.md names the cameraunlock-core commit compiled into the
# release ZIPs, and bumping the submodule does not touch it. Packaging refuses
# to ship that mismatch, so a bump with no notices edit stops the release here
# rather than in CI with the tag already pushed.
#
# Called only after the preconditions below have passed: it writes a commit,
# and a commit must never land off the back of an invocation that then aborts
# for a dirty tree, the wrong branch or an existing tag.
function Sync-CoreNotices {
    & git -C $projectDir diff --quiet -- THIRD-PARTY-NOTICES.md
    if ($LASTEXITCODE -ne 0) { throw "THIRD-PARTY-NOTICES.md has uncommitted edits. Commit or discard them, then re-run." }
    & (Join-Path $projectDir 'cameraunlock-core\scripts\sync-core-notices.ps1') -Repo $projectDir
    if ($LASTEXITCODE -ne 0) { throw "sync-core-notices.ps1 exited $LASTEXITCODE - fix THIRD-PARTY-NOTICES.md before releasing." }
    & git -C $projectDir diff --quiet -- THIRD-PARTY-NOTICES.md
    if ($LASTEXITCODE -ne 0) {
        & git -C $projectDir commit -q -m 'chore: record the cameraunlock-core commit this build compiles' -- THIRD-PARTY-NOTICES.md
        if ($LASTEXITCODE -ne 0) { throw "Could not commit the re-synced THIRD-PARTY-NOTICES.md." }
        Write-Host 'THIRD-PARTY-NOTICES.md re-synced to the pinned cameraunlock-core commit.' -ForegroundColor Yellow
    }
}

function Get-ModVersion {
    $content = Get-Content $versionPath -Raw
    if ($content -match 'HEADTRACKING_VERSION_STRING\s+"([^"]+)"') { return $Matches[1] }
    throw "Could not read HEADTRACKING_VERSION_STRING from $versionPath"
}

function Set-ModVersion {
    param([string]$NewVersion)
    $parts = $NewVersion.Split('.')
    # ReadAllText/WriteAllText so the file's existing line endings survive.
    $raw = [System.IO.File]::ReadAllText($versionPath)
    $raw = [regex]::Replace($raw, '(?m)^(#define HEADTRACKING_VERSION_MAJOR\s+)\d+', "`${1}$($parts[0])")
    $raw = [regex]::Replace($raw, '(?m)^(#define HEADTRACKING_VERSION_MINOR\s+)\d+', "`${1}$($parts[1])")
    $raw = [regex]::Replace($raw, '(?m)^(#define HEADTRACKING_VERSION_PATCH\s+)\d+', "`${1}$($parts[2])")
    $raw = [regex]::Replace($raw, 'HEADTRACKING_VERSION_STRING\s+"[^"]+"', "HEADTRACKING_VERSION_STRING `"$NewVersion`"")
    [System.IO.File]::WriteAllText($versionPath, $raw)
}

# Mirrors New-ChangelogFromCommits' insertion so every writer of this file puts
# its entry in the same place: directly under the "# Changelog" heading, above
# the entries already there.
#
# ReadAllText/WriteAllText with a BOM-less UTF8Encoding because PS 5.1 defaults
# Get-Content/Set-Content to the system ANSI codepage, which mangles any
# non-ASCII already in the file on every round trip.
function Add-ChangelogEntry {
    param([string]$Path, [string]$Entry)
    $changelog = [System.IO.File]::ReadAllText($Path)
    $anchor = '(?s)(# Changelog.*?\r?\n\r?\n)'
    if ($changelog -notmatch $anchor) {
        throw "$Path has no '# Changelog' heading to insert the new entry under."
    }
    $changelog = $changelog -replace $anchor, "`$1$Entry"
    $changelog = $changelog.TrimEnd() + "`n"
    [System.IO.File]::WriteAllText($Path, $changelog, (New-Object System.Text.UTF8Encoding $false))
}

function Add-MaintenanceChangelogEntry {
    param([string]$Path, [string]$NewVersion)
    $date = Get-Date -Format 'yyyy-MM-dd'
    Add-ChangelogEntry -Path $Path -Entry "## [$NewVersion] - $date`n`n### Changed`n`n- Maintenance release (no user-facing changes).`n`n"
}

Write-Host ''
Write-Host '=== Portal with RTX Head Tracking Release ===' -ForegroundColor Cyan
Write-Host ''

$current = Get-ModVersion

if ([string]::IsNullOrWhiteSpace($Version)) {
    Write-Host "Current version: $current" -ForegroundColor Yellow
    Write-Host 'Usage: pixi run release <major|minor|patch|nightly|X.Y.Z>'
    exit 0
}

if ($Version -eq 'nightly') {
    & (Join-Path $PSScriptRoot 'release-nightly.ps1')
    exit $LASTEXITCODE
}

try {
    $Version = Resolve-ReleaseVersion -Argument $Version -CurrentVersion $current
} catch {
    Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

$tag = "v$Version"

$branch = git rev-parse --abbrev-ref HEAD
if ($branch -ne 'main') {
    Write-Host "Must be on main branch to release (currently on '$branch')" -ForegroundColor Red
    exit 1
}
if (-not (Test-CleanGitStatus)) {
    Write-Host 'Working tree has uncommitted changes - commit or stash first.' -ForegroundColor Red
    git status --short
    exit 1
}
if (Test-GitTagExists -Tag $tag) {
    Write-Host "Tag '$tag' already exists." -ForegroundColor Red
    exit 1
}

Sync-CoreNotices

Write-Host "Current version: $current" -ForegroundColor Gray
Write-Host "New version:     $Version" -ForegroundColor Green
Write-Host ''

# Step 1 - changelog first, because it is the gate that can fail. Generating it
# before mutating any version file means an abort leaves the tree clean rather
# than stranding a half-applied bump with no tag.
Write-Host 'Generating CHANGELOG from commits...' -ForegroundColor Cyan
$hasTags = git tag -l 2>$null
if (-not $hasTags) {
    # No previous tag, so there is no commit range to generate an entry from.
    # Prepend rather than overwrite: up to the first tag this file is written by
    # hand, and it carries the history of everything built before the release.
    $date = Get-Date -Format 'yyyy-MM-dd'
    $entry = "## [$Version] - $date`n`nFirst release.`n`n"
    if (Test-Path $changelogPath) {
        Add-ChangelogEntry -Path $changelogPath -Entry $entry
    } else {
        [System.IO.File]::WriteAllText($changelogPath, "# Changelog`n`n$entry", (New-Object System.Text.UTF8Encoding $false))
    }
} else {
    try {
        New-ChangelogFromCommits -ChangelogPath $changelogPath -Version $Version `
            -ArtifactPaths @('src/', 'cameraunlock-core', 'scripts/')
    } catch {
        if (-not $Force) {
            Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
            Write-Host 'No user-facing changes to release. Re-run with -Force for a maintenance release.' -ForegroundColor Yellow
            exit 1
        }
        Write-Host 'No user-facing commits since last tag - writing maintenance entry (-Force).' -ForegroundColor Yellow
        Add-MaintenanceChangelogEntry -Path $changelogPath -NewVersion $Version
    }
}

# Step 2 - src/version.h is the canonical version: the packager reads it to
# name the ZIPs and release.yml reads it to name the GitHub release. The other
# two copies are written here so they cannot drift away from it -
# install.cmd's MOD_VERSION, which the installer records in the user's
# .headtracking-state.json, and CMakeLists.txt's project version, which nothing
# reads but which is the first place a reader looks.
Write-Host "Updating src/version.h to $Version..." -ForegroundColor Cyan
Set-ModVersion -NewVersion $Version

Write-Host "Updating scripts/install.cmd MOD_VERSION to $Version..." -ForegroundColor Cyan
$installRaw = [System.IO.File]::ReadAllText($installCmdPath)
if ($installRaw -notmatch 'set "MOD_VERSION=[^"]+"') {
    throw "MOD_VERSION line not found in $installCmdPath"
}
$installRaw = [regex]::Replace($installRaw, 'set "MOD_VERSION=[^"]+"', "set `"MOD_VERSION=$Version`"")
[System.IO.File]::WriteAllText($installCmdPath, $installRaw)

Write-Host "Updating CMakeLists.txt project version to $Version..." -ForegroundColor Cyan
$cmakeRaw = [System.IO.File]::ReadAllText($cmakePath)
if ($cmakeRaw -notmatch '(?m)^(project\([^)]*VERSION\s+)\d+\.\d+\.\d+') {
    throw "project(... VERSION x.y.z) line not found in $cmakePath"
}
$cmakeRaw = [regex]::Replace($cmakeRaw, '(?m)^(project\([^)]*VERSION\s+)\d+\.\d+\.\d+', "`${1}$Version")
[System.IO.File]::WriteAllText($cmakePath, $cmakeRaw)

# Step 3 - build the artifact this release ships.
Write-Host 'Building release (x86)...' -ForegroundColor Cyan
Push-Location $projectDir
try {
    pixi run build-release
    if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
} finally {
    Pop-Location
}

# Step 4 - commit named files only, so build artifacts cannot sweep in.
Write-Host 'Committing version + changelog...' -ForegroundColor Cyan
git add $versionPath $changelogPath $installCmdPath $cmakePath
if ($LASTEXITCODE -ne 0) { throw 'git add failed - the version bump was not staged.' }
git diff --cached --quiet
if ($LASTEXITCODE -eq 0) {
    Write-Host 'No version/changelog changes - tagging existing HEAD.' -ForegroundColor Yellow
} else {
    git commit -m "Release v$Version"
    if ($LASTEXITCODE -ne 0) { throw 'Commit failed' }
}

# Step 5 - tag + push. Every step is checked, because git failures are exit
# codes rather than exceptions and $ErrorActionPreference does not see them.
# The push order is load-bearing: pushing the tag first, or pushing it after a
# main push that was rejected (a non-fast-forward, a protected branch), lands a
# release tag on a commit that is not on main - and the tag push carries the
# commit's objects with it, so CI happily builds and publishes from it.
Write-Host "Creating tag $tag..." -ForegroundColor Cyan
git tag -a $tag -m "Release $tag"
if ($LASTEXITCODE -ne 0) { throw "Could not create tag $tag." }
git push origin main
if ($LASTEXITCODE -ne 0) { throw "Pushing main failed - the local tag $tag was NOT pushed. Fix the push, then run: git push origin main; git push origin $tag" }
git push origin $tag
if ($LASTEXITCODE -ne 0) { throw "Pushing tag $tag failed - main is pushed, so CI has not been triggered. Re-run: git push origin $tag" }

Write-Host ''
Write-Host "Release $tag pushed - CI will build and publish artifacts." -ForegroundColor Green
