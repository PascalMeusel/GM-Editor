param(
    [string]$ReadinessReportPath = "build/contextual_acceptance_readiness_structure_way_ground_resolution.json",
    [string]$PromotionPlanPath = "build/contextual_promotion_plan_structure_way_ground_resolution.json",
    [string]$ValidationResultsPath = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2/validation_results.json",
    [string]$OutputDirectory = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2/review/contextual_candidates",
    [string]$OutputStem = "contextual_human_review_packet",
    [int]$MaxValidationEntriesPerWorkOrder = 6
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")

function Format-JsonTwoSpace([string]$Json) {
    $builder = [System.Text.StringBuilder]::new()
    $indent = 0
    $inString = $false
    $escaped = $false
    for ($index = 0; $index -lt $Json.Length; $index++) {
        $char = $Json[$index]
        if ($inString) {
            [void]$builder.Append($char)
            if ($escaped) {
                $escaped = $false
            }
            elseif ($char -eq "\") {
                $escaped = $true
            }
            elseif ($char -eq '"') {
                $inString = $false
            }
            continue
        }

        switch ($char) {
            '"' {
                $inString = $true
                [void]$builder.Append($char)
            }
            { $_ -eq "{" -or $_ -eq "[" } {
                $nextIndex = $index + 1
                if ($nextIndex -lt $Json.Length) {
                    $nextChar = $Json[$nextIndex]
                    if (($char -eq "{" -and $nextChar -eq "}") -or ($char -eq "[" -and $nextChar -eq "]")) {
                        [void]$builder.Append($char)
                        [void]$builder.Append($nextChar)
                        $index++
                        continue
                    }
                }
                [void]$builder.Append($char)
                $indent++
                [void]$builder.Append("`n")
                [void]$builder.Append(("  " * $indent))
            }
            { $_ -eq "}" -or $_ -eq "]" } {
                $indent = [Math]::Max(0, $indent - 1)
                [void]$builder.Append("`n")
                [void]$builder.Append(("  " * $indent))
                [void]$builder.Append($char)
            }
            "," {
                [void]$builder.Append($char)
                [void]$builder.Append("`n")
                [void]$builder.Append(("  " * $indent))
            }
            ":" {
                [void]$builder.Append(": ")
            }
            default {
                [void]$builder.Append($char)
            }
        }
    }
    return $builder.ToString()
}

function Write-CleanJsonFile([string]$Path, $Value) {
    $json = $Value | ConvertTo-Json -Depth 32 -Compress
    if ($json -is [array]) {
        $json = $json -join "`n"
    }
    $json = $json -replace "`r`n", "`n"
    $json = $json -replace "`r", "`n"
    $json = $json -replace "\\u003c", "<"
    $json = $json -replace "\\u003e", ">"
    $json = $json -replace "\\u0026", "&"
    $json = Format-JsonTwoSpace $json
    $json = (($json -split "`n") | ForEach-Object { $_.TrimEnd() }) -join "`n"
    [System.IO.File]::WriteAllText($Path, $json.TrimEnd() + "`n", [System.Text.UTF8Encoding]::new($false))
}

function Write-CleanTextFile([string]$Path, [string]$Text) {
    $normalized = $Text -replace "`r`n", "`n"
    $normalized = $normalized -replace "`r", "`n"
    $normalized = (($normalized -split "`n") | ForEach-Object { $_.TrimEnd() }) -join "`n"
    [System.IO.File]::WriteAllText($Path, $normalized.TrimEnd() + "`n", [System.Text.UTF8Encoding]::new($false))
}

function Test-EvidencePath([string]$Value) {
    if ([string]::IsNullOrWhiteSpace($Value)) {
        return $false
    }
    return $Value -match "^(Resources|build|docs)/" -or $Value -match "\.(png|json|md)$"
}

function Get-EvidenceLinks($Evidence, [string]$Prefix = "") {
    $items = @()
    if ($null -eq $Evidence) {
        return $items
    }

    if ($Evidence -is [string]) {
        if (Test-EvidencePath $Evidence) {
            $items += [pscustomobject]@{ key = $Prefix; path = $Evidence }
        }
        return $items
    }

    if ($Evidence -is [System.Collections.IEnumerable] -and $Evidence -isnot [string]) {
        $index = 0
        foreach ($entry in $Evidence) {
            $childPrefix = if ([string]::IsNullOrWhiteSpace($Prefix)) { "[$index]" } else { "$Prefix[$index]" }
            $items += @(Get-EvidenceLinks $entry $childPrefix)
            $index++
        }
        return $items
    }

    foreach ($property in $Evidence.PSObject.Properties) {
        $key = if ([string]::IsNullOrWhiteSpace($Prefix)) { $property.Name } else { "$Prefix.$($property.Name)" }
        $items += @(Get-EvidenceLinks $property.Value $key)
    }
    return $items
}

function Get-ParsedDate($Value) {
    $parsed = [DateTime]::MinValue
    if (-not [string]::IsNullOrWhiteSpace([string]$Value)) {
        [void][DateTime]::TryParse(
            [string]$Value,
            [System.Globalization.CultureInfo]::InvariantCulture,
            [System.Globalization.DateTimeStyles]::AssumeUniversal,
            [ref]$parsed)
    }
    return $parsed
}

function Get-PromotionPlanById($PromotionPlan) {
    $items = @{}
    foreach ($workOrder in (ConvertTo-TilesetArray (Get-TilesetProperty $PromotionPlan "work_orders" @()))) {
        $id = Get-TilesetProperty $workOrder "id" ""
        if (-not [string]::IsNullOrWhiteSpace($id)) {
            $items[$id] = $workOrder
        }
    }
    return $items
}

function Get-ValidationEntrySummary($Entry) {
    $links = @(Get-EvidenceLinks (Get-TilesetProperty $Entry "evidence" ([pscustomobject]@{})))
    $links = @($links | Sort-Object key, path -Unique)
    return [pscustomobject]@{
        id = Get-TilesetProperty $Entry "id" ""
        result_type = Get-TilesetProperty $Entry "result_type" ""
        reviewed_at_utc = Get-TilesetProperty $Entry "reviewed_at_utc" ""
        status = Get-TilesetProperty $Entry "status" ""
        rating = [int](Get-TilesetProperty $Entry "rating" 0)
        issue_labels = @(ConvertTo-TilesetArray (Get-TilesetProperty $Entry "issue_labels" @()))
        candidate_pass = [bool](Get-TilesetProperty $Entry "candidate_pass" $false)
        candidate_only = [bool](Get-TilesetProperty $Entry "candidate_only" $false)
        acceptance_ready = [bool](Get-TilesetProperty $Entry "acceptance_ready" $false)
        accepted = [bool](Get-TilesetProperty $Entry "accepted" $false)
        notes = Get-TilesetProperty $Entry "notes" ""
        evidence_links = $links
    }
}

function Get-ReviewAttentionFlags($ValidationEvidence, $GeneratedMapFitEvidence, [int]$QaWarningCount, [int]$ContextualCoverageSampleCount) {
    $flags = @()
    $issueLabels = @($ValidationEvidence | ForEach-Object {
            ConvertTo-TilesetArray (Get-TilesetProperty $_ "issue_labels" @())
        } | ForEach-Object { [string]$_ })

    if ($GeneratedMapFitEvidence.Count -eq 0) {
        $flags += "no_generated_map_fit_evidence_in_current_baseline"
    }
    if ($QaWarningCount -eq 0 -and $ContextualCoverageSampleCount -eq 0) {
        $flags += "no_current_structured_qa_warning_or_sample"
    }
    if ($GeneratedMapFitEvidence.Count -eq 0 -and @($issueLabels | Where-Object {
            $_ -match "generated_map_review_required|map_scale_review_pending|needs_app_scale_review"
        }).Count -gt 0) {
        $flags += "targeted_generated_map_review_needed"
    }
    return @($flags | Select-Object -Unique)
}

$readinessPath = Resolve-TilesetPath $ReadinessReportPath
$promotionPath = Resolve-TilesetPath $PromotionPlanPath
$validationPath = Resolve-TilesetPath $ValidationResultsPath
$outputDirectoryPath = Resolve-TilesetPath $OutputDirectory
$jsonPath = Join-Path $outputDirectoryPath "$OutputStem.json"
$markdownPath = Join-Path $outputDirectoryPath "$OutputStem.md"

foreach ($path in @($readinessPath, $promotionPath, $validationPath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing input: $path"
    }
}
New-Item -ItemType Directory -Force -Path $outputDirectoryPath | Out-Null

$readiness = Get-Content -Raw -LiteralPath $readinessPath | ConvertFrom-Json
$promotionPlan = Get-Content -Raw -LiteralPath $promotionPath | ConvertFrom-Json
$sourceQa = Get-TilesetProperty $readiness "source_qa" (Get-TilesetProperty $promotionPlan "source_qa" $null)
$promotionById = Get-PromotionPlanById $promotionPlan
$workOrderPackets = @()

foreach ($readinessItem in (ConvertTo-TilesetArray (Get-TilesetProperty $readiness "work_orders" @()))) {
    $id = Get-TilesetProperty $readinessItem "id" ""
    if ([string]::IsNullOrWhiteSpace($id)) {
        continue
    }
    $plan = if ($promotionById.ContainsKey($id)) { $promotionById[$id] } else { $null }
    $validationEvidence = @(ConvertTo-TilesetArray (Get-TilesetProperty $readinessItem "validation_evidence" @()))
    $latestValidation = @($validationEvidence | Sort-Object -Descending -Property @{ Expression = { Get-ParsedDate (Get-TilesetProperty $_ "reviewed_at_utc" "") } } | Select-Object -First $MaxValidationEntriesPerWorkOrder)
    $latestValidationSummaries = @($latestValidation | ForEach-Object { Get-ValidationEntrySummary $_ })
    $latestValidationSummary = if ($latestValidationSummaries.Count -gt 0) { $latestValidationSummaries[0] } else { $null }
    $generatedMapFitEvidence = @(ConvertTo-TilesetArray (Get-TilesetProperty $readinessItem "generated_map_fit_evidence" @()))
    $contextualCoverageSamples = @(ConvertTo-TilesetArray (Get-TilesetProperty $readinessItem "contextual_coverage_samples" @()))
    $reviewHolds = @(ConvertTo-TilesetArray (Get-TilesetProperty $plan "review_holds" @()) | ForEach-Object { [string]$_ })
    $qaWarningCount = [int](Get-TilesetProperty $readinessItem "qa_warning_count" 0)
    $contextualCoverageSampleCount = [int](Get-TilesetProperty $readinessItem "contextual_coverage_sample_count" 0)
    $attentionFlags = @(Get-ReviewAttentionFlags $validationEvidence $generatedMapFitEvidence $qaWarningCount $contextualCoverageSampleCount)

    $workOrderPackets += [pscustomobject]@{
        id = $id
        asset_class = Get-TilesetProperty $readinessItem "asset_class" ""
        readiness_state = Get-TilesetProperty $readinessItem "readiness_state" ""
        promotion_gate = Get-TilesetProperty $plan "promotion_gate" ""
        next_action = Get-TilesetProperty $readinessItem "next_action" ""
        accepted_asset_count = [int](Get-TilesetProperty $readinessItem "accepted_asset_count" 0)
        target_asset_count = [int](Get-TilesetProperty $readinessItem "target_asset_count" 0)
        candidate_asset_count = [int](Get-TilesetProperty $readinessItem "candidate_asset_count" 0)
        promotable_candidate_asset_count = [int](Get-TilesetProperty $plan "promotable_candidate_asset_count" 0)
        excluded_needs_repaint_asset_count = [int](Get-TilesetProperty $plan "excluded_needs_repaint_asset_count" 0)
        qa_warning_count = $qaWarningCount
        candidate_preview_diagnostic_count = [int](Get-TilesetProperty $readinessItem "candidate_preview_diagnostic_count" 0)
        contextual_coverage_sample_count = $contextualCoverageSampleCount
        contextual_sample_candidate_preview_asset_count = [int](Get-TilesetProperty $readinessItem "contextual_sample_candidate_preview_asset_count" 0)
        contextual_coverage_samples = $contextualCoverageSamples
        generated_map_fit_evidence_count = $generatedMapFitEvidence.Count
        generated_map_fit_evidence = $generatedMapFitEvidence
        review_attention_flags = $attentionFlags
        review_holds = $reviewHolds
        blockers = @(ConvertTo-TilesetArray (Get-TilesetProperty $readinessItem "blockers" @()) | ForEach-Object { [string]$_ })
        latest_validation = $latestValidationSummary
        validation_evidence_count = $validationEvidence.Count
        shown_validation_evidence_count = $latestValidationSummaries.Count
        validation_evidence = $latestValidationSummaries
    }
}

$summary = [pscustomobject]@{
    work_order_count = $workOrderPackets.Count
    accepted_asset_count = [int](Get-TilesetProperty (Get-TilesetProperty $readiness "summary" ([pscustomobject]@{})) "accepted_asset_count" 0)
    target_asset_count = [int](Get-TilesetProperty (Get-TilesetProperty $readiness "summary" ([pscustomobject]@{})) "target_asset_count" 0)
    candidate_asset_count = [int](Get-TilesetProperty (Get-TilesetProperty $readiness "summary" ([pscustomobject]@{})) "candidate_asset_count" 0)
    candidate_generation_required_count = [int](Get-TilesetProperty (Get-TilesetProperty $readiness "summary" ([pscustomobject]@{})) "candidate_generation_required_count" 0)
    human_review_required_count = [int](Get-TilesetProperty (Get-TilesetProperty $readiness "summary" ([pscustomobject]@{})) "candidate_complete_review_required_count" 0)
    promotion_ready_count = [int](Get-TilesetProperty (Get-TilesetProperty $promotionPlan "summary" ([pscustomobject]@{})) "ready_for_manual_status_change_count" 0)
    generated_map_fit_reviewed_work_order_count = [int](Get-TilesetProperty (Get-TilesetProperty $readiness "summary" ([pscustomobject]@{})) "generated_map_fit_reviewed_work_order_count" 0)
    generated_map_fit_review_diagnostic_count = [int](Get-TilesetProperty (Get-TilesetProperty $readiness "summary" ([pscustomobject]@{})) "generated_map_fit_review_diagnostic_count" 0)
    generated_map_fit_review_unique_candidate_asset_count = [int](Get-TilesetProperty (Get-TilesetProperty $readiness "summary" ([pscustomobject]@{})) "generated_map_fit_review_unique_candidate_asset_count" 0)
    generated_map_fit_review_sampled_role_coverage_slot_count = [int](Get-TilesetProperty (Get-TilesetProperty $readiness "summary" ([pscustomobject]@{})) "generated_map_fit_review_sampled_role_coverage_slot_count" 0)
    generated_map_fit_review_missing_sampled_role_coverage_slot_count = [int](Get-TilesetProperty (Get-TilesetProperty $readiness "summary" ([pscustomobject]@{})) "generated_map_fit_review_missing_sampled_role_coverage_slot_count" 0)
    contextual_coverage_sample_count = [int](Get-TilesetProperty (Get-TilesetProperty $readiness "summary" ([pscustomobject]@{})) "contextual_coverage_sample_count" 0)
    contextual_sampled_work_order_count = [int](Get-TilesetProperty (Get-TilesetProperty $readiness "summary" ([pscustomobject]@{})) "contextual_sampled_work_order_count" 0)
    contextual_sampled_candidate_preview_asset_count = [int](Get-TilesetProperty (Get-TilesetProperty $readiness "summary" ([pscustomobject]@{})) "contextual_sampled_candidate_preview_asset_count" 0)
    work_orders_without_generated_map_fit_evidence_count = @($workOrderPackets | Where-Object { $_.generated_map_fit_evidence_count -eq 0 }).Count
    work_orders_without_contextual_coverage_samples_count = @($workOrderPackets | Where-Object { $_.contextual_coverage_sample_count -eq 0 }).Count
    targeted_generated_map_review_needed_count = @($workOrderPackets | Where-Object { "targeted_generated_map_review_needed" -in $_.review_attention_flags }).Count
}
$summary | Add-Member -NotePropertyName "runtime_usable" -NotePropertyValue ($summary.target_asset_count -gt 0 -and $summary.accepted_asset_count -ge $summary.target_asset_count)
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
$summary | Add-Member -NotePropertyName "runtime_usability_state" -NotePropertyValue $runtimeUsabilityState

$report = [pscustomobject]@{
    schema_version = 1
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    mode = "candidate_only_human_review_packet"
    readiness_report = ConvertTo-TilesetRelativePath $readinessPath
    promotion_plan = ConvertTo-TilesetRelativePath $promotionPath
    source_qa = $sourceQa
    validation_results = ConvertTo-TilesetRelativePath $validationPath
    output_markdown = ConvertTo-TilesetRelativePath $markdownPath
    summary = $summary
    work_orders = $workOrderPackets
}
Write-CleanJsonFile $jsonPath $report

$markdown = [System.Text.StringBuilder]::new()
[void]$markdown.AppendLine("# Contextual Human Review Packet")
[void]$markdown.AppendLine("")
[void]$markdown.AppendLine("- Generated: $($report.generated_at_utc)")
[void]$markdown.AppendLine("- Mode: candidate-only review packet; no asset statuses are changed")
[void]$markdown.AppendLine((Format-TilesetSourceQaLine $sourceQa))
[void]$markdown.AppendLine("- Accepted runtime coverage: $($summary.accepted_asset_count)/$($summary.target_asset_count)")
[void]$markdown.AppendLine(("- Runtime usability: ``{0}``" -f $summary.runtime_usability_state))
[void]$markdown.AppendLine("- Candidate coverage: $($summary.candidate_asset_count) candidates")
[void]$markdown.AppendLine("- Candidate generation required: $($summary.candidate_generation_required_count) work orders")
[void]$markdown.AppendLine("- Human/art-direction review required: $($summary.human_review_required_count) work orders")
[void]$markdown.AppendLine("- Promotion-ready without more review: $($summary.promotion_ready_count) work orders")
[void]$markdown.AppendLine("- Generated-map fit evidence: $($summary.generated_map_fit_reviewed_work_order_count) work orders, $($summary.generated_map_fit_review_diagnostic_count) diagnostics, $($summary.generated_map_fit_review_unique_candidate_asset_count) unique candidate previews, $($summary.generated_map_fit_review_sampled_role_coverage_slot_count) sampled role/span slots, $($summary.generated_map_fit_review_missing_sampled_role_coverage_slot_count) missing sampled slots")
[void]$markdown.AppendLine("- Contextual exercise samples: $($summary.contextual_coverage_sample_count) samples across $($summary.contextual_sampled_work_order_count) work orders, $($summary.contextual_sampled_candidate_preview_asset_count) unique candidate previews")
[void]$markdown.AppendLine("- Work orders without generated-map fit evidence in this baseline: $($summary.work_orders_without_generated_map_fit_evidence_count)")
[void]$markdown.AppendLine("- Work orders without contextual exercise samples in this baseline: $($summary.work_orders_without_contextual_coverage_samples_count)")
[void]$markdown.AppendLine("- Targeted generated-map review still flagged by labels: $($summary.targeted_generated_map_review_needed_count) work orders")
[void]$markdown.AppendLine("")
[void]$markdown.AppendLine("| Work order | Class | Candidates | QA warnings | Samples | Fit evidence | Gate | Evidence entries shown |")
[void]$markdown.AppendLine("| --- | --- | ---: | ---: | ---: | ---: | --- | ---: |")
foreach ($item in $workOrderPackets) {
    [void]$markdown.AppendLine(('| `{0}` | `{1}` | {2}/{3} | {4} | {5} | {6} | `{7}` | {8}/{9} |' -f
            $item.id,
            $item.asset_class,
            $item.candidate_asset_count,
            $item.target_asset_count,
            $item.qa_warning_count,
            $item.contextual_coverage_sample_count,
            $item.generated_map_fit_evidence_count,
            $item.promotion_gate,
            $item.shown_validation_evidence_count,
            $item.validation_evidence_count))
}

foreach ($item in $workOrderPackets) {
    [void]$markdown.AppendLine("")
    [void]$markdown.AppendLine("## $($item.id)")
    [void]$markdown.AppendLine("")
    [void]$markdown.AppendLine(('- State: `{0}` / `{1}`' -f $item.readiness_state, $item.promotion_gate))
    if ($null -ne $item.latest_validation -and -not [string]::IsNullOrWhiteSpace($item.latest_validation.id)) {
        $latestLabels = @(ConvertTo-TilesetArray (Get-TilesetProperty $item.latest_validation "issue_labels" @()) | ForEach-Object { [string]$_ })
        $latestLabelText = if ($latestLabels.Count -gt 0) { $latestLabels -join ", " } else { "<none>" }
        [void]$markdown.AppendLine(('- Latest validation: `{0}`, status `{1}`, rating {2}, labels: {3}' -f
                $item.latest_validation.id,
                $item.latest_validation.status,
                $item.latest_validation.rating,
                $latestLabelText))
        $latestNotes = Get-TilesetProperty $item.latest_validation "notes" ""
        if (-not [string]::IsNullOrWhiteSpace($latestNotes)) {
            [void]$markdown.AppendLine("- Latest validation notes: $latestNotes")
        }
    }
    [void]$markdown.AppendLine("- Counts: $($item.accepted_asset_count)/$($item.target_asset_count) accepted, $($item.candidate_asset_count) candidates, $($item.excluded_needs_repaint_asset_count) needs-repaint excluded")
    [void]$markdown.AppendLine("- Next action: $($item.next_action)")
    if ($item.review_holds.Count -gt 0) {
        [void]$markdown.AppendLine("- Review holds: $($item.review_holds -join ', ')")
    }
    if ($item.blockers.Count -gt 0) {
        [void]$markdown.AppendLine("- Blockers: $($item.blockers -join '; ')")
    }
    if ($item.review_attention_flags.Count -gt 0) {
        [void]$markdown.AppendLine("- Review attention: $($item.review_attention_flags -join ', ')")
    }
    foreach ($fit in $item.generated_map_fit_evidence) {
        [void]$markdown.AppendLine(('- Generated-map fit: {0} diagnostics, {1} unique previews, {2} sampled role/span slots, {3} missing sampled slots; `{4}`' -f
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

    foreach ($entry in $item.validation_evidence) {
        [void]$markdown.AppendLine("")
        [void]$markdown.AppendLine("### $($entry.id)")
        [void]$markdown.AppendLine(('- `{0}`, status `{1}`, rating {2}, candidate_pass `{3}`, acceptance_ready `{4}`, accepted `{5}`' -f
                $entry.result_type,
                $entry.status,
                $entry.rating,
                $entry.candidate_pass,
                $entry.acceptance_ready,
                $entry.accepted))
        if ($entry.issue_labels.Count -gt 0) {
            [void]$markdown.AppendLine("- Labels: $($entry.issue_labels -join ', ')")
        }
        foreach ($link in $entry.evidence_links) {
            [void]$markdown.AppendLine(('- `{0}`: `{1}`' -f $link.key, $link.path))
        }
    }
}
Write-CleanTextFile $markdownPath $markdown.ToString()

Write-Host "Contextual human review packet JSON wrote $jsonPath"
Write-Host "Contextual human review packet Markdown wrote $markdownPath"
Write-Host "Work orders: $($summary.work_order_count); human review required: $($summary.human_review_required_count); promotion-ready: $($summary.promotion_ready_count)"
