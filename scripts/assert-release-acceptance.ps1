#requires -Version 5.1

[CmdletBinding(PositionalBinding = $false)]
param(
    [Parameter(Mandatory = $true)][string]$ManualRunRoot,
    [Parameter(Mandatory = $true)][string]$VisualRunRoot,
    [Parameter(Mandatory = $true)][string]$ExpectedSourceCommit,
    [Parameter(Mandatory = $true)][string]$OutputReport
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if ($ExpectedSourceCommit -notmatch '^[0-9a-fA-F]{40}$') {
    throw "ExpectedSourceCommit must be a full 40-character commit SHA."
}

$manualRoot = [IO.Path]::GetFullPath($ManualRunRoot).TrimEnd('\', '/')
$visualRoot = [IO.Path]::GetFullPath($VisualRunRoot).TrimEnd('\', '/')
$reportPath = [IO.Path]::GetFullPath($OutputReport)
$reportSidecar = "$reportPath.sha256"
if ((Test-Path -LiteralPath $reportPath) -or (Test-Path -LiteralPath $reportSidecar)) {
    throw "Acceptance report or sidecar already exists: $reportPath"
}

$manualManifestPath = Join-Path $manualRoot "manual-qa-run.json"
$manualResultsPath = Join-Path $manualRoot "manual-qa-results.csv"
$visualManifestPath = Join-Path $visualRoot "visual-smoke-run.json"
$visualQueuePath = Join-Path $visualRoot "capture-queue.csv"
foreach ($required in @($manualManifestPath, $manualResultsPath,
                        $visualManifestPath, $visualQueuePath)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required release-acceptance evidence is missing: $required"
    }
}

# The existing validators enforce immutable case definitions, named reviewers,
# timestamps, evidence hashes, required review checks, and complete pass states.
& (Join-Path $PSScriptRoot "assert-release-manual-qa.ps1") `
    -RunRoot $manualRoot -Priority All -RequireComplete -RequirePass
& (Join-Path $PSScriptRoot "assert-release-visual-smoke.ps1") `
    -RunRoot $visualRoot -Priority All -RequireComplete -RequirePass

$manual = Get-Content -LiteralPath $manualManifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
$visual = Get-Content -LiteralPath $visualManifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
$sourceStatus = @($visual.sourceStatus)
if ($sourceStatus.Count -ne 0) {
    throw "Visual evidence was prepared from a dirty source tree."
}
if (-not ([string]$visual.sourceCommit).Equals(
        $ExpectedSourceCommit, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Visual evidence belongs to commit '$($visual.sourceCommit)', not '$ExpectedSourceCommit'."
}
if (-not ([string]$manual.executableSha256).Equals(
        [string]$visual.editorSha256, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Manual and visual evidence are bound to different editor binaries."
}

$manualRows = @(Import-Csv -LiteralPath $manualResultsPath)
$visualRows = @(Import-Csv -LiteralPath $visualQueuePath)
$report = [PSCustomObject][ordered]@{
    schemaVersion = 1
    generatedAtUtc = [DateTime]::UtcNow.ToString("o")
    sourceCommit = $ExpectedSourceCommit.ToLowerInvariant()
    editorSha256 = ([string]$manual.executableSha256).ToLowerInvariant()
    manualQa = [PSCustomObject][ordered]@{
        runId = [string]$manual.runId
        caseCount = $manualRows.Count
        manifestSha256 = (Get-FileHash -LiteralPath $manualManifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
        resultsSha256 = (Get-FileHash -LiteralPath $manualResultsPath -Algorithm SHA256).Hash.ToLowerInvariant()
        allPassed = $true
    }
    visualSmoke = [PSCustomObject][ordered]@{
        runId = [string]$visual.runId
        captureCount = $visualRows.Count
        manifestSha256 = (Get-FileHash -LiteralPath $visualManifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
        queueSha256 = (Get-FileHash -LiteralPath $visualQueuePath -Algorithm SHA256).Hash.ToLowerInvariant()
        allPassed = $true
    }
    gate = "pass"
}

$parent = Split-Path -Parent $reportPath
if (-not [string]::IsNullOrWhiteSpace($parent)) {
    [IO.Directory]::CreateDirectory($parent) | Out-Null
}
$utf8NoBom = New-Object Text.UTF8Encoding($false)
[IO.File]::WriteAllText($reportPath, ($report | ConvertTo-Json -Depth 6), $utf8NoBom)
$reportHash = (Get-FileHash -LiteralPath $reportPath -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText(
    $reportSidecar,
    "$reportHash  $([IO.Path]::GetFileName($reportPath))`r`n",
    $utf8NoBom)

Write-Host "Release acceptance passed for $ExpectedSourceCommit with $($manualRows.Count) manual cases and $($visualRows.Count) visual captures."
