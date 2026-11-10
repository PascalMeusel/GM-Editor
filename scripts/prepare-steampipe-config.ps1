#requires -Version 5.1

[CmdletBinding(PositionalBinding = $false)]
param(
    [Parameter(Mandatory = $true)][ValidateRange(1, 2147483647)][int]$AppId,
    [Parameter(Mandatory = $true)][ValidateRange(1, 2147483647)][int]$DepotId,
    [string]$DepotRoot = "dist/steam/windows-x64/GMEditor-Depot",
    [string]$OutputDirectory = "dist/steam/steampipe/scripts",
    [switch]$UploadConfiguration
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if ($AppId -eq $DepotId) {
    throw "AppId and DepotId must be different Steamworks identifiers."
}

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
function Resolve-ProjectPath([string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $projectRoot $Path))
}

function Test-IsSameOrChildPath([string]$Child, [string]$Parent) {
    $normalizedChild = [System.IO.Path]::GetFullPath($Child).TrimEnd('\', '/')
    $normalizedParent = [System.IO.Path]::GetFullPath($Parent).TrimEnd('\', '/')
    if ($normalizedChild.Equals($normalizedParent, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $true
    }
    return $normalizedChild.StartsWith(
        $normalizedParent + [System.IO.Path]::DirectorySeparatorChar,
        [System.StringComparison]::OrdinalIgnoreCase)
}

$resolvedDepot = Resolve-ProjectPath $DepotRoot
$resolvedOutput = Resolve-ProjectPath $OutputDirectory
$fixedSteamRoot = [System.IO.Path]::GetFullPath((Join-Path $projectRoot "dist/steam")).TrimEnd('\', '/')
$fixedPrefix = $fixedSteamRoot + [System.IO.Path]::DirectorySeparatorChar
if (-not $resolvedDepot.StartsWith($fixedPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "DepotRoot must remain below the repository's dist/steam directory: $resolvedDepot"
}
if (-not $resolvedOutput.StartsWith($fixedPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "OutputDirectory must remain below the repository's dist/steam directory: $resolvedOutput"
}
if (-not (Test-Path -LiteralPath $resolvedDepot -PathType Container)) {
    throw "Validated depot is missing: $resolvedDepot"
}
if (Test-IsSameOrChildPath -Child $resolvedOutput -Parent $resolvedDepot) {
    throw "OutputDirectory must not be inside the validated depot: $resolvedOutput"
}

& (Join-Path $PSScriptRoot "assert-steam-depot.ps1") -DepotRoot $resolvedDepot

$buildOutput = [System.IO.Path]::GetFullPath((Join-Path $resolvedOutput "../output"))
if (-not $buildOutput.StartsWith($fixedPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "SteamPipe BuildOutput must remain below the repository's dist/steam directory: $buildOutput"
}
if (Test-IsSameOrChildPath -Child $buildOutput -Parent $resolvedDepot) {
    throw "SteamPipe BuildOutput must not be inside the validated depot: $buildOutput"
}
[System.IO.Directory]::CreateDirectory($resolvedOutput) | Out-Null
[System.IO.Directory]::CreateDirectory($buildOutput) | Out-Null

# Steam accepts absolute ContentRoot and BuildOutput values.  Emit absolute
# paths so copying both VDF files into the SDK scripts directory cannot silently
# retarget the upload to a different relative directory.
$contentRootValue = $resolvedDepot.Replace('\', '/')
$buildOutputValue = $buildOutput.Replace('\', '/')
$previewValue = if ($UploadConfiguration) { "0" } else { "1" }

$depotFileName = "depot_build_$DepotId.vdf"
$appFileName = "app_build_$AppId.vdf"
$depotText = @"
"DepotBuild"
{
    "DepotID" "$DepotId"
    "FileMapping"
    {
        "LocalPath" "*"
        "DepotPath" "."
        "Recursive" "1"
    }
    "FileExclusion" "*.pdb"
    "FileExclusion" "*.ilk"
    "FileExclusion" "*.exp"
    "FileExclusion" "*.lib"
    "FileExclusion" "*.log"
    "FileExclusion" "*.db"
    "FileExclusion" "*.tmp"
}
"@
$appText = @"
"AppBuild"
{
    "AppID" "$AppId"
    "Desc" "GM Editor 0.9 Windows x64 release candidate"
    "Preview" "$previewValue"
    "SetLive" ""
    "ContentRoot" "$contentRootValue"
    "BuildOutput" "$buildOutputValue"
    "Depots"
    {
        "$DepotId" "$depotFileName"
    }
}
"@

$utf8 = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText((Join-Path $resolvedOutput $depotFileName), $depotText, $utf8)
[System.IO.File]::WriteAllText((Join-Path $resolvedOutput $appFileName), $appText, $utf8)

$mode = if ($UploadConfiguration) { "UPLOAD-ENABLED CONFIG (no upload was executed)" } else { "PREVIEW ONLY" }
Write-Host "SteamPipe VDF files created: $mode"
Write-Host "App:   $(Join-Path $resolvedOutput $appFileName)"
Write-Host "Depot: $(Join-Path $resolvedOutput $depotFileName)"
Write-Host "ContentRoot: $resolvedDepot"
if ($UploadConfiguration) {
    Write-Warning "Review the generated mapping and run a preview first. This script never invokes steamcmd."
}
