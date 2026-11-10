[CmdletBinding(PositionalBinding = $false)]
param(
    [Parameter(Mandatory = $true)]
    [string]$QaReportPath
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")

if (-not (Test-Path -LiteralPath $QaReportPath)) {
    throw "Batch review JSON not found: $QaReportPath"
}

$report = Get-Content -LiteralPath $QaReportPath -Raw | ConvertFrom-Json

if ([bool](Get-TilesetProperty $report "art_readiness" $true)) {
    throw "Expected no-art batch review to set art_readiness=false"
}

$coverage = Get-TilesetProperty $report "contextual_asset_coverage" ([pscustomobject]@{})
$coverageProperties = @($coverage.PSObject.Properties)
if ($coverageProperties.Count -ne 0) {
    throw "Expected no-art batch review to leave contextual_asset_coverage empty, found: $($coverageProperties.Name -join ', ')"
}

$timing = Get-TilesetProperty $report "timing" ([pscustomobject]@{})
$preflightElapsedMs = [int64](Get-TilesetProperty $timing "preflight_elapsed_ms" -1)
$warmupElapsedMs = [int64](Get-TilesetProperty $timing "art_readiness_warmup_elapsed_ms" -1)
if ($preflightElapsedMs -ne 0) {
    throw "Expected no-art preflight to be 0 ms, found $preflightElapsedMs ms"
}
if ($warmupElapsedMs -ne 0) {
    throw "Expected no-art contextual warm-up to be 0 ms, found $warmupElapsedMs ms"
}

$results = @(ConvertTo-TilesetArray (Get-TilesetProperty $report "results" @()))
if ($results.Count -eq 0) {
    throw "Expected no-art batch review to contain at least one generated-map result"
}

foreach ($result in $results) {
    $caseName = [string](Get-TilesetProperty $result "case" "<unknown>")
    $phaseTiming = Get-TilesetProperty $result "phase_timing" ([pscustomobject]@{})
    $validationElapsedMs = [int64](Get-TilesetProperty $phaseTiming "validation_elapsed_ms" -1)
    if ($validationElapsedMs -ne 0) {
        throw "Expected no-art validation timing for case '$caseName' to be 0 ms, found $validationElapsedMs ms"
    }

    $samples = @(ConvertTo-TilesetArray (Get-TilesetProperty $result "contextual_coverage_samples" @()))
    if ($samples.Count -ne 0) {
        throw "Expected no-art case '$caseName' to emit no contextual coverage samples, found $($samples.Count)"
    }

    $stageTimings = @(ConvertTo-TilesetArray (Get-TilesetProperty $result "generation_stage_timings" @()))
    if (-not @($stageTimings | Where-Object { (Get-TilesetProperty $_ "stage" "") -eq "Complete" })) {
        throw "Expected no-art case '$caseName' to persist generator stage timing through Complete"
    }
}

Write-Host "No-art batch review fast path: $($results.Count) case(s), preflight ${preflightElapsedMs} ms, contextual warm-up ${warmupElapsedMs} ms"
