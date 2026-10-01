#Requires -Version 5.1
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot '..\cameraunlock-core\powershell\GamePathDetection.psm1')

function Get-PortalWithRtxPath {
    [CmdletBinding()]
    param([string]$GivenPath)

    $config = Get-GameConfig -GameId 'portal-with-rtx'
    if ($GivenPath) {
        if (Test-GameInstallation -Path $GivenPath -Executable $config.Executable) {
            return (Resolve-Path -LiteralPath $GivenPath).Path
        }
    }

    $gamePath = Find-GamePath -Config $config
    if ($gamePath) {
        return (Resolve-Path -LiteralPath $gamePath).Path
    }

    throw ("Could not find Portal with RTX. Pass the folder containing hl2.exe as an " +
           "argument, or set PORTAL_WITH_RTX_PATH.")
}

Export-ModuleMember -Function Get-PortalWithRtxPath
