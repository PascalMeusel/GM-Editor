#requires -Version 5.1

[CmdletBinding(PositionalBinding = $false)]
param([string]$DraftDirectory = "marketing/steam/draft")

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.Drawing

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$root = if ([System.IO.Path]::IsPathRooted($DraftDirectory)) {
    [System.IO.Path]::GetFullPath($DraftDirectory)
} else {
    [System.IO.Path]::GetFullPath((Join-Path $projectRoot $DraftDirectory))
}
if (-not (Test-Path -LiteralPath $root -PathType Container)) {
    throw "Marketing draft directory is missing: $root"
}

$expected = [ordered]@{
    "store-header-920x430.png"       = @(920, 430)
    "store-small-462x174.png"        = @(462, 174)
    "store-main-1232x706.png"        = @(1232, 706)
    "store-vertical-748x896.png"     = @(748, 896)
    "library-capsule-600x900.png"    = @(600, 900)
    "library-header-920x430.png"     = @(920, 430)
    "library-hero-clean-3840x1240.png" = @(3840, 1240)
    "library-logo-1280x336.png"      = @(1280, 336)
    "shortcut-icon-512.png"          = @(512, 512)
    "app-icon-184.jpg"               = @(184, 184)
    "press-key-art-3840x2160.png"    = @(3840, 2160)
    "social-share-1920x1080.png"     = @(1920, 1080)
    "store-page-background-1438x810.png" = @(1438, 810)
    "event-cover-800x450.png"        = @(800, 450)
    "event-header-1920x622.png"      = @(1920, 622)
}

foreach ($entry in $expected.GetEnumerator()) {
    $path = Join-Path $root $entry.Key
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required Steam draft is missing: $($entry.Key)"
    }
    $image = [System.Drawing.Image]::FromFile($path)
    try {
        if ($image.Width -ne $entry.Value[0] -or $image.Height -ne $entry.Value[1]) {
            throw "Wrong dimensions for $($entry.Key): $($image.Width)x$($image.Height), expected $($entry.Value[0])x$($entry.Value[1])."
        }
    }
    finally {
        $image.Dispose()
    }
}

$unexpected = @(Get-ChildItem -LiteralPath $root -File | Where-Object { -not $expected.Contains($_.Name) })
if ($unexpected.Count -gt 0) {
    throw "Unexpected file in the exact-size draft set: $($unexpected[0].Name)"
}

$hashLines = foreach ($name in ($expected.Keys | Sort-Object)) {
    $path = Join-Path $root $name
    $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $name"
}
$hashPath = Join-Path (Split-Path -Parent $root) "DRAFT_SHA256SUMS.txt"
[System.IO.File]::WriteAllLines($hashPath, @($hashLines), (New-Object System.Text.UTF8Encoding($false)))

Write-Host "Steam marketing draft dimensions verified: $($expected.Count) files."
Write-Host "Checksums: $hashPath"
