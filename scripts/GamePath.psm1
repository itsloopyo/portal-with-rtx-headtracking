#Requires -Version 5.1
# Where Portal with RTX is installed.
#
# Self-contained rather than routed through cameraunlock-core's
# GamePathDetection.psm1: that module resolves a game id out of the core's
# games.json, and this game does not have an entry there yet. Adding one is a
# change to a different repository; this is a dev script.

Set-StrictMode -Version Latest

function Get-PortalWithRtxPath {
    [CmdletBinding()]
    param([string]$GivenPath)

    $candidates = New-Object System.Collections.Generic.List[string]
    if ($GivenPath) { $candidates.Add($GivenPath) }
    if ($env:PORTAL_WITH_RTX_PATH) { $candidates.Add($env:PORTAL_WITH_RTX_PATH) }

    foreach ($steam in @("${env:ProgramFiles(x86)}\Steam", "$env:ProgramFiles\Steam")) {
        $vdf = Join-Path $steam 'steamapps\libraryfolders.vdf'
        if (-not (Test-Path $vdf)) { continue }
        foreach ($line in Get-Content $vdf) {
            if ($line -match '"path"\s+"(.+?)"') {
                $library = $Matches[1] -replace '\\\\', '\'
                $candidates.Add((Join-Path $library 'steamapps\common\PortalRTX'))
            }
        }
    }

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path (Join-Path $candidate 'hl2.exe'))) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw ("Could not find Portal with RTX. Pass the folder containing hl2.exe as an " +
           "argument, or set PORTAL_WITH_RTX_PATH.")
}

Export-ModuleMember -Function Get-PortalWithRtxPath
