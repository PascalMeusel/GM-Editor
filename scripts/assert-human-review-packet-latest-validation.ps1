[CmdletBinding(PositionalBinding = $false)]
param(
    [string]$PacketJsonPath = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2/review/contextual_candidates/contextual_human_review_packet.json",
    [string]$PacketMarkdownPath = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2/review/contextual_candidates/contextual_human_review_packet.md"
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")

$jsonPath = Resolve-TilesetPath $PacketJsonPath
$markdownPath = Resolve-TilesetPath $PacketMarkdownPath
foreach ($path in @($jsonPath, $markdownPath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing human-review packet artifact: $path"
    }
}

$packet = Get-Content -Raw -LiteralPath $jsonPath | ConvertFrom-Json
$summary = Get-TilesetProperty $packet "summary" ([pscustomobject]@{})
$runtimeUsable = [bool](Get-TilesetProperty $summary "runtime_usable" $true)
$runtimeUsabilityState = [string](Get-TilesetProperty $summary "runtime_usability_state" "")
if ($runtimeUsable) {
    throw "Human-review packet should not mark candidate-only contextual art as runtime usable"
}
if ($runtimeUsabilityState -ne "candidate_complete_not_runtime_accepted") {
    throw "Expected candidate-only runtime usability state, found '$runtimeUsabilityState'"
}

$workOrders = @(ConvertTo-TilesetArray (Get-TilesetProperty $packet "work_orders" @()))
if ($workOrders.Count -eq 0) {
    throw "Human-review packet has no work orders"
}

$missingLatest = @()
foreach ($workOrder in $workOrders) {
    $id = Get-TilesetProperty $workOrder "id" "<unknown>"
    $validationEvidenceCount = [int](Get-TilesetProperty $workOrder "validation_evidence_count" 0)
    $latest = Get-TilesetProperty $workOrder "latest_validation" $null
    $latestId = Get-TilesetProperty $latest "id" ""
    if ($validationEvidenceCount -gt 0 -and [string]::IsNullOrWhiteSpace($latestId)) {
        $missingLatest += $id
    }
}

if ($missingLatest.Count -gt 0) {
    throw "Human-review packet work orders missing latest_validation: $($missingLatest -join ', ')"
}

$grassRoad = @($workOrders | Where-Object { (Get-TilesetProperty $_ "id" "") -eq "fn_v2_way_dirt_road_on_grass_open" } | Select-Object -First 1)
if ($grassRoad.Count -ne 1) {
    throw "Human-review packet is missing fn_v2_way_dirt_road_on_grass_open"
}

$grassLatest = Get-TilesetProperty $grassRoad[0] "latest_validation" $null
$grassLatestId = Get-TilesetProperty $grassLatest "id" ""
if ($grassLatestId -notlike "*app_scale_contrast_review*") {
    throw "Expected grass-road latest validation to point at the app-scale contrast review, found '$grassLatestId'"
}

$grassLatestLabels = @(ConvertTo-TilesetArray (Get-TilesetProperty $grassLatest "issue_labels" @()) | ForEach-Object { [string]$_ })
if ("app_scale_contrast_metrics_pass" -notin $grassLatestLabels) {
    throw "Expected grass-road latest validation to include app_scale_contrast_metrics_pass"
}
if ("low_contrast_at_app_scale_still_open" -in $grassLatestLabels) {
    throw "Grass-road latest validation still exposes the superseded low-contrast hold"
}

$markdown = Get-Content -Raw -LiteralPath $markdownPath
if ($markdown -notmatch "(?m)^- Latest validation:") {
    throw "Human-review packet Markdown does not render latest validation lines"
}
if ($markdown -notmatch '(?m)^- Runtime usability: `candidate_complete_not_runtime_accepted`') {
    throw "Human-review packet Markdown does not render explicit runtime usability state"
}

Write-Host "Human-review latest-validation packet check: $($workOrders.Count) work order(s), grass-road latest '$grassLatestId'"
