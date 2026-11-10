[CmdletBinding(PositionalBinding = $false)]
param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$BatchReviewExe = "build/map_generation_batch_review.exe",
    [int]$Cycles = 8,
    [int]$StartCycle = 0,
    [string]$CaseFilter = "",
    [int]$CaseOffset = 0,
    [int]$CaseLimit = -1,
    [switch]$IncludeContextFixtures,
    [switch]$ListCasesOnly,
    [string]$BatchOutputDirectory = "",
    [string]$IntermediateStem = "",
    [string]$PacketOutputDirectory = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2/review/contextual_candidates",
    [string]$PacketStem = "",
    [string[]]$RequiredSampleWorkOrderIds = @()
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")

function Invoke-CheckedNativeCommand([string]$ExePath, [string[]]$Arguments) {
    & $ExePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed ($LASTEXITCODE): $ExePath $($Arguments -join ' ')"
    }
}

function Get-NormalizedRequiredSampleWorkOrderIds([string[]]$Ids) {
    $normalized = @()
    foreach ($idGroup in $Ids) {
        foreach ($id in ([string]$idGroup -split "[,;]")) {
            $trimmed = $id.Trim()
            if (-not [string]::IsNullOrWhiteSpace($trimmed)) {
                $normalized += $trimmed
            }
        }
    }
    return @($normalized | Select-Object -Unique)
}

$sourceRootPath = Resolve-TilesetPath $SourceRoot
$batchReviewExePath = Resolve-TilesetPath $BatchReviewExe
if (-not (Test-Path -LiteralPath $batchReviewExePath)) {
    throw "Missing batch review executable: $batchReviewExePath"
}

$defaultStem = if ($IncludeContextFixtures) { "way_owner_samples_with_fixtures_medium" } else { "way_owner_samples_medium" }
if ([string]::IsNullOrWhiteSpace($IntermediateStem)) {
    $IntermediateStem = $defaultStem
}
if ([string]::IsNullOrWhiteSpace($PacketStem)) {
    $PacketStem = "contextual_human_review_packet_$IntermediateStem"
}
if ([string]::IsNullOrWhiteSpace($BatchOutputDirectory)) {
    $BatchOutputDirectory = "build/map-generation-review-$IntermediateStem"
}

$batchOutputPath = Resolve-TilesetPath $BatchOutputDirectory
# Intermediate evidence follows the caller's batch directory, including CTest's
# isolated external build directory. A clean Git status alone misses ignored
# reports accidentally written under the source checkout's build/ directory.
$coverageReportPath = Join-Path $batchOutputPath "style_coverage_report_$IntermediateStem.json"
$readinessReportPath = Join-Path $batchOutputPath "contextual_acceptance_readiness_$IntermediateStem.json"
$promotionPlanPath = Join-Path $batchOutputPath "contextual_promotion_plan_$IntermediateStem.json"
$packetOutputPath = Resolve-TilesetPath $PacketOutputDirectory
$requiredSampleWorkOrderIds = @(Get-NormalizedRequiredSampleWorkOrderIds $RequiredSampleWorkOrderIds)

$listCaseArgs = @(
    "--list-cases",
    "--cycles", [string]$Cycles,
    "--start-cycle", [string]$StartCycle
)
if ($IncludeContextFixtures) {
    $listCaseArgs += "--include-context-fixtures"
}
if (-not [string]::IsNullOrWhiteSpace($CaseFilter)) {
    $listCaseArgs += @("--case-filter", $CaseFilter)
}
if ($CaseOffset -gt 0) {
    $listCaseArgs += @("--case-offset", [string]$CaseOffset)
}
if ($CaseLimit -ge 0) {
    $listCaseArgs += @("--case-limit", [string]$CaseLimit)
}
if ($ListCasesOnly) {
    Invoke-CheckedNativeCommand $batchReviewExePath $listCaseArgs
    return
}

$batchArgs = @(
    "--art-readiness",
    "--cycles", [string]$Cycles,
    "--start-cycle", [string]$StartCycle,
    "--skip-contact-sheet",
    "--out", $batchOutputPath
)
if ($IncludeContextFixtures) {
    $batchArgs += "--include-context-fixtures"
}
if (-not [string]::IsNullOrWhiteSpace($CaseFilter)) {
    $batchArgs += @("--case-filter", $CaseFilter)
}
if ($CaseOffset -gt 0) {
    $batchArgs += @("--case-offset", [string]$CaseOffset)
}
if ($CaseLimit -ge 0) {
    $batchArgs += @("--case-limit", [string]$CaseLimit)
}

Invoke-CheckedNativeCommand $batchReviewExePath $batchArgs

$qaReportPath = Join-Path $batchOutputPath "map_generation_batch_report.json"
if (-not (Test-Path -LiteralPath $qaReportPath)) {
    throw "Batch review did not write QA report: $qaReportPath"
}

$batchProgressPath = Join-Path $batchOutputPath "progress.json"
if (-not (Test-Path -LiteralPath $batchProgressPath)) {
    throw "Batch review did not write progress sidecar: $batchProgressPath"
}
$batchProgress = Get-Content -Raw -LiteralPath $batchProgressPath | ConvertFrom-Json
$batchProgressStatus = [string](Get-TilesetProperty $batchProgress "status" "")
if ($batchProgressStatus -ne "complete") {
    throw "Batch review progress sidecar did not complete: $batchProgressPath status '$batchProgressStatus'"
}

if ($requiredSampleWorkOrderIds.Count -gt 0) {
    & (Join-Path $PSScriptRoot "assert-contextual-coverage-samples.ps1") `
        -QaReportPath $qaReportPath `
        -RequiredWorkOrderIds $requiredSampleWorkOrderIds
}

& (Join-Path $PSScriptRoot "tileset_style_coverage_report.ps1") `
    -SourceRoot $sourceRootPath `
    -QaReportPath $qaReportPath `
    -OutputPath $coverageReportPath

& (Join-Path $PSScriptRoot "contextual_acceptance_readiness_report.ps1") `
    -CoverageReportPath $coverageReportPath `
    -QaReportPath $qaReportPath `
    -OutputJsonPath $readinessReportPath

& (Join-Path $PSScriptRoot "contextual_promotion_plan.ps1") `
    -SourceRoot $sourceRootPath `
    -CoverageReportPath $coverageReportPath `
    -QaReportPath $qaReportPath `
    -OutputJsonPath $promotionPlanPath

& (Join-Path $PSScriptRoot "contextual_human_review_packet.ps1") `
    -ReadinessReportPath $readinessReportPath `
    -PromotionPlanPath $promotionPlanPath `
    -OutputDirectory $packetOutputPath `
    -OutputStem $PacketStem

$packetJsonPath = Join-Path $packetOutputPath "$PacketStem.json"
$packet = Get-Content -Raw -LiteralPath $packetJsonPath | ConvertFrom-Json
$summary = Get-TilesetProperty $packet "summary" ([pscustomobject]@{})
$sourceQa = Get-TilesetProperty $packet "source_qa" ([pscustomobject]@{})
$sourceQaLine = Format-TilesetSourceQaLine $sourceQa
if ($sourceQaLine.StartsWith("- ", [System.StringComparison]::Ordinal)) {
    $sourceQaLine = $sourceQaLine.Substring(2)
}

Write-Host "Contextual way-owner sample packet complete"
Write-Host "QA report: $(ConvertTo-TilesetRelativePath $qaReportPath)"
Write-Host "Batch progress: $(ConvertTo-TilesetRelativePath $batchProgressPath)"
Write-Host "Coverage report: $(ConvertTo-TilesetRelativePath $coverageReportPath)"
Write-Host "Readiness report: $(ConvertTo-TilesetRelativePath $readinessReportPath)"
Write-Host "Promotion plan: $(ConvertTo-TilesetRelativePath $promotionPlanPath)"
Write-Host "Packet: $(ConvertTo-TilesetRelativePath $packetJsonPath)"
Write-Host $sourceQaLine
if ($requiredSampleWorkOrderIds.Count -gt 0) {
    Write-Host "Required sampled work orders: $($requiredSampleWorkOrderIds -join ', ')"
}
Write-Host "Samples: $((Get-TilesetProperty $summary 'contextual_coverage_sample_count' 0)); sampled work orders: $((Get-TilesetProperty $summary 'contextual_sampled_work_order_count' 0)); promotion-ready: $((Get-TilesetProperty $summary 'promotion_ready_count' 0))"
