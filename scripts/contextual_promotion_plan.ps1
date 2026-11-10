param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$CoverageReportPath = "build/style_coverage_report_structure_way_ground_resolution.json",
    [string]$QaReportPath = "build/map-generation-review-structure-way-ground-resolution/map_generation_batch_report.json",
    [string]$ValidationResultsPath = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2/validation_results.json",
    [string]$OutputJsonPath = "build/contextual_promotion_plan_structure_way_ground_resolution.json",
    [string]$OutputMarkdownPath = ""
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")

function Get-StatusCount($Assets, [string]$Status) {
    return @($Assets | Where-Object { (Get-TilesetProperty $_ "status" "") -eq $Status }).Count
}

function Get-RequiredSlots($WorkOrder) {
    return @(ConvertTo-TilesetArray (Get-TilesetProperty $WorkOrder "required_masks_or_roles" @()) | ForEach-Object { [string]$_ })
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

function Get-ContextSlotKey($Diagnostic) {
    $parts = @()
    foreach ($field in @("ground", "entrance", "orientation", "crossing", "approach_way", "mask", "span_position", "component_role")) {
        $value = Get-TilesetProperty $Diagnostic $field ""
        if (-not [string]::IsNullOrWhiteSpace($value)) {
            $parts += "$field=$value"
        }
    }
    if ($parts.Count -eq 0) {
        return "<none>"
    }
    return ($parts -join "; ")
}

function Get-RuntimeMissingContextualSlots($Diagnostics) {
    $runtimeSlotDiagnostics = @($Diagnostics | Where-Object {
            $_.diagnostic_kind -eq "context_or_art_gap" -and
            $_.fallback_reason -eq "missing_contextual_asset_slot" -and
            [string]::IsNullOrWhiteSpace($_.candidate_preview_asset)
        })

    $items = @()
    foreach ($group in ($runtimeSlotDiagnostics | Group-Object { Get-ContextSlotKey $_ } | Sort-Object -Property @{ Expression = "Count"; Descending = $true }, Name)) {
        $items += [pscustomobject]@{
            slot = $group.Name
            diagnostic_count = $group.Count
            sample_seeds = @($group.Group | Select-Object -ExpandProperty seed -Unique | Select-Object -First 5)
        }
    }
    return $items
}

function Get-QaDiagnostics($QaReport) {
    $items = @()
    if ($null -eq $QaReport) {
        return $items
    }

    foreach ($result in (ConvertTo-TilesetArray (Get-TilesetProperty $QaReport "results" @()))) {
        foreach ($diagnostic in (ConvertTo-TilesetArray (Get-TilesetProperty $result "diagnostics" @()))) {
            $semanticContext = Get-TilesetProperty $diagnostic "semantic_context" ""
            $items += [pscustomobject]@{
                case = Get-TilesetProperty $result "case" ""
                seed = Get-TilesetProperty $result "seed" 0
                label = Get-TilesetProperty $diagnostic "label" ""
                selected_asset_id = Get-TilesetProperty $diagnostic "selected_asset_id" ""
                fallback_reason = Get-TilesetProperty $diagnostic "fallback_reason" ""
                semantic_context = $semanticContext
                work_order_id = Get-ContextValue $semanticContext "work_order"
                request_owner = Get-ContextValue $semanticContext "request_owner"
                candidate_preview_asset = Get-ContextValue $semanticContext "candidate_preview_asset"
                ground = Get-ContextValue $semanticContext "ground"
                entrance = Get-ContextValue $semanticContext "entrance"
                orientation = Get-ContextValue $semanticContext "orientation"
                crossing = Get-ContextValue $semanticContext "crossing"
                approach_way = Get-ContextValue $semanticContext "approach_way"
                mask = Get-ContextValue $semanticContext "mask"
                span_position = Get-ContextValue $semanticContext "span_position"
                component_role = Get-ContextValue $semanticContext "component_role"
                coordinates = @(ConvertTo-TilesetArray (Get-TilesetProperty $diagnostic "coordinates" @()))
            }
        }
    }
    return $items
}

function Get-GroupedCounts($Items, [string]$PropertyName) {
    $groups = @()
    foreach ($group in ($Items | Group-Object $PropertyName | Sort-Object -Property @{ Expression = "Count"; Descending = $true }, @{ Expression = "Name"; Ascending = $true })) {
        $groups += [pscustomobject]@{
            name = if ([string]::IsNullOrWhiteSpace($group.Name)) { "<empty>" } else { $group.Name }
            count = $group.Count
        }
    }
    return $groups
}

function Get-ValidationEvidence($ValidationResults, [string]$WorkOrderId) {
    $items = @()
    foreach ($entry in (ConvertTo-TilesetArray (Get-TilesetProperty $ValidationResults "results" @()))) {
        $singularWorkOrderId = Get-TilesetProperty $entry "work_order_id" ""
        $pluralWorkOrderIds = @(ConvertTo-TilesetArray (Get-TilesetProperty $entry "work_order_ids" @()) | ForEach-Object { [string]$_ })
        if ($singularWorkOrderId -ne $WorkOrderId -and $WorkOrderId -notin $pluralWorkOrderIds) {
            continue
        }
        $judgment = Get-TilesetProperty $entry "visual_judgment" $null
        $items += [pscustomobject]@{
            id = Get-TilesetProperty $entry "id" ""
            result_type = Get-TilesetProperty $entry "result_type" ""
            work_order_id = $singularWorkOrderId
            work_order_ids = $pluralWorkOrderIds
            reviewed_at_utc = Get-TilesetProperty $entry "reviewed_at_utc" ""
            status = Get-TilesetProperty $entry "status" ""
            rating = [int](Get-TilesetProperty $entry "rating" 0)
            issue_labels = @(ConvertTo-TilesetArray (Get-TilesetProperty $entry "issue_labels" @()))
            candidate_pass = [bool](Get-TilesetProperty $judgment "candidate_pass" $false)
            candidate_only = [bool](Get-TilesetProperty $judgment "candidate_only" $false)
            acceptance_ready = [bool](Get-TilesetProperty $judgment "acceptance_ready" $false)
            accepted = [bool](Get-TilesetProperty $judgment "accepted" $false)
            notes = Get-TilesetProperty $judgment "notes" ""
            evidence = Get-TilesetProperty $entry "evidence" ([pscustomobject]@{})
        }
    }
    return $items
}

function Get-LatestValidationEvidence($Evidence) {
    $sorted = @($Evidence | Sort-Object -Descending -Property @{
            Expression = {
                $parsed = [DateTime]::MinValue
                if (-not [string]::IsNullOrWhiteSpace($_.reviewed_at_utc)) {
                    [void][DateTime]::TryParse(
                        $_.reviewed_at_utc,
                        [System.Globalization.CultureInfo]::InvariantCulture,
                        [System.Globalization.DateTimeStyles]::AssumeUniversal,
                        [ref]$parsed)
                }
                $parsed
            }
        })
    if ($sorted.Count -eq 0) {
        return $null
    }
    return $sorted[0]
}

function Get-PropertySum($Items, [string]$PropertyName) {
    $sum = 0
    foreach ($item in $Items) {
        $sum += [int](Get-TilesetProperty $item $PropertyName 0)
    }
    return $sum
}

function Get-GeneratedMapFitEvidence($Evidence, [string]$WorkOrderId) {
    $items = @()
    foreach ($entry in @($Evidence | Where-Object { $_.result_type -eq "generated_map_contextual_candidate_fit_review" })) {
        $entryEvidence = Get-TilesetProperty $entry "evidence" ([pscustomobject]@{})
        $groups = @(ConvertTo-TilesetArray (Get-TilesetProperty $entryEvidence "groups" @()))
        $matchingGroups = @($groups | Where-Object { (Get-TilesetProperty $_ "work_order_id" "") -eq $WorkOrderId })
        if ($matchingGroups.Count -eq 0) {
            continue
        }

        $items += [pscustomobject]@{
            validation_id = $entry.id
            reviewed_at_utc = $entry.reviewed_at_utc
            status = $entry.status
            candidate_only = $entry.candidate_only
            review_sheet = Get-TilesetProperty $entryEvidence "review_sheet" ""
            markdown = Get-TilesetProperty $entryEvidence "markdown" ""
            diagnostic_count = Get-PropertySum $matchingGroups "diagnostic_count"
            unique_candidate_preview_asset_count = Get-PropertySum $matchingGroups "unique_candidate_preview_asset_count"
            sampled_role_coverage_slot_count = Get-PropertySum $matchingGroups "sampled_role_coverage_slot_count"
            missing_sampled_role_coverage_slot_count = [int](Get-TilesetProperty $entryEvidence "missing_sampled_role_coverage_slot_count" 0)
        }
    }
    return @($items | Sort-Object -Descending -Property reviewed_at_utc)
}

function Get-PromotionGate($MissingSlots, $UnderfilledSlots, $OverfilledSlots, [int]$RuntimeMissingContextualSlotCount, $QaErrorCount, $Evidence) {
    if ($MissingSlots.Count -gt 0 -or $UnderfilledSlots.Count -gt 0 -or $RuntimeMissingContextualSlotCount -gt 0) {
        return "blocked_candidate_generation_required"
    }
    if ($QaErrorCount -gt 0) {
        return "blocked_qa_errors"
    }
    $latestEvidence = Get-LatestValidationEvidence $Evidence
    if ($null -ne $latestEvidence -and ($latestEvidence.accepted -or $latestEvidence.acceptance_ready) -and $OverfilledSlots.Count -eq 0) {
        return "ready_for_manual_status_change"
    }
    return "blocked_human_art_direction_review_required"
}

function Get-PromotionAction($Gate) {
    switch ($Gate) {
        "blocked_candidate_generation_required" { return "Generate or repaint missing/underfilled slots before planning promotion." }
        "blocked_qa_errors" { return "Fix QA errors before any status change." }
        "ready_for_manual_status_change" { return "If art direction approves, change listed candidate assets to accepted and repack runtime atlases." }
    }
    return "Review map-scale evidence and either approve listed candidates, keep them candidate-only, or request repaint."
}

function Format-Percent($Value) {
    return [string]::Format([System.Globalization.CultureInfo]::InvariantCulture, "{0:0.0}%", [double]$Value)
}

$resolvedSourceRoot = Resolve-TilesetPath $SourceRoot
$coveragePath = Resolve-TilesetPath $CoverageReportPath
$qaPath = Resolve-TilesetPath $QaReportPath
$validationPath = Resolve-TilesetPath $ValidationResultsPath
$jsonPath = Resolve-TilesetPath $OutputJsonPath
if ([string]::IsNullOrWhiteSpace($OutputMarkdownPath)) {
    $OutputMarkdownPath = [System.IO.Path]::ChangeExtension($OutputJsonPath, ".md")
}
$markdownPath = Resolve-TilesetPath $OutputMarkdownPath

if (-not (Test-Path -LiteralPath $resolvedSourceRoot)) {
    throw "Missing v2 source root: $resolvedSourceRoot"
}
if (-not (Test-Path -LiteralPath $coveragePath)) {
    throw "Missing coverage report: $coveragePath"
}
if (-not (Test-Path -LiteralPath $qaPath)) {
    throw "Missing QA report: $qaPath"
}

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $jsonPath) | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $markdownPath) | Out-Null

$bundle = Get-TilesetV2ManifestBundle $resolvedSourceRoot
$assetRecords = @(Get-TilesetV2AssetRecords $bundle)
$workOrderRecords = @(Get-TilesetV2ProductionWorkOrderRecords $bundle)
$workOrderOwnerCounts = Get-WorkOrderOwnerCounts $workOrderRecords
$coverage = Get-Content -Raw -LiteralPath $coveragePath | ConvertFrom-Json
$qaReport = Get-Content -Raw -LiteralPath $qaPath | ConvertFrom-Json
$sourceQa = Get-TilesetProperty $coverage "source_qa" $null
$validationResults = Read-TilesetJson $validationPath ([pscustomobject]@{ results = @() })
$diagnostics = @(Get-QaDiagnostics $qaReport)
foreach ($diagnostic in $diagnostics) {
    $diagnostic | Add-Member -MemberType NoteProperty -Name diagnostic_kind -Value (Get-TilesetContextualDiagnosticKind $diagnostic)
}

$coverageById = @{}
foreach ($item in (ConvertTo-TilesetArray (Get-TilesetProperty $coverage "work_orders" @()))) {
    $id = Get-TilesetProperty $item "id" ""
    if (-not [string]::IsNullOrWhiteSpace($id)) {
        $coverageById[$id] = $item
    }
}

$workOrderPlans = @()
foreach ($record in $workOrderRecords) {
    $workOrder = $record.WorkOrder
    $workOrderId = $record.Id
    $ownerAssets = @(Get-WorkOrderAssetRecords $assetRecords $workOrder $workOrderOwnerCounts | ForEach-Object { $_.Asset })
    $candidateAssets = @($ownerAssets | Where-Object { (Get-TilesetProperty $_ "status" "") -eq "candidate" })
    $acceptedAssets = @($ownerAssets | Where-Object { (Get-TilesetProperty $_ "status" "") -eq "accepted" })
    $needsRepaintAssets = @($ownerAssets | Where-Object { (Get-TilesetProperty $_ "status" "") -eq "needs_repaint" })
    $otherAssets = @($ownerAssets | Where-Object { (Get-TilesetProperty $_ "status" "") -notin @("candidate", "accepted", "needs_repaint") })
    $requiredSlots = @(Get-RequiredSlots $workOrder)
    $requiredSlotCount = Get-RequiredSlotCount $workOrder
    $targetVariants = [Math]::Max(1, [int](Get-TilesetProperty $workOrder "target_variants" 0))
    $targetAssets = [Math]::Max(1, $requiredSlotCount) * $targetVariants
    $slotPlans = @()
    $missingSlots = @()
    $underfilledSlots = @()
    $overfilledSlots = @()

    foreach ($slot in $requiredSlots) {
        $slotAssets = @($ownerAssets | Where-Object { Test-AssetMatchesRequiredSlot $_ $slot })
        $slotAccepted = @($slotAssets | Where-Object { (Get-TilesetProperty $_ "status" "") -eq "accepted" })
        $slotCandidates = @($slotAssets | Where-Object { (Get-TilesetProperty $_ "status" "") -eq "candidate" })
        $slotNeedsRepaint = @($slotAssets | Where-Object { (Get-TilesetProperty $_ "status" "") -eq "needs_repaint" })
        $candidateOrAccepted = $slotAccepted.Count + $slotCandidates.Count
        if ($candidateOrAccepted -eq 0) {
            $missingSlots += $slot
        }
        elseif ($candidateOrAccepted -lt $targetVariants) {
            $underfilledSlots += $slot
        }
        elseif ($candidateOrAccepted -gt $targetVariants) {
            $overfilledSlots += $slot
        }

        $slotPlans += [pscustomobject]@{
            slot = $slot
            target_variants = $targetVariants
            accepted_asset_count = $slotAccepted.Count
            candidate_asset_count = $slotCandidates.Count
            needs_repaint_asset_count = $slotNeedsRepaint.Count
            candidate_or_accepted_count = $candidateOrAccepted
            post_promotion_accepted_asset_count = $slotAccepted.Count + $slotCandidates.Count
            promotable_candidate_asset_ids = @($slotCandidates | Sort-Object { Get-TilesetProperty $_ "id" "" } | ForEach-Object { Get-TilesetProperty $_ "id" "" })
            excluded_needs_repaint_asset_ids = @($slotNeedsRepaint | Sort-Object { Get-TilesetProperty $_ "id" "" } | ForEach-Object { Get-TilesetProperty $_ "id" "" })
        }
    }

    $linkedDiagnostics = @($diagnostics | Where-Object { $_.work_order_id -eq $workOrderId })
    $exerciseDiagnostics = @($linkedDiagnostics | Where-Object { $_.label -eq "single_road_family_collapse" })
    $promotionBlockingDiagnostics = @($linkedDiagnostics | Where-Object { $_.label -ne "single_road_family_collapse" })
    $runtimeMissingContextualSlots = @(Get-RuntimeMissingContextualSlots $promotionBlockingDiagnostics)
    $evidence = @(Get-ValidationEvidence $validationResults $workOrderId)
    $generatedMapFitEvidence = @(Get-GeneratedMapFitEvidence $evidence $workOrderId)
    $qaErrors = [int](Get-TilesetProperty $qaReport "error_count" 0)
    $promotionGate = Get-PromotionGate $missingSlots $underfilledSlots $overfilledSlots $runtimeMissingContextualSlots.Count $qaErrors $evidence
    $reviewHolds = @()
    foreach ($held in @($evidence | Where-Object { -not $_.accepted -and -not $_.acceptance_ready })) {
        if (-not [string]::IsNullOrWhiteSpace($held.status)) {
            $reviewHolds += $held.status
        }
    }
    if ($needsRepaintAssets.Count -gt 0) {
        $reviewHolds += "exclude_needs_repaint_assets"
    }
    if ($evidence.Count -eq 0) {
        $reviewHolds += "missing_validation_evidence"
    }
    if ($otherAssets.Count -gt 0) {
        $reviewHolds += "nonstandard_status_assets_present"
    }
    if ($runtimeMissingContextualSlots.Count -gt 0) {
        $reviewHolds += "runtime_missing_contextual_slots"
    }

    $coverageItem = if ($coverageById.ContainsKey($workOrderId)) { $coverageById[$workOrderId] } else { $null }
    $workOrderPlans += [pscustomobject]@{
        id = $workOrderId
        manifest = $record.Manifest
        asset_class = Get-TilesetProperty $workOrder "asset_class" ""
        owner = Get-TilesetProperty $workOrder "owner" ""
        coverage_owner = Get-TilesetProductionWorkOrderCoverageOwner $workOrder
        status = Get-TilesetProperty $workOrder "status" ""
        target_asset_count = $targetAssets
        required_slot_count = $requiredSlotCount
        target_variants = $targetVariants
        accepted_asset_count = $acceptedAssets.Count
        candidate_asset_count = $candidateAssets.Count
        needs_repaint_asset_count = $needsRepaintAssets.Count
        other_status_asset_count = $otherAssets.Count
        promotable_candidate_asset_count = $candidateAssets.Count
        excluded_needs_repaint_asset_count = $needsRepaintAssets.Count
        post_promotion_accepted_asset_count = $acceptedAssets.Count + $candidateAssets.Count
        post_promotion_candidate_asset_count = 0
        missing_candidate_or_accepted_slots = $missingSlots
        underfilled_candidate_or_accepted_slots = $underfilledSlots
        overfilled_candidate_or_accepted_slots = $overfilledSlots
        runtime_missing_contextual_slot_count = $runtimeMissingContextualSlots.Count
        runtime_missing_contextual_diagnostic_count = [int](@($runtimeMissingContextualSlots | Measure-Object -Property diagnostic_count -Sum).Sum)
        runtime_missing_contextual_slots = $runtimeMissingContextualSlots
        missing_accepted_slots = @(ConvertTo-TilesetArray (Get-TilesetProperty $coverageItem "missing_accepted_slots" @()))
        promotion_gate = $promotionGate
        next_action = Get-PromotionAction $promotionGate
        review_holds = @($reviewHolds | Select-Object -Unique)
        qa_warning_count = $promotionBlockingDiagnostics.Count
        qa_exercise_diagnostic_count = $exerciseDiagnostics.Count
        qa_candidate_not_runtime_accepted_count = @($promotionBlockingDiagnostics | Where-Object { $_.diagnostic_kind -eq "candidate_not_runtime_accepted" }).Count
        qa_context_or_art_gap_count = @($promotionBlockingDiagnostics | Where-Object { $_.diagnostic_kind -eq "context_or_art_gap" }).Count
        qa_unclassified_runtime_warning_count = @($promotionBlockingDiagnostics | Where-Object { $_.diagnostic_kind -eq "unclassified_runtime_warning" }).Count
        qa_by_kind = @(Get-GroupedCounts $promotionBlockingDiagnostics "diagnostic_kind")
        candidate_preview_diagnostic_count = @($promotionBlockingDiagnostics | Where-Object { -not [string]::IsNullOrWhiteSpace($_.candidate_preview_asset) }).Count
        contextual_coverage_sample_count = [int](Get-TilesetProperty $coverageItem "contextual_coverage_sample_count" 0)
        contextual_sample_candidate_preview_asset_count = [int](Get-TilesetProperty $coverageItem "contextual_sample_candidate_preview_asset_count" 0)
        contextual_sample_candidate_preview_asset_ids = @(ConvertTo-TilesetArray (Get-TilesetProperty $coverageItem "contextual_sample_candidate_preview_asset_ids" @()))
        contextual_coverage_samples_by_label = @(ConvertTo-TilesetArray (Get-TilesetProperty $coverageItem "contextual_coverage_samples_by_label" @()))
        contextual_coverage_samples = @(ConvertTo-TilesetArray (Get-TilesetProperty $coverageItem "contextual_coverage_samples" @()))
        sample_diagnostics = @($promotionBlockingDiagnostics | Select-Object -First 5)
        exercise_diagnostics = @($exerciseDiagnostics | Select-Object -First 5)
        validation_evidence = $evidence
        generated_map_fit_evidence = $generatedMapFitEvidence
        slot_plan = $slotPlans
        promotable_candidate_asset_ids = @($candidateAssets | Sort-Object { Get-TilesetProperty $_ "id" "" } | ForEach-Object { Get-TilesetProperty $_ "id" "" })
        excluded_needs_repaint_asset_ids = @($needsRepaintAssets | Sort-Object { Get-TilesetProperty $_ "id" "" } | ForEach-Object { Get-TilesetProperty $_ "id" "" })
    }
}

$summary = [pscustomobject]@{
    work_order_count = $workOrderPlans.Count
    target_asset_count = ($workOrderPlans | Measure-Object -Property target_asset_count -Sum).Sum
    accepted_asset_count = ($workOrderPlans | Measure-Object -Property accepted_asset_count -Sum).Sum
    candidate_asset_count = ($workOrderPlans | Measure-Object -Property candidate_asset_count -Sum).Sum
    promotable_candidate_asset_count = ($workOrderPlans | Measure-Object -Property promotable_candidate_asset_count -Sum).Sum
    excluded_needs_repaint_asset_count = ($workOrderPlans | Measure-Object -Property excluded_needs_repaint_asset_count -Sum).Sum
    post_promotion_accepted_asset_count = ($workOrderPlans | Measure-Object -Property post_promotion_accepted_asset_count -Sum).Sum
    qa_warning_count = [int](Get-TilesetProperty $qaReport "warning_count" 0)
    qa_error_count = [int](Get-TilesetProperty $qaReport "error_count" 0)
    candidate_not_runtime_accepted_count = @($diagnostics | Where-Object { $_.diagnostic_kind -eq "candidate_not_runtime_accepted" }).Count
    context_or_art_gap_count = @($diagnostics | Where-Object { $_.diagnostic_kind -eq "context_or_art_gap" }).Count
    generator_exercise_gap_count = @($diagnostics | Where-Object { $_.diagnostic_kind -eq "generator_exercise_gap" }).Count
    unclassified_runtime_warning_count = @($diagnostics | Where-Object { $_.diagnostic_kind -eq "unclassified_runtime_warning" }).Count
    blocked_candidate_generation_required_count = @($workOrderPlans | Where-Object { $_.promotion_gate -eq "blocked_candidate_generation_required" }).Count
    blocked_qa_errors_count = @($workOrderPlans | Where-Object { $_.promotion_gate -eq "blocked_qa_errors" }).Count
    blocked_human_art_direction_review_required_count = @($workOrderPlans | Where-Object { $_.promotion_gate -eq "blocked_human_art_direction_review_required" }).Count
    ready_for_manual_status_change_count = @($workOrderPlans | Where-Object { $_.promotion_gate -eq "ready_for_manual_status_change" }).Count
    generated_map_fit_reviewed_work_order_count = @($workOrderPlans | Where-Object { $_.generated_map_fit_evidence.Count -gt 0 }).Count
    generated_map_fit_review_diagnostic_count = Get-PropertySum (@($workOrderPlans | ForEach-Object { $_.generated_map_fit_evidence })) "diagnostic_count"
    generated_map_fit_review_unique_candidate_asset_count = Get-PropertySum (@($workOrderPlans | ForEach-Object { $_.generated_map_fit_evidence })) "unique_candidate_preview_asset_count"
    generated_map_fit_review_sampled_role_coverage_slot_count = Get-PropertySum (@($workOrderPlans | ForEach-Object { $_.generated_map_fit_evidence })) "sampled_role_coverage_slot_count"
    generated_map_fit_review_missing_sampled_role_coverage_slot_count = Get-PropertySum (@($workOrderPlans | ForEach-Object { $_.generated_map_fit_evidence })) "missing_sampled_role_coverage_slot_count"
    contextual_coverage_sample_count = Get-PropertySum $workOrderPlans "contextual_coverage_sample_count"
    contextual_sampled_work_order_count = @($workOrderPlans | Where-Object { [int]$_.contextual_coverage_sample_count -gt 0 }).Count
    contextual_sampled_candidate_preview_asset_count = @($workOrderPlans | ForEach-Object { $_.contextual_sample_candidate_preview_asset_ids } | Sort-Object -Unique).Count
}
$summary | Add-Member -MemberType NoteProperty -Name accepted_completion_percent -Value ([Math]::Round(([double]$summary.accepted_asset_count * 100.0) / [Math]::Max(1, [int]$summary.target_asset_count), 1))
$summary | Add-Member -MemberType NoteProperty -Name post_promotion_completion_percent -Value ([Math]::Round(([double]$summary.post_promotion_accepted_asset_count * 100.0) / [Math]::Max(1, [int]$summary.target_asset_count), 1))
$summary | Add-Member -MemberType NoteProperty -Name runtime_usable -Value ($summary.target_asset_count -gt 0 -and $summary.accepted_asset_count -ge $summary.target_asset_count)
$runtimeUsabilityState = if ($summary.target_asset_count -le 0) {
    "no_contextual_runtime_targets"
}
elseif ($summary.runtime_usable) {
    "accepted_runtime_complete"
}
elseif ($summary.candidate_asset_count -ge $summary.target_asset_count) {
    "candidate_complete_not_runtime_accepted"
}
else {
    "candidate_generation_or_acceptance_required"
}
$summary | Add-Member -MemberType NoteProperty -Name runtime_usability_state -Value $runtimeUsabilityState

$report = [pscustomobject]@{
    schema_version = 1
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    mode = "read_only_no_status_changes"
    source_root = ConvertTo-TilesetRelativePath $resolvedSourceRoot
    coverage_report = ConvertTo-TilesetRelativePath $coveragePath
    qa_report = ConvertTo-TilesetRelativePath $qaPath
    source_qa = $sourceQa
    validation_results = ConvertTo-TilesetRelativePath $validationPath
    summary = $summary
    work_orders = $workOrderPlans
}

$report | ConvertTo-Json -Depth 28 | Set-Content -LiteralPath $jsonPath

$markdown = [System.Text.StringBuilder]::new()
[void]$markdown.AppendLine("# Contextual Promotion Plan")
[void]$markdown.AppendLine("")
[void]$markdown.AppendLine("- Generated: $($report.generated_at_utc)")
[void]$markdown.AppendLine("- Mode: read-only; no asset statuses are changed")
[void]$markdown.AppendLine((Format-TilesetSourceQaLine $sourceQa))
[void]$markdown.AppendLine("- Current accepted coverage: $($summary.accepted_asset_count)/$($summary.target_asset_count) ($(Format-Percent $summary.accepted_completion_percent))")
[void]$markdown.AppendLine(("- Current runtime usability: ``{0}``" -f $summary.runtime_usability_state))
[void]$markdown.AppendLine("- Candidate assets that would be touched if every listed work order were approved: $($summary.promotable_candidate_asset_count)")
[void]$markdown.AppendLine("- Excluded ``needs_repaint`` assets: $($summary.excluded_needs_repaint_asset_count)")
[void]$markdown.AppendLine("- Hypothetical post-promotion accepted coverage: $($summary.post_promotion_accepted_asset_count)/$($summary.target_asset_count) ($(Format-Percent $summary.post_promotion_completion_percent))")
[void]$markdown.AppendLine("- QA split: $($summary.candidate_not_runtime_accepted_count) candidate-not-runtime-accepted, $($summary.context_or_art_gap_count) context-or-art gaps, $($summary.generator_exercise_gap_count) generator-exercise gaps, $($summary.unclassified_runtime_warning_count) unclassified")
[void]$markdown.AppendLine("- Automatic promotion-ready work orders: $($summary.ready_for_manual_status_change_count)")
[void]$markdown.AppendLine("- Candidate generation still required: $($summary.blocked_candidate_generation_required_count)")
[void]$markdown.AppendLine("- Human/art-direction review still required: $($summary.blocked_human_art_direction_review_required_count)")
[void]$markdown.AppendLine("- Generated-map fit evidence: $($summary.generated_map_fit_reviewed_work_order_count) work orders, $($summary.generated_map_fit_review_diagnostic_count) diagnostics, $($summary.generated_map_fit_review_unique_candidate_asset_count) unique candidate previews, $($summary.generated_map_fit_review_sampled_role_coverage_slot_count) sampled role/span slots, $($summary.generated_map_fit_review_missing_sampled_role_coverage_slot_count) missing sampled slots")
[void]$markdown.AppendLine("- Contextual exercise samples: $($summary.contextual_coverage_sample_count) samples across $($summary.contextual_sampled_work_order_count) work orders, $($summary.contextual_sampled_candidate_preview_asset_count) unique candidate previews")
[void]$markdown.AppendLine("")
[void]$markdown.AppendLine("| Work order | Class | Promote | Exclude | Samples | Post accepted | Gate | Next action |")
[void]$markdown.AppendLine("| --- | --- | ---: | ---: | ---: | ---: | --- | --- |")
foreach ($item in $workOrderPlans) {
    [void]$markdown.AppendLine(('| `{0}` | `{1}` | {2} | {3} | {4} | {5}/{6} | `{7}` | {8} |' -f
            $item.id,
            $item.asset_class,
            $item.promotable_candidate_asset_count,
            $item.excluded_needs_repaint_asset_count,
            $item.contextual_coverage_sample_count,
            $item.post_promotion_accepted_asset_count,
            $item.target_asset_count,
            $item.promotion_gate,
            $item.next_action))
}
[void]$markdown.AppendLine("")
[void]$markdown.AppendLine("## Holds")
foreach ($item in $workOrderPlans) {
    [void]$markdown.AppendLine("")
    [void]$markdown.AppendLine("### $($item.id)")
    if ($item.review_holds.Count -eq 0) {
        [void]$markdown.AppendLine("- No review holds recorded.")
    }
    else {
        foreach ($hold in $item.review_holds) {
            [void]$markdown.AppendLine("- $hold")
        }
    }
    if ($item.missing_candidate_or_accepted_slots.Count -gt 0) {
        [void]$markdown.AppendLine("- Missing slots: $($item.missing_candidate_or_accepted_slots -join ', ')")
    }
    if ($item.underfilled_candidate_or_accepted_slots.Count -gt 0) {
        [void]$markdown.AppendLine("- Underfilled slots: $($item.underfilled_candidate_or_accepted_slots -join ', ')")
    }
    if ($item.overfilled_candidate_or_accepted_slots.Count -gt 0) {
        [void]$markdown.AppendLine("- Overfilled slots: $($item.overfilled_candidate_or_accepted_slots -join ', ')")
    }
    if ($item.runtime_missing_contextual_slot_count -gt 0) {
        $topRuntimeSlots = @($item.runtime_missing_contextual_slots | Select-Object -First 3 | ForEach-Object { "$($_.slot) ($($_.diagnostic_count))" })
        [void]$markdown.AppendLine("- Runtime-missing contextual slots: $($item.runtime_missing_contextual_slot_count) exact slots; top: $($topRuntimeSlots -join ', ')")
    }
    foreach ($kind in $item.qa_by_kind) {
        [void]$markdown.AppendLine(('- QA kind `{0}`: {1}' -f $kind.name, $kind.count))
    }
    foreach ($fit in $item.generated_map_fit_evidence) {
        [void]$markdown.AppendLine(('- Generated-map fit evidence `{0}`: {1} diagnostics, {2} unique previews, {3} sampled role/span slots, {4} missing sampled slots; sheet `{5}`' -f
                $fit.validation_id,
                $fit.diagnostic_count,
                $fit.unique_candidate_preview_asset_count,
                $fit.sampled_role_coverage_slot_count,
                $fit.missing_sampled_role_coverage_slot_count,
                $fit.review_sheet))
    }
    foreach ($sample in $item.contextual_coverage_samples) {
        [void]$markdown.AppendLine(('- Contextual sample `{0}` / `{1}`: candidate preview `{2}`, fallback `{3}`' -f
                $sample.case,
                $sample.domain,
                $sample.candidate_preview_asset_id,
                $sample.fallback_reason))
    }
}

$markdown.ToString() | Set-Content -LiteralPath $markdownPath
Write-Host "Contextual promotion plan JSON wrote $jsonPath"
Write-Host "Contextual promotion plan Markdown wrote $markdownPath"
Write-Host "Work orders: $($summary.work_order_count); promotable candidates: $($summary.promotable_candidate_asset_count); auto-ready: $($summary.ready_for_manual_status_change_count)"
