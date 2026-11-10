[CmdletBinding(PositionalBinding = $false)]
param(
    [string]$QaReportPath,
    [string[]]$RequiredWorkOrderIds = @(),
    [int]$ExpectedTargetAssetCount = -1,
    [int]$ExpectedAcceptedAssetCount = -1,
    [int]$ExpectedCandidateAssetCount = -1,
    [switch]$AssertCandidatePreviewRuntimeSplit,
    [string]$PackedRuntimeManifestPath = "Resources/Maps/Tilesets/Generated/FantasyNeutral/v2/packed_runtime_manifest.json",
    [string]$PackedCandidateManifestPath = "Resources/Maps/Tilesets/Generated/FantasyNeutral/v2/packed_candidate_manifest.json"
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")

function Get-NormalizedWorkOrderIds([string[]]$Ids) {
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

function Get-ContextValue([string]$SemanticContext, [string]$Key) {
    if ([string]::IsNullOrWhiteSpace($SemanticContext)) {
        return ""
    }
    foreach ($part in ($SemanticContext -split ";")) {
        $trimmed = $part.Trim()
        $prefix = "$Key="
        if ($trimmed.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            return $trimmed.Substring($prefix.Length)
        }
    }
    return ""
}

if ([string]::IsNullOrWhiteSpace($QaReportPath)) {
    throw "QaReportPath is required."
}

$qaReportResolvedPath = Resolve-TilesetPath $QaReportPath
if (-not (Test-Path -LiteralPath $qaReportResolvedPath)) {
    throw "Missing QA report: $qaReportResolvedPath"
}

$qaReport = Get-Content -Raw -LiteralPath $qaReportResolvedPath | ConvertFrom-Json
$samples = @()
$diagnosticPreviews = @()
foreach ($result in (ConvertTo-TilesetArray (Get-TilesetProperty $qaReport "results" @()))) {
    $caseName = Get-TilesetProperty $result "case" ""
    foreach ($sample in (ConvertTo-TilesetArray (Get-TilesetProperty $result "contextual_coverage_samples" @()))) {
        $samples += [pscustomobject]@{
            case = $caseName
            domain = Get-TilesetProperty $sample "domain" ""
            work_order_id = Get-TilesetProperty $sample "work_order_id" ""
            selected_asset_id = Get-TilesetProperty $sample "selected_asset_id" ""
            candidate_preview_asset_id = Get-TilesetProperty $sample "candidate_preview_asset_id" ""
            fallback_reason = Get-TilesetProperty $sample "fallback_reason" ""
            source = "contextual_coverage_sample"
        }
    }
    foreach ($diagnostic in (ConvertTo-TilesetArray (Get-TilesetProperty $result "diagnostics" @()))) {
        $semanticContext = Get-TilesetProperty $diagnostic "semantic_context" ""
        $previewId = Get-ContextValue $semanticContext "candidate_preview_asset"
        if (-not [string]::IsNullOrWhiteSpace($previewId)) {
            $diagnosticPreviews += [pscustomobject]@{
                case = $caseName
                domain = Get-ContextValue $semanticContext "domain"
                work_order_id = Get-ContextValue $semanticContext "work_order"
                selected_asset_id = Get-TilesetProperty $diagnostic "selected_asset_id" ""
                candidate_preview_asset_id = $previewId
                fallback_reason = Get-TilesetProperty $diagnostic "fallback_reason" ""
                source = "diagnostic"
                label = Get-TilesetProperty $diagnostic "label" ""
            }
        }
    }
}

$sampledWorkOrderIds = @($samples |
    ForEach-Object { $_.work_order_id } |
    Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
    Sort-Object -Unique)

$requiredWorkOrderIds = @(Get-NormalizedWorkOrderIds $RequiredWorkOrderIds)
$missing = @($requiredWorkOrderIds | Where-Object { $_ -notin $sampledWorkOrderIds })
if ($missing.Count -gt 0) {
    throw "Missing contextual coverage samples for work order(s): $($missing -join ', ')"
}

$contextualAssetCoverage = Get-TilesetProperty $qaReport "contextual_asset_coverage" ([pscustomobject]@{})
if ($ExpectedTargetAssetCount -ge 0) {
    $actual = [int](Get-TilesetProperty $contextualAssetCoverage "target_asset_count" -1)
    if ($actual -ne $ExpectedTargetAssetCount) {
        throw "Expected contextual target asset count $ExpectedTargetAssetCount, found $actual"
    }
}
if ($ExpectedAcceptedAssetCount -ge 0) {
    $actual = [int](Get-TilesetProperty $contextualAssetCoverage "accepted_asset_count" -1)
    if ($actual -ne $ExpectedAcceptedAssetCount) {
        throw "Expected contextual accepted asset count $ExpectedAcceptedAssetCount, found $actual"
    }
}
if ($ExpectedCandidateAssetCount -ge 0) {
    $actual = [int](Get-TilesetProperty $contextualAssetCoverage "candidate_asset_count" -1)
    if ($actual -ne $ExpectedCandidateAssetCount) {
        throw "Expected contextual candidate asset count $ExpectedCandidateAssetCount, found $actual"
    }
}

if ($AssertCandidatePreviewRuntimeSplit) {
    $runtimeManifestResolvedPath = Resolve-TilesetPath $PackedRuntimeManifestPath
    $candidateManifestResolvedPath = Resolve-TilesetPath $PackedCandidateManifestPath
    foreach ($manifestPath in @($runtimeManifestResolvedPath, $candidateManifestResolvedPath)) {
        if (-not (Test-Path -LiteralPath $manifestPath)) {
            throw "Missing packed manifest for candidate/runtime split assertion: $manifestPath"
        }
    }

    $runtimeManifest = Get-Content -Raw -LiteralPath $runtimeManifestResolvedPath | ConvertFrom-Json
    $candidateManifest = Get-Content -Raw -LiteralPath $candidateManifestResolvedPath | ConvertFrom-Json
    $runtimeIds = @{}
    $candidateStatuses = @{}
    foreach ($asset in (ConvertTo-TilesetArray (Get-TilesetProperty $runtimeManifest "assets" @()))) {
        $id = Get-TilesetProperty $asset "id" ""
        if (-not [string]::IsNullOrWhiteSpace($id)) {
            $runtimeIds[$id] = Get-TilesetProperty $asset "status" ""
        }
    }
    foreach ($asset in (ConvertTo-TilesetArray (Get-TilesetProperty $candidateManifest "assets" @()))) {
        $id = Get-TilesetProperty $asset "id" ""
        if (-not [string]::IsNullOrWhiteSpace($id)) {
            $candidateStatuses[$id] = Get-TilesetProperty $asset "status" ""
        }
    }

    $previewRows = @(($samples + $diagnosticPreviews) | Where-Object { -not [string]::IsNullOrWhiteSpace($_.candidate_preview_asset_id) })
    if ($previewRows.Count -eq 0) {
        throw "Expected at least one contextual coverage sample or diagnostic with candidate_preview_asset_id"
    }

    foreach ($row in $previewRows) {
        if ($row.selected_asset_id -eq $row.candidate_preview_asset_id) {
            throw "Candidate preview asset leaked into selected runtime asset for $($row.source) case '$($row.case)' work order '$($row.work_order_id)': $($row.candidate_preview_asset_id)"
        }
        $diagnosticKind = Get-TilesetContextualDiagnosticKind ([pscustomobject]@{
            label = $row.label
            fallback_reason = $row.fallback_reason
        })
        if ($diagnosticKind -notin @("candidate_not_runtime_accepted", "context_or_art_gap")) {
            throw "Expected candidate preview $($row.source) '$($row.candidate_preview_asset_id)' to remain behind a candidate or contextual-art fallback, found '$($row.fallback_reason)'"
        }
        if (-not $candidateStatuses.ContainsKey($row.candidate_preview_asset_id)) {
            throw "Candidate preview asset '$($row.candidate_preview_asset_id)' was not found in the packed candidate manifest"
        }
        if ($candidateStatuses[$row.candidate_preview_asset_id] -ne "candidate") {
            throw "Candidate preview asset '$($row.candidate_preview_asset_id)' has packed candidate status '$($candidateStatuses[$row.candidate_preview_asset_id])'"
        }
        if ($runtimeIds.ContainsKey($row.candidate_preview_asset_id)) {
            throw "Candidate preview asset '$($row.candidate_preview_asset_id)' also appears in the accepted runtime manifest"
        }
    }
}

Write-Host "Contextual coverage sample assertion passed"
Write-Host "Report: $(ConvertTo-TilesetRelativePath $qaReportResolvedPath)"
Write-Host "Samples: $($samples.Count); sampled work orders: $($sampledWorkOrderIds.Count)"
if ($AssertCandidatePreviewRuntimeSplit) {
    $previewCount = @($samples + $diagnosticPreviews | Where-Object { -not [string]::IsNullOrWhiteSpace($_.candidate_preview_asset_id) }).Count
    Write-Host "Candidate/runtime split: $previewCount preview sample/diagnostic row(s) remain candidate-only"
}
if ($ExpectedTargetAssetCount -ge 0 -or $ExpectedAcceptedAssetCount -ge 0 -or $ExpectedCandidateAssetCount -ge 0) {
    Write-Host "Contextual asset coverage: $((Get-TilesetProperty $contextualAssetCoverage 'accepted_asset_count' 0))/$((Get-TilesetProperty $contextualAssetCoverage 'target_asset_count' 0)) accepted; $((Get-TilesetProperty $contextualAssetCoverage 'candidate_asset_count' 0)) candidates"
}
