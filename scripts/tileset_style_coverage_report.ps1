param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$QaReportPath = "build/map-generation-review-structure-way-ground-resolution/map_generation_batch_report.json",
    [string]$OutputPath = "build/style_coverage_report_structure_way_ground_resolution.json"
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")

function Get-StatusCount($Assets, [string]$Status) {
    return @($Assets | Where-Object { (Get-TilesetProperty $_ "status" "") -eq $Status }).Count
}

function Get-RequiredSlotCount($WorkOrder) {
    $slots = @(Get-RequiredSlots $WorkOrder)
    if ($slots.Count -gt 0) {
        return $slots.Count
    }

    $context = Get-TilesetProperty $WorkOrder "required_context" $null
    $grounds = @(ConvertTo-TilesetArray (Get-TilesetProperty $context "grounds" @()))
    $entrances = @(ConvertTo-TilesetArray (Get-TilesetProperty $context "entrance_types" @()))
    if ($grounds.Count -gt 0 -and $entrances.Count -gt 0) {
        return $grounds.Count * $entrances.Count
    }
    return 1
}

function Get-RequiredSlots($WorkOrder) {
    return @(ConvertTo-TilesetArray (Get-TilesetProperty $WorkOrder "required_masks_or_roles" @()) | ForEach-Object { [string]$_ })
}

function Test-AssetMatchesRequiredSlot($Asset, [string]$Slot) {
    if ([string]::IsNullOrWhiteSpace($Slot)) {
        return $false
    }

    $id = Get-TilesetProperty $Asset "id" ""
    if ($id.Contains(".$Slot.") -or $id.Contains("__$Slot__")) {
        return $true
    }

    if ($Slot -match "^mask([0-9]{3})$") {
        $mask = Get-TilesetProperty $Asset "mask" $null
        if ($null -ne $mask -and [int]$mask -eq [int]$Matches[1]) {
            return $true
        }
    }

    foreach ($field in @("role", "slot", "span_position")) {
        if ((Get-TilesetProperty $Asset $field "") -eq $Slot) {
            return $true
        }
    }
    return $false
}

function Get-SlotCoverage($Assets, [string[]]$RequiredSlots, [int]$TargetVariants) {
    $items = @()
    foreach ($slot in $RequiredSlots) {
        $slotAssets = @($Assets | Where-Object { Test-AssetMatchesRequiredSlot $_ $slot })
        $accepted = Get-StatusCount $slotAssets "accepted"
        $candidate = Get-StatusCount $slotAssets "candidate"
        $needsRepaint = Get-StatusCount $slotAssets "needs_repaint"
        $items += [pscustomobject]@{
            slot = $slot
            target_variants = $TargetVariants
            accepted_asset_count = $accepted
            candidate_asset_count = $candidate
            candidate_or_accepted_count = $accepted + $candidate
            needs_repaint_asset_count = $needsRepaint
            missing_accepted_asset_count = [Math]::Max(0, $TargetVariants - $accepted)
            missing_candidate_or_accepted_count = [Math]::Max(0, $TargetVariants - ($accepted + $candidate))
        }
    }
    return $items
}

function Get-LinkedQaDiagnostics($QaReport, $Labels, [string]$WorkOrderId = "") {
    $items = @()
    if ($null -eq $QaReport) {
        return $items
    }

    foreach ($result in (ConvertTo-TilesetArray (Get-TilesetProperty $QaReport "results" @()))) {
        foreach ($diagnostic in (ConvertTo-TilesetArray (Get-TilesetProperty $result "diagnostics" @()))) {
            $label = Get-TilesetProperty $diagnostic "label" ""
            if ($label -notin $Labels) {
                continue
            }
            $semanticContext = Get-TilesetProperty $diagnostic "semantic_context" ""
            if (-not [string]::IsNullOrWhiteSpace($WorkOrderId) -and
                $semanticContext.Contains("work_order=") -and
                -not $semanticContext.Contains("work_order=$WorkOrderId")) {
                continue
            }
            $items += [pscustomobject]@{
                case = Get-TilesetProperty $result "case" ""
                seed = Get-TilesetProperty $result "seed" 0
                label = $label
                selected_asset_id = Get-TilesetProperty $diagnostic "selected_asset_id" ""
                fallback_reason = Get-TilesetProperty $diagnostic "fallback_reason" ""
                semantic_context = $semanticContext
                diagnostic_kind = Get-TilesetContextualDiagnosticKind $diagnostic
                coordinates = @(ConvertTo-TilesetArray (Get-TilesetProperty $diagnostic "coordinates" @()))
            }
        }
    }
    return $items
}

function Get-LinkedContextualCoverageSamples($QaReport, $Labels, [string]$WorkOrderId = "") {
    $items = @()
    if ($null -eq $QaReport) {
        return $items
    }

    foreach ($result in (ConvertTo-TilesetArray (Get-TilesetProperty $QaReport "results" @()))) {
        foreach ($sample in (ConvertTo-TilesetArray (Get-TilesetProperty $result "contextual_coverage_samples" @()))) {
            $label = Get-TilesetProperty $sample "qa_label" ""
            $semanticContext = Get-TilesetProperty $sample "semantic_context" ""
            $sampleWorkOrderId = Get-TilesetProperty $sample "work_order_id" ""

            if (-not [string]::IsNullOrWhiteSpace($WorkOrderId)) {
                $matchesWorkOrder = $sampleWorkOrderId -eq $WorkOrderId
                if (-not $matchesWorkOrder -and -not [string]::IsNullOrWhiteSpace($semanticContext)) {
                    $matchesWorkOrder = $semanticContext.Contains("work_order=$WorkOrderId")
                }
                if (-not $matchesWorkOrder) {
                    continue
                }
            }
            elseif ($Labels.Count -gt 0 -and $label -notin $Labels) {
                continue
            }

            $items += [pscustomobject]@{
                case = Get-TilesetProperty $result "case" ""
                seed = Get-TilesetProperty $result "seed" 0
                domain = Get-TilesetProperty $sample "domain" ""
                label = $label
                work_order_id = $sampleWorkOrderId
                selected_asset_id = Get-TilesetProperty $sample "selected_asset_id" ""
                candidate_preview_asset_id = Get-TilesetProperty $sample "candidate_preview_asset_id" ""
                fallback_reason = Get-TilesetProperty $sample "fallback_reason" ""
                semantic_context = $semanticContext
                target_asset_count = [int](Get-TilesetProperty $sample "target_asset_count" 0)
                accepted_asset_count = [int](Get-TilesetProperty $sample "accepted_asset_count" 0)
                candidate_asset_count = [int](Get-TilesetProperty $sample "candidate_asset_count" 0)
            }
        }
    }
    return $items
}

function Get-WorkOrderOwnerCounts($WorkOrderRecords) {
    $counts = @{}
    foreach ($record in $WorkOrderRecords) {
        $owner = Get-TilesetProperty $record.WorkOrder "owner" ""
        if ([string]::IsNullOrWhiteSpace($owner)) {
            continue
        }
        if (-not $counts.ContainsKey($owner)) {
            $counts[$owner] = 0
        }
        $counts[$owner] += 1
    }
    return $counts
}

function Get-WorkOrderAssetRecords($AssetRecords, $WorkOrder, $OwnerCounts) {
    $workOrderId = Get-TilesetProperty $WorkOrder "id" ""
    $manifestOwner = Get-TilesetProperty $WorkOrder "owner" ""
    $coverageOwner = Get-TilesetProductionWorkOrderCoverageOwner $WorkOrder
    $manifestOwnerIsShared = $OwnerCounts.ContainsKey($manifestOwner) -and [int]$OwnerCounts[$manifestOwner] -gt 1

    return @($AssetRecords | Where-Object {
        $asset = $_.Asset
        $assetWorkOrderId = Get-TilesetProperty $asset "work_order_id" ""
        $assetOwner = Get-TilesetProperty $asset "owner" ""
        $hasExplicitWorkOrder = -not [string]::IsNullOrWhiteSpace($assetWorkOrderId)

        ($hasExplicitWorkOrder -and $assetWorkOrderId -eq $workOrderId) -or
            (-not $hasExplicitWorkOrder -and
                ($assetOwner -eq $coverageOwner -or
                    ($assetOwner -eq $manifestOwner -and -not $manifestOwnerIsShared)))
    })
}

function Get-QaReportProvenance($QaReport, [string]$QaReportResolvedPath) {
    if ($null -eq $QaReport) {
        return $null
    }

    return [pscustomobject]@{
        report = ConvertTo-TilesetRelativePath $QaReportResolvedPath
        report_type = Get-TilesetProperty $QaReport "report_type" ""
        case_count = [int](Get-TilesetProperty $QaReport "case_count" 0)
        case_filter = Get-TilesetProperty $QaReport "case_filter" ""
        available_case_count = [int](Get-TilesetProperty $QaReport "available_case_count" (Get-TilesetProperty $QaReport "case_count" 0))
        case_offset = [int](Get-TilesetProperty $QaReport "case_offset" 0)
        case_limit = Get-TilesetProperty $QaReport "case_limit" $null
        context_fixtures = [bool](Get-TilesetProperty $QaReport "context_fixtures" $false)
        art_readiness = [bool](Get-TilesetProperty $QaReport "art_readiness" $false)
        warning_count = [int](Get-TilesetProperty $QaReport "warning_count" 0)
        error_count = [int](Get-TilesetProperty $QaReport "error_count" 0)
        contextual_asset_coverage = Get-TilesetProperty $QaReport "contextual_asset_coverage" ([pscustomobject]@{})
        timing = Get-TilesetProperty $QaReport "timing" ([pscustomobject]@{})
    }
}

$resolvedSourceRoot = Resolve-TilesetPath $SourceRoot
if (-not (Test-Path -LiteralPath $resolvedSourceRoot)) {
    throw "Missing v2 source root: $resolvedSourceRoot"
}
if (-not $OutputPath) {
    $OutputPath = Join-Path $resolvedSourceRoot "review/style_coverage_report.json"
}
$resolvedOutputPath = Resolve-TilesetPath $OutputPath
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $resolvedOutputPath) | Out-Null

$bundle = Get-TilesetV2ManifestBundle $resolvedSourceRoot
$assetRecords = @(Get-TilesetV2AssetRecords $bundle)
$workOrderRecords = @(Get-TilesetV2ProductionWorkOrderRecords $bundle)
$workOrderOwnerCounts = Get-WorkOrderOwnerCounts $workOrderRecords
$qaReportResolvedPath = Resolve-TilesetPath $QaReportPath
$qaReport = $null
if (-not [string]::IsNullOrWhiteSpace($QaReportPath) -and (Test-Path -LiteralPath $qaReportResolvedPath)) {
    $qaReport = Get-Content -Raw -LiteralPath $qaReportResolvedPath | ConvertFrom-Json
}
$qaReportProvenance = Get-QaReportProvenance $qaReport $qaReportResolvedPath

$coverageItems = @()
foreach ($record in $workOrderRecords) {
    $workOrder = $record.WorkOrder
    $owner = Get-TilesetProperty $workOrder "owner" ""
    $coverageOwner = Get-TilesetProductionWorkOrderCoverageOwner $workOrder
    $assetClass = Get-TilesetProperty $workOrder "asset_class" ""
    $labels = @(ConvertTo-TilesetArray (Get-TilesetProperty $workOrder "qa_labels" @()))
    $ownerAssets = @(Get-WorkOrderAssetRecords $assetRecords $workOrder $workOrderOwnerCounts | ForEach-Object { $_.Asset })
    $requiredSlots = @(Get-RequiredSlots $workOrder)
    $slotCount = Get-RequiredSlotCount $workOrder
    $targetVariants = [int](Get-TilesetProperty $workOrder "target_variants" 0)
    $normalizedTargetVariants = [Math]::Max(1, $targetVariants)
    $targetAssets = [Math]::Max(1, $slotCount) * [Math]::Max(1, $targetVariants)
    $accepted = Get-StatusCount $ownerAssets "accepted"
    $candidate = Get-StatusCount $ownerAssets "candidate"
    $needsRepaint = Get-StatusCount $ownerAssets "needs_repaint"
    $slotCoverage = @(Get-SlotCoverage $ownerAssets $requiredSlots $normalizedTargetVariants)
    $qaDiagnostics = @(Get-LinkedQaDiagnostics $qaReport $labels $record.Id)
    $contextualSamples = @(Get-LinkedContextualCoverageSamples $qaReport $labels $record.Id)
    $qaByLabel = @()
    $contextualSamplesByLabel = @()
    foreach ($label in $labels) {
        $qaByLabel += [pscustomobject]@{
            label = $label
            diagnostics = @($qaDiagnostics | Where-Object { $_.label -eq $label }).Count
        }
        $contextualSamplesByLabel += [pscustomobject]@{
            label = $label
            samples = @($contextualSamples | Where-Object { $_.label -eq $label }).Count
        }
    }
    $contextualCandidatePreviewIds = @($contextualSamples |
        ForEach-Object { $_.candidate_preview_asset_id } |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
        Sort-Object -Unique)

    $coverageItems += [pscustomobject]@{
        id = $record.Id
        manifest = $record.Manifest
        asset_class = $assetClass
        owner = $owner
        coverage_owner = $coverageOwner
        scale = Get-TilesetProperty $workOrder "scale" ""
        owning_workstream = Get-TilesetProperty $workOrder "owning_workstream" ""
        status = Get-TilesetProperty $workOrder "status" ""
        required_slots = $slotCount
        target_variants = $targetVariants
        target_asset_count = $targetAssets
        accepted_asset_count = $accepted
        candidate_asset_count = $candidate
        needs_repaint_asset_count = $needsRepaint
        missing_accepted_asset_count = [Math]::Max(0, $targetAssets - $accepted)
        accepted_completion_percent = [Math]::Round(($accepted * 100.0) / [Math]::Max(1, $targetAssets), 1)
        candidate_or_accepted_percent = [Math]::Round((($accepted + $candidate) * 100.0) / [Math]::Max(1, $targetAssets), 1)
        qa_labels = $labels
        slot_coverage = $slotCoverage
        missing_candidate_or_accepted_slots = @($slotCoverage | Where-Object { [int]$_.candidate_or_accepted_count -eq 0 } | ForEach-Object { $_.slot })
        underfilled_candidate_or_accepted_slots = @($slotCoverage | Where-Object { [int]$_.candidate_or_accepted_count -gt 0 -and [int]$_.candidate_or_accepted_count -lt [int]$_.target_variants } | ForEach-Object { $_.slot })
        missing_accepted_slots = @($slotCoverage | Where-Object { [int]$_.accepted_asset_count -eq 0 } | ForEach-Object { $_.slot })
        qa_candidate_not_runtime_accepted_count = @($qaDiagnostics | Where-Object { $_.diagnostic_kind -eq "candidate_not_runtime_accepted" }).Count
        qa_context_or_art_gap_count = @($qaDiagnostics | Where-Object { $_.diagnostic_kind -eq "context_or_art_gap" }).Count
        qa_generator_exercise_gap_count = @($qaDiagnostics | Where-Object { $_.diagnostic_kind -eq "generator_exercise_gap" }).Count
        qa_unclassified_runtime_warning_count = @($qaDiagnostics | Where-Object { $_.diagnostic_kind -eq "unclassified_runtime_warning" }).Count
        qa_diagnostics_by_label = $qaByLabel
        qa_sample_diagnostics = @($qaDiagnostics | Select-Object -First 6)
        contextual_coverage_sample_count = $contextualSamples.Count
        contextual_coverage_samples_by_label = $contextualSamplesByLabel
        contextual_sample_candidate_preview_asset_count = $contextualCandidatePreviewIds.Count
        contextual_sample_candidate_preview_asset_ids = $contextualCandidatePreviewIds
        contextual_coverage_samples = @($contextualSamples | Select-Object -First 6)
    }
}

$totalTarget = 0
$totalAccepted = 0
$totalCandidate = 0
foreach ($item in $coverageItems) {
    $totalTarget += [int]$item.target_asset_count
    $totalAccepted += [int]$item.accepted_asset_count
    $totalCandidate += [int]$item.candidate_asset_count
}

$byClass = @()
foreach ($group in ($coverageItems | Group-Object asset_class)) {
    $classTarget = 0
    $classAccepted = 0
    $classCandidate = 0
    foreach ($item in $group.Group) {
        $classTarget += [int]$item.target_asset_count
        $classAccepted += [int]$item.accepted_asset_count
        $classCandidate += [int]$item.candidate_asset_count
    }
    $byClass += [pscustomobject]@{
        asset_class = $group.Name
        work_orders = $group.Count
        target_asset_count = $classTarget
        accepted_asset_count = $classAccepted
        candidate_asset_count = $classCandidate
        missing_accepted_asset_count = [Math]::Max(0, $classTarget - $classAccepted)
        accepted_completion_percent = [Math]::Round(($classAccepted * 100.0) / [Math]::Max(1, $classTarget), 1)
    }
}

$report = [pscustomobject]@{
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    source_root = ConvertTo-TilesetRelativePath $resolvedSourceRoot
    qa_report = if ($qaReport) { ConvertTo-TilesetRelativePath $qaReportResolvedPath } else { "" }
    source_qa = $qaReportProvenance
    summary = [pscustomobject]@{
        work_order_count = $coverageItems.Count
        target_asset_count = $totalTarget
        accepted_asset_count = $totalAccepted
        candidate_asset_count = $totalCandidate
        missing_accepted_asset_count = [Math]::Max(0, $totalTarget - $totalAccepted)
        accepted_completion_percent = [Math]::Round(($totalAccepted * 100.0) / [Math]::Max(1, $totalTarget), 1)
        candidate_or_accepted_percent = [Math]::Round((($totalAccepted + $totalCandidate) * 100.0) / [Math]::Max(1, $totalTarget), 1)
        qa_warning_count = if ($qaReport) { [int](Get-TilesetProperty $qaReport "warning_count" 0) } else { 0 }
        qa_error_count = if ($qaReport) { [int](Get-TilesetProperty $qaReport "error_count" 0) } else { 0 }
        candidate_not_runtime_accepted_count = @($coverageItems | Measure-Object -Property qa_candidate_not_runtime_accepted_count -Sum).Sum
        context_or_art_gap_count = @($coverageItems | Measure-Object -Property qa_context_or_art_gap_count -Sum).Sum
        generator_exercise_gap_count = @($coverageItems | Measure-Object -Property qa_generator_exercise_gap_count -Sum).Sum
        unclassified_runtime_warning_count = @($coverageItems | Measure-Object -Property qa_unclassified_runtime_warning_count -Sum).Sum
        contextual_coverage_sample_count = @($coverageItems | Measure-Object -Property contextual_coverage_sample_count -Sum).Sum
        contextual_sampled_work_order_count = @($coverageItems | Where-Object { [int]$_.contextual_coverage_sample_count -gt 0 }).Count
        contextual_sampled_candidate_preview_asset_count = @($coverageItems | ForEach-Object { $_.contextual_sample_candidate_preview_asset_ids } | Sort-Object -Unique).Count
    }
    by_asset_class = $byClass
    work_orders = $coverageItems
}

$report | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath $resolvedOutputPath
Write-Host "Tileset style coverage report wrote $resolvedOutputPath"
Write-Host "Work orders: $($coverageItems.Count); targets: $totalTarget; accepted: $totalAccepted; candidates: $totalCandidate"
if ($qaReport) {
    Write-Host "Contextual coverage samples: $($report.summary.contextual_coverage_sample_count) across $($report.summary.contextual_sampled_work_order_count) work orders"
}
