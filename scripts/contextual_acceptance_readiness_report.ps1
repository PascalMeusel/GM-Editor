param(
    [string]$CoverageReportPath = "build/style_coverage_report_structure_way_ground_resolution.json",
    [string]$QaReportPath = "build/map-generation-review-structure-way-ground-resolution/map_generation_batch_report.json",
    [string]$ValidationResultsPath = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2/validation_results.json",
    [string]$OutputJsonPath = "build/contextual_acceptance_readiness_structure_way_ground_resolution.json",
    [string]$OutputMarkdownPath = ""
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")

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
            rating = Get-TilesetProperty $entry "rating" 0
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

function Get-ReadinessState($WorkOrder, $Evidence, [int]$RuntimeMissingContextualSlotCount) {
    $target = [int](Get-TilesetProperty $WorkOrder "target_asset_count" 0)
    $accepted = [int](Get-TilesetProperty $WorkOrder "accepted_asset_count" 0)
    $candidate = [int](Get-TilesetProperty $WorkOrder "candidate_asset_count" 0)
    $missingCandidateSlots = @(ConvertTo-TilesetArray (Get-TilesetProperty $WorkOrder "missing_candidate_or_accepted_slots" @()))
    $underfilledSlots = @(ConvertTo-TilesetArray (Get-TilesetProperty $WorkOrder "underfilled_candidate_or_accepted_slots" @()))

    if ($accepted -ge $target -and $target -gt 0) {
        return "accepted_runtime_complete"
    }
    if ($missingCandidateSlots.Count -gt 0 -or $underfilledSlots.Count -gt 0 -or $candidate -lt $target -or $RuntimeMissingContextualSlotCount -gt 0) {
        return "candidate_generation_required"
    }
    $latestEvidence = Get-LatestValidationEvidence $Evidence
    if ($null -ne $latestEvidence -and ($latestEvidence.accepted -or $latestEvidence.acceptance_ready)) {
        return "candidate_complete_ready_for_promotion_decision"
    }
    return "candidate_complete_human_review_required"
}

function Get-NextAction($State) {
    switch ($State) {
        "accepted_runtime_complete" { return "Keep in runtime validation." }
        "candidate_generation_required" { return "Generate or repaint the missing/underfilled candidate slots." }
        "candidate_complete_ready_for_promotion_decision" { return "Review evidence and promote approved assets to accepted." }
    }
    return "Human/art-direction review, then promote or repaint."
}

function Format-Percent($Value) {
    return [string]::Format([System.Globalization.CultureInfo]::InvariantCulture, "{0:0.0}%", [double]$Value)
}

$coveragePath = Resolve-TilesetPath $CoverageReportPath
$qaPath = Resolve-TilesetPath $QaReportPath
$validationPath = Resolve-TilesetPath $ValidationResultsPath
$jsonPath = Resolve-TilesetPath $OutputJsonPath
if ([string]::IsNullOrWhiteSpace($OutputMarkdownPath)) {
    $OutputMarkdownPath = [System.IO.Path]::ChangeExtension($OutputJsonPath, ".md")
}
$markdownPath = Resolve-TilesetPath $OutputMarkdownPath

if (-not (Test-Path -LiteralPath $coveragePath)) {
    throw "Missing coverage report: $coveragePath"
}
if (-not (Test-Path -LiteralPath $qaPath)) {
    throw "Missing QA report: $qaPath"
}

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $jsonPath) | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $markdownPath) | Out-Null

$coverage = Get-Content -Raw -LiteralPath $coveragePath | ConvertFrom-Json
$qaReport = Get-Content -Raw -LiteralPath $qaPath | ConvertFrom-Json
$validationResults = Read-TilesetJson $validationPath ([pscustomobject]@{ results = @() })
$diagnostics = @(Get-QaDiagnostics $qaReport)
foreach ($diagnostic in $diagnostics) {
    $diagnostic | Add-Member -MemberType NoteProperty -Name diagnostic_kind -Value (Get-TilesetContextualDiagnosticKind $diagnostic)
}

$workOrderReadiness = @()
foreach ($workOrder in (ConvertTo-TilesetArray (Get-TilesetProperty $coverage "work_orders" @()))) {
    $workOrderId = Get-TilesetProperty $workOrder "id" ""
    $linkedDiagnostics = @($diagnostics | Where-Object { $_.work_order_id -eq $workOrderId })
    $exerciseDiagnostics = @($linkedDiagnostics | Where-Object { $_.label -eq "single_road_family_collapse" })
    $promotionBlockingDiagnostics = @($linkedDiagnostics | Where-Object { $_.label -ne "single_road_family_collapse" })
    $runtimeMissingContextualSlots = @(Get-RuntimeMissingContextualSlots $promotionBlockingDiagnostics)
    $evidence = @(Get-ValidationEvidence $validationResults $workOrderId)
    $generatedMapFitEvidence = @(Get-GeneratedMapFitEvidence $evidence $workOrderId)
    $state = Get-ReadinessState $workOrder $evidence $runtimeMissingContextualSlots.Count
    $missingCandidateSlots = @(ConvertTo-TilesetArray (Get-TilesetProperty $workOrder "missing_candidate_or_accepted_slots" @()))
    $underfilledCandidateSlots = @(ConvertTo-TilesetArray (Get-TilesetProperty $workOrder "underfilled_candidate_or_accepted_slots" @()))
    $contextualCoverageSamples = @(ConvertTo-TilesetArray (Get-TilesetProperty $workOrder "contextual_coverage_samples" @()))
    $contextualCoverageSamplesByLabel = @(ConvertTo-TilesetArray (Get-TilesetProperty $workOrder "contextual_coverage_samples_by_label" @()))
    $blockers = @()

    if ($missingCandidateSlots.Count -gt 0) {
        $blockers += "missing candidate slots: " + ($missingCandidateSlots -join ", ")
    }
    if ($underfilledCandidateSlots.Count -gt 0) {
        $blockers += "underfilled candidate slots: " + ($underfilledCandidateSlots -join ", ")
    }
    if ($runtimeMissingContextualSlots.Count -gt 0) {
        $topRuntimeSlots = @($runtimeMissingContextualSlots | Select-Object -First 3 | ForEach-Object { "$($_.slot) ($($_.diagnostic_count))" })
        $blockers += "runtime-discovered missing contextual slots: $($runtimeMissingContextualSlots.Count) exact slots; top: " + ($topRuntimeSlots -join ", ")
    }

    $target = [int](Get-TilesetProperty $workOrder "target_asset_count" 0)
    $accepted = [int](Get-TilesetProperty $workOrder "accepted_asset_count" 0)
    if ($accepted -lt $target) {
        $blockers += "$accepted/$target accepted runtime assets"
    }
    foreach ($held in @($evidence | Where-Object { -not $_.acceptance_ready })) {
        if (-not [string]::IsNullOrWhiteSpace($held.status)) {
            $blockers += "validation hold: $($held.status)"
        }
    }

    $workOrderReadiness += [pscustomobject]@{
        id = $workOrderId
        manifest = Get-TilesetProperty $workOrder "manifest" ""
        asset_class = Get-TilesetProperty $workOrder "asset_class" ""
        owner = Get-TilesetProperty $workOrder "owner" ""
        coverage_owner = Get-TilesetProperty $workOrder "coverage_owner" ""
        target_asset_count = $target
        accepted_asset_count = $accepted
        candidate_asset_count = [int](Get-TilesetProperty $workOrder "candidate_asset_count" 0)
        needs_repaint_asset_count = [int](Get-TilesetProperty $workOrder "needs_repaint_asset_count" 0)
        accepted_completion_percent = [double](Get-TilesetProperty $workOrder "accepted_completion_percent" 0)
        candidate_or_accepted_percent = [double](Get-TilesetProperty $workOrder "candidate_or_accepted_percent" 0)
        missing_candidate_or_accepted_slots = $missingCandidateSlots
        underfilled_candidate_or_accepted_slots = $underfilledCandidateSlots
        runtime_missing_contextual_slot_count = $runtimeMissingContextualSlots.Count
        runtime_missing_contextual_diagnostic_count = [int](@($runtimeMissingContextualSlots | Measure-Object -Property diagnostic_count -Sum).Sum)
        runtime_missing_contextual_slots = $runtimeMissingContextualSlots
        missing_accepted_slots = @(ConvertTo-TilesetArray (Get-TilesetProperty $workOrder "missing_accepted_slots" @()))
        readiness_state = $state
        next_action = Get-NextAction $state
        blockers = @($blockers | Select-Object -Unique)
        qa_warning_count = $promotionBlockingDiagnostics.Count
        qa_exercise_diagnostic_count = $exerciseDiagnostics.Count
        qa_candidate_not_runtime_accepted_count = @($promotionBlockingDiagnostics | Where-Object { $_.diagnostic_kind -eq "candidate_not_runtime_accepted" }).Count
        qa_context_or_art_gap_count = @($promotionBlockingDiagnostics | Where-Object { $_.diagnostic_kind -eq "context_or_art_gap" }).Count
        qa_unclassified_runtime_warning_count = @($promotionBlockingDiagnostics | Where-Object { $_.diagnostic_kind -eq "unclassified_runtime_warning" }).Count
        qa_by_kind = @(Get-GroupedCounts $promotionBlockingDiagnostics "diagnostic_kind")
        qa_by_label = @(Get-GroupedCounts $promotionBlockingDiagnostics "label")
        qa_by_fallback_reason = @(Get-GroupedCounts $promotionBlockingDiagnostics "fallback_reason")
        candidate_preview_diagnostic_count = @($promotionBlockingDiagnostics | Where-Object { -not [string]::IsNullOrWhiteSpace($_.candidate_preview_asset) }).Count
        contextual_coverage_sample_count = [int](Get-TilesetProperty $workOrder "contextual_coverage_sample_count" 0)
        contextual_sample_candidate_preview_asset_count = [int](Get-TilesetProperty $workOrder "contextual_sample_candidate_preview_asset_count" 0)
        contextual_sample_candidate_preview_asset_ids = @(ConvertTo-TilesetArray (Get-TilesetProperty $workOrder "contextual_sample_candidate_preview_asset_ids" @()))
        contextual_coverage_samples_by_label = $contextualCoverageSamplesByLabel
        contextual_coverage_samples = $contextualCoverageSamples
        sample_diagnostics = @($promotionBlockingDiagnostics | Select-Object -First 5)
        exercise_diagnostics = @($exerciseDiagnostics | Select-Object -First 5)
        validation_evidence = $evidence
        generated_map_fit_evidence = $generatedMapFitEvidence
    }
}

$unlinkedDiagnostics = @($diagnostics | Where-Object {
    [string]::IsNullOrWhiteSpace($_.work_order_id) -and
    $_.label -in @("style_family_fallback", "single_road_family_collapse", "bridge_middle_repetition", "range_not_multitile", "tower_cave_grass_halo", "component_biome_drift", "road_overlay_shortcut")
})

$summary = Get-TilesetProperty $coverage "summary" ([pscustomobject]@{})
$sourceQa = Get-TilesetProperty $coverage "source_qa" $null
$report = [pscustomobject]@{
    schema_version = 1
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    coverage_report = ConvertTo-TilesetRelativePath $coveragePath
    qa_report = ConvertTo-TilesetRelativePath $qaPath
    source_qa = $sourceQa
    validation_results = ConvertTo-TilesetRelativePath $validationPath
    summary = [pscustomobject]@{
        work_order_count = $workOrderReadiness.Count
        target_asset_count = [int](Get-TilesetProperty $summary "target_asset_count" 0)
        accepted_asset_count = [int](Get-TilesetProperty $summary "accepted_asset_count" 0)
        candidate_asset_count = [int](Get-TilesetProperty $summary "candidate_asset_count" 0)
        accepted_completion_percent = [double](Get-TilesetProperty $summary "accepted_completion_percent" 0)
        candidate_or_accepted_percent = [double](Get-TilesetProperty $summary "candidate_or_accepted_percent" 0)
        qa_warning_count = [int](Get-TilesetProperty $qaReport "warning_count" 0)
        qa_error_count = [int](Get-TilesetProperty $qaReport "error_count" 0)
        candidate_not_runtime_accepted_count = @($diagnostics | Where-Object { $_.diagnostic_kind -eq "candidate_not_runtime_accepted" }).Count
        context_or_art_gap_count = @($diagnostics | Where-Object { $_.diagnostic_kind -eq "context_or_art_gap" }).Count
        generator_exercise_gap_count = @($diagnostics | Where-Object { $_.diagnostic_kind -eq "generator_exercise_gap" }).Count
        unclassified_runtime_warning_count = @($diagnostics | Where-Object { $_.diagnostic_kind -eq "unclassified_runtime_warning" }).Count
        candidate_generation_required_count = @($workOrderReadiness | Where-Object { $_.readiness_state -eq "candidate_generation_required" }).Count
        candidate_complete_review_required_count = @($workOrderReadiness | Where-Object { $_.readiness_state -eq "candidate_complete_human_review_required" }).Count
        accepted_runtime_complete_count = @($workOrderReadiness | Where-Object { $_.readiness_state -eq "accepted_runtime_complete" }).Count
        single_road_family_exercise_diagnostic_count = @($diagnostics | Where-Object { $_.label -eq "single_road_family_collapse" }).Count
        unlinked_contextual_diagnostic_count = $unlinkedDiagnostics.Count
        generated_map_fit_reviewed_work_order_count = @($workOrderReadiness | Where-Object { $_.generated_map_fit_evidence.Count -gt 0 }).Count
        generated_map_fit_review_diagnostic_count = Get-PropertySum (@($workOrderReadiness | ForEach-Object { $_.generated_map_fit_evidence })) "diagnostic_count"
        generated_map_fit_review_unique_candidate_asset_count = Get-PropertySum (@($workOrderReadiness | ForEach-Object { $_.generated_map_fit_evidence })) "unique_candidate_preview_asset_count"
        generated_map_fit_review_sampled_role_coverage_slot_count = Get-PropertySum (@($workOrderReadiness | ForEach-Object { $_.generated_map_fit_evidence })) "sampled_role_coverage_slot_count"
        generated_map_fit_review_missing_sampled_role_coverage_slot_count = Get-PropertySum (@($workOrderReadiness | ForEach-Object { $_.generated_map_fit_evidence })) "missing_sampled_role_coverage_slot_count"
        contextual_coverage_sample_count = [int](Get-TilesetProperty $summary "contextual_coverage_sample_count" 0)
        contextual_sampled_work_order_count = [int](Get-TilesetProperty $summary "contextual_sampled_work_order_count" 0)
        contextual_sampled_candidate_preview_asset_count = [int](Get-TilesetProperty $summary "contextual_sampled_candidate_preview_asset_count" 0)
    }
    work_orders = $workOrderReadiness
    unlinked_contextual_diagnostics = @($unlinkedDiagnostics | Select-Object -First 40)
}

$report | ConvertTo-Json -Depth 24 | Set-Content -LiteralPath $jsonPath

$markdown = [System.Text.StringBuilder]::new()
[void]$markdown.AppendLine("# Contextual Acceptance Readiness")
[void]$markdown.AppendLine("")
[void]$markdown.AppendLine("- Generated: $($report.generated_at_utc)")
[void]$markdown.AppendLine((Format-TilesetSourceQaLine $sourceQa))
[void]$markdown.AppendLine("- Coverage: $($report.summary.accepted_asset_count)/$($report.summary.target_asset_count) accepted, $($report.summary.candidate_asset_count) candidates, $(Format-Percent $report.summary.candidate_or_accepted_percent) candidate-or-accepted coverage")
[void]$markdown.AppendLine("- QA: $($report.summary.qa_error_count) errors, $($report.summary.qa_warning_count) warnings")
[void]$markdown.AppendLine("- QA split: $($report.summary.candidate_not_runtime_accepted_count) candidate-not-runtime-accepted, $($report.summary.context_or_art_gap_count) context-or-art gaps, $($report.summary.generator_exercise_gap_count) generator-exercise gaps, $($report.summary.unclassified_runtime_warning_count) unclassified")
[void]$markdown.AppendLine("- Single-road-family exercise diagnostics: $($report.summary.single_road_family_exercise_diagnostic_count) (tracked separately from promotion-blocking contextual warnings)")
[void]$markdown.AppendLine("- Candidate generation required: $($report.summary.candidate_generation_required_count) work orders")
[void]$markdown.AppendLine("- Candidate-complete, human review required: $($report.summary.candidate_complete_review_required_count) work orders")
[void]$markdown.AppendLine("- Generated-map fit evidence: $($report.summary.generated_map_fit_reviewed_work_order_count) work orders, $($report.summary.generated_map_fit_review_diagnostic_count) diagnostics, $($report.summary.generated_map_fit_review_unique_candidate_asset_count) unique candidate previews, $($report.summary.generated_map_fit_review_sampled_role_coverage_slot_count) sampled role/span slots, $($report.summary.generated_map_fit_review_missing_sampled_role_coverage_slot_count) missing sampled slots")
[void]$markdown.AppendLine("- Contextual exercise samples: $($report.summary.contextual_coverage_sample_count) samples across $($report.summary.contextual_sampled_work_order_count) work orders, $($report.summary.contextual_sampled_candidate_preview_asset_count) unique candidate previews")
[void]$markdown.AppendLine("")
[void]$markdown.AppendLine("| Work order | Class | Accepted | Candidates | QA warnings | Samples | State | Next action |")
[void]$markdown.AppendLine("| --- | --- | ---: | ---: | ---: | ---: | --- | --- |")
foreach ($item in $workOrderReadiness) {
    [void]$markdown.AppendLine(('| `{0}` | `{1}` | {2}/{3} | {4} | {5} | {6} | `{7}` | {8} |' -f
            $item.id,
            $item.asset_class,
            $item.accepted_asset_count,
            $item.target_asset_count,
            $item.candidate_asset_count,
            $item.qa_warning_count,
            $item.contextual_coverage_sample_count,
            $item.readiness_state,
            $item.next_action))
}
[void]$markdown.AppendLine("")
[void]$markdown.AppendLine("## Blockers")
foreach ($item in $workOrderReadiness) {
    [void]$markdown.AppendLine("")
    [void]$markdown.AppendLine("### $($item.id)")
    if ($item.blockers.Count -eq 0) {
        [void]$markdown.AppendLine("- No blockers recorded.")
    }
    else {
        foreach ($blocker in $item.blockers) {
            [void]$markdown.AppendLine("- $blocker")
        }
    }
    foreach ($reason in $item.qa_by_fallback_reason) {
        [void]$markdown.AppendLine(('- QA `{0}`: {1}' -f $reason.name, $reason.count))
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
Write-Host "Contextual acceptance readiness JSON wrote $jsonPath"
Write-Host "Contextual acceptance readiness Markdown wrote $markdownPath"
Write-Host "Work orders: $($workOrderReadiness.Count); accepted: $($report.summary.accepted_asset_count)/$($report.summary.target_asset_count); candidates: $($report.summary.candidate_asset_count)"
