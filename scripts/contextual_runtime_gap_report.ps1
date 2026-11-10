param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$QaReportPath = "build/map-generation-review-structure-way-ground-resolution/map_generation_batch_report.json",
    [string]$OutputJsonPath = "build/contextual_runtime_gap_report_structure_way_ground_resolution.json",
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

function Get-QaDiagnostics($QaReport) {
    $items = @()
    foreach ($result in (ConvertTo-TilesetArray (Get-TilesetProperty $QaReport "results" @()))) {
        foreach ($diagnostic in (ConvertTo-TilesetArray (Get-TilesetProperty $result "diagnostics" @()))) {
            $semanticContext = Get-TilesetProperty $diagnostic "semantic_context" ""
            $label = Get-TilesetProperty $diagnostic "label" ""
            if ([string]::IsNullOrWhiteSpace($label)) {
                continue
            }
            $items += [pscustomobject]@{
                case = Get-TilesetProperty $result "case" ""
                seed = Get-TilesetProperty $result "seed" 0
                severity = Get-TilesetProperty $diagnostic "severity" ""
                label = $label
                message = Get-TilesetProperty $diagnostic "message" ""
                selected_asset_id = Get-TilesetProperty $diagnostic "selected_asset_id" ""
                fallback_reason = Get-TilesetProperty $diagnostic "fallback_reason" ""
                semantic_context = $semanticContext
                work_order_id = Get-ContextValue $semanticContext "work_order"
                request_owner = Get-ContextValue $semanticContext "request_owner"
                candidate_preview_asset = Get-ContextValue $semanticContext "candidate_preview_asset"
                way_family = Get-ContextValue $semanticContext "way_family"
                bridge_family = Get-ContextValue $semanticContext "bridge_family"
                structure_family = Get-ContextValue $semanticContext "structure_family"
                component_family = Get-ContextValue $semanticContext "component_family"
                component_role = Get-ContextValue $semanticContext "component_role"
                sample_component_role_count = Get-ContextValue $semanticContext "sample_component_role_count"
                sample_component_roles = Get-ContextValue $semanticContext "sample_component_roles"
                ground = Get-ContextValue $semanticContext "ground"
                entrance = Get-ContextValue $semanticContext "entrance"
                orientation = Get-ContextValue $semanticContext "orientation"
                crossing = Get-ContextValue $semanticContext "crossing"
                approach_way = Get-ContextValue $semanticContext "approach_way"
                mask = Get-ContextValue $semanticContext "mask"
                span_position = Get-ContextValue $semanticContext "span_position"
                sample_span_position_count = Get-ContextValue $semanticContext "sample_span_position_count"
                sample_span_positions = Get-ContextValue $semanticContext "sample_span_positions"
                coordinates = @(ConvertTo-TilesetArray (Get-TilesetProperty $diagnostic "coordinates" @()))
            }
        }
    }
    return $items
}

function Get-NextAction($Kind, [string]$Reason) {
    if ($Kind -eq "candidate_not_runtime_accepted") {
        return "Human/art-direction review, then promote accepted assets or request repaint."
    }
    if ($Kind -eq "generator_exercise_gap") {
        return "Broaden generated scenario coverage; do not generate more asset slots only for this warning."
    }

    switch ($Reason) {
        "missing_bridge_crossing_context" { return "Fix generator/bridge semantics so bridge cells know the crossed water/gap context before judging art." }
        "missing_bridge_exit_approach_asset" { return "Review bridge approach/exit roles and add or remap bridge approach assets if the generated context is valid." }
        "missing_bridge_approach_asset" { return "Review bridge approach roles and add or remap approach assets if the generated context is valid." }
        "missing_bridge_bank_context" { return "Add bank/threshold context review before accepting bridge spans." }
        "missing_structure_entrance_context_asset" { return "Triage generator entrance semantics versus missing guard-tower entrance variants." }
        "missing_contextual_asset_slot" { return "Add or remap the exact requested contextual asset slot, or constrain generation if that context is out of scope." }
        "wrong_structure_ground_context" { return "Triage structure ground selection; add or remap ground-specific structure variants only after semantics are correct." }
        "missing_waterfront_structure_ground_asset" { return "Decide whether waterfront towers are valid scope, then add waterfront structure context or constrain generation." }
        "missing_bridgehead_structure_entrance_asset" { return "Decide whether bridgehead tower entrances are valid scope, then add bridgehead variants or constrain generation." }
        "repeated_bridge_component_context" { return "Review generated bridge component sequencing and candidate role fit before accepting bridge spans." }
    }
    return "Inspect samples and assign either generator semantics, asset remap, repaint, or promotion review."
}

function Get-WorkOrderMap($Bundle) {
    $items = @{}
    foreach ($record in (Get-TilesetV2ProductionWorkOrderRecords $Bundle)) {
        if (-not [string]::IsNullOrWhiteSpace($record.Id)) {
            $items[$record.Id] = $record.WorkOrder
        }
    }
    return $items
}

function Get-AllowedContextValues($WorkOrder, [string]$Field) {
    $context = Get-TilesetProperty $WorkOrder "required_context" $null
    if ($null -eq $context) {
        return @()
    }

    $fieldNames = switch ($Field) {
        "way_family" { @("way_family") }
        "bridge_family" { @("bridge_family") }
        "structure_family" { @("structure_family") }
        "component_family" { @("component_family") }
        "ground" { @("ground", "grounds") }
        "entrance" { @("entrance", "entrance_type", "entrance_types") }
        "orientation" { @("orientation", "orientations") }
        "crossing" { @("crossing", "crossings") }
        "approach_way" { @("approach_way", "approach_ways") }
        default { @($Field) }
    }

    $values = @()
    foreach ($fieldName in $fieldNames) {
        $values += @(ConvertTo-TilesetArray (Get-TilesetProperty $context $fieldName @()) | ForEach-Object { [string]$_ })
    }
    if ($Field -eq "orientation") {
        $values += "none"
    }

    return @($values | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Sort-Object -Unique)
}

function Get-UnsupportedContextValues($Diagnostic, $WorkOrdersById) {
    if ([string]::IsNullOrWhiteSpace($Diagnostic.work_order_id) -or -not $WorkOrdersById.ContainsKey($Diagnostic.work_order_id)) {
        return @()
    }

    $workOrder = $WorkOrdersById[$Diagnostic.work_order_id]
    $items = @()
    foreach ($field in @("way_family", "bridge_family", "structure_family", "component_family", "ground", "entrance", "orientation", "crossing", "approach_way")) {
        $value = Get-TilesetProperty $Diagnostic $field ""
        if ([string]::IsNullOrWhiteSpace($value)) {
            continue
        }
        $allowedValues = @(Get-AllowedContextValues $workOrder $field)
        if ($allowedValues.Count -gt 0 -and -not (Test-ContextValueAllowed $field $value $allowedValues)) {
            $items += [pscustomobject]@{
                field = $field
                value = $value
                allowed_values = $allowedValues
            }
        }
    }
    return $items
}

function ConvertTo-ContextCompositeKey($Values) {
    return (@($Values | ForEach-Object {
                $value = [string]$_
                if ($value.StartsWith("way_", [System.StringComparison]::Ordinal)) {
                    $value.Substring(4)
                }
                else {
                    $value
                }
            }) -join "_or_")
}

function Test-ContextValueAllowed([string]$Field, [string]$Value, $AllowedValues) {
    if ($Value -in $AllowedValues) {
        return $true
    }

    if ($Field -eq "component_family") {
        foreach ($allowed in $AllowedValues) {
            if ($Value.StartsWith(($allowed + "_"), [System.StringComparison]::Ordinal)) {
                return $true
            }
        }
    }

    if ($Field -eq "crossing") {
        $composite = ConvertTo-ContextCompositeKey $AllowedValues
        if (-not [string]::IsNullOrWhiteSpace($composite) -and $Value -eq $composite) {
            return $true
        }
    }

    return $false
}

function Get-UnsupportedContextSummary($Diagnostics) {
    $items = @()
    foreach ($diagnostic in $Diagnostics) {
        foreach ($unsupported in (ConvertTo-TilesetArray (Get-TilesetProperty $diagnostic "unsupported_context_values" @()))) {
            $items += [pscustomobject]@{
                field = Get-TilesetProperty $unsupported "field" ""
                value = Get-TilesetProperty $unsupported "value" ""
                allowed_values = @(ConvertTo-TilesetArray (Get-TilesetProperty $unsupported "allowed_values" @()))
            }
        }
    }

    $summary = @()
    foreach ($group in ($items | Group-Object field, value | Sort-Object Count -Descending)) {
        $first = $group.Group[0]
        $summary += [pscustomobject]@{
            field = $first.field
            value = $first.value
            count = $group.Count
            allowed_values = $first.allowed_values
        }
    }
    return $summary
}

function Get-ContextSlotKey($Diagnostic) {
    $parts = @()
    foreach ($field in @("ground", "entrance", "orientation", "crossing", "approach_way", "mask", "span_position", "component_family", "component_role")) {
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

function Get-ContextSlotSummary($Diagnostics) {
    $summary = @()
    foreach ($group in ($Diagnostics | Group-Object { Get-ContextSlotKey $_ } | Sort-Object -Property @{ Expression = "Count"; Descending = $true }, Name)) {
        $first = $group.Group[0]
        $summary += [pscustomobject]@{
            slot_key = $group.Name
            count = $group.Count
            candidate_preview_diagnostic_count = @($group.Group | Where-Object { -not [string]::IsNullOrWhiteSpace($_.candidate_preview_asset) }).Count
            ground = $first.ground
            entrance = $first.entrance
            orientation = $first.orientation
            crossing = $first.crossing
            approach_way = $first.approach_way
            mask = $first.mask
            span_position = $first.span_position
            component_family = $first.component_family
            component_role = $first.component_role
            sample_cases = @($group.Group | ForEach-Object { $_.case } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -First 3)
            sample_seeds = @($group.Group | ForEach-Object { $_.seed } | Select-Object -First 3)
        }
    }
    return $summary
}

function Get-AssetWorkOrderIds($Asset) {
    $items = @()
    $single = Get-TilesetProperty $Asset "work_order_id" ""
    if (-not [string]::IsNullOrWhiteSpace($single)) {
        $items += $single
    }
    $items += @(ConvertTo-TilesetArray (Get-TilesetProperty $Asset "work_order_ids" @()) | ForEach-Object { [string]$_ })
    return @($items | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Sort-Object -Unique)
}

function Get-AssetContextField($Asset, [string]$Field) {
    $context = Get-TilesetProperty $Asset "context" $null
    switch ($Field) {
        "ground" { return Get-TilesetProperty $context "ground" "" }
        "entrance" {
            $value = Get-TilesetProperty $context "entrance_type" ""
            if ([string]::IsNullOrWhiteSpace($value)) {
                $value = Get-TilesetProperty $context "entrance" ""
            }
            return $value
        }
        "orientation" { return Get-TilesetProperty $context "orientation" "" }
        "crossing" { return Get-TilesetProperty $context "crossing" "" }
        "approach_way" { return Get-TilesetProperty $context "approach_way" "" }
        "component_family" { return Get-TilesetProperty $context "component_family" "" }
        "mask" {
            $mask = Get-TilesetProperty $Asset "mask" $null
            if ($null -ne $mask) {
                return [string]$mask
            }
            return Get-TilesetProperty $context "mask" ""
        }
        "span_position" {
            $value = Get-TilesetProperty $Asset "span_position" ""
            if ([string]::IsNullOrWhiteSpace($value)) {
                $value = Get-TilesetProperty $context "span_position" ""
            }
            return $value
        }
        "role" {
            $value = Get-TilesetProperty $Asset "role" ""
            if ([string]::IsNullOrWhiteSpace($value)) {
                $value = Get-TilesetProperty $context "role" ""
            }
            return $value
        }
    }
    return ""
}

function Get-CandidateSlotKey($Record) {
    $parts = @()
    foreach ($field in @("ground", "entrance", "orientation", "crossing", "approach_way", "mask", "span_position", "role", "component_family")) {
        $value = Get-TilesetProperty $Record $field ""
        if (-not [string]::IsNullOrWhiteSpace($value)) {
            $parts += "$field=$value"
        }
    }
    if ($parts.Count -eq 0) {
        return "<unspecified>"
    }
    return ($parts -join "; ")
}

function Get-CandidateSlotRecordsByWorkOrder($Bundle) {
    $byWorkOrder = @{}
    foreach ($record in (Get-TilesetV2AssetRecords $Bundle)) {
        $asset = $record.Asset
        $status = Get-TilesetProperty $asset "status" ""
        if ($status -notin @("candidate", "accepted")) {
            continue
        }

        $workOrderIds = @(Get-AssetWorkOrderIds $asset)
        if ($workOrderIds.Count -eq 0) {
            continue
        }

        $slotRecord = [pscustomobject]@{
            asset_id = Get-TilesetProperty $asset "id" ""
            status = $status
            ground = Get-AssetContextField $asset "ground"
            entrance = Get-AssetContextField $asset "entrance"
            orientation = Get-AssetContextField $asset "orientation"
            crossing = Get-AssetContextField $asset "crossing"
            approach_way = Get-AssetContextField $asset "approach_way"
            mask = Get-AssetContextField $asset "mask"
            span_position = Get-AssetContextField $asset "span_position"
            role = Get-AssetContextField $asset "role"
            component_family = Get-AssetContextField $asset "component_family"
        }
        $slotRecord | Add-Member -MemberType NoteProperty -Name slot_key -Value (Get-CandidateSlotKey $slotRecord)

        foreach ($workOrderId in $workOrderIds) {
            if (-not $byWorkOrder.ContainsKey($workOrderId)) {
                $byWorkOrder[$workOrderId] = @()
            }
            $byWorkOrder[$workOrderId] += $slotRecord
        }
    }
    return $byWorkOrder
}

function Get-CandidateSlotScore($Slot, $Candidate) {
    $score = 0
    foreach ($field in @("ground", "entrance", "orientation", "crossing", "approach_way", "mask", "span_position", "component_family", "component_role")) {
        $wanted = Get-TilesetProperty $Slot $field ""
        $available = Get-TilesetProperty $Candidate $field ""
        if ($field -eq "component_role" -and [string]::IsNullOrWhiteSpace($available)) {
            $available = Get-TilesetProperty $Candidate "role" ""
        }
        if ([string]::IsNullOrWhiteSpace($wanted) -or [string]::IsNullOrWhiteSpace($available)) {
            continue
        }
        if ($wanted -eq $available) {
            switch ($field) {
                "ground" { $score += 4 }
                "entrance" { $score += 4 }
                "span_position" { $score += 4 }
                "component_role" { $score += 4 }
                "orientation" { $score += 3 }
                default { $score += 2 }
            }
        }
    }
    return $score
}

function Get-CandidateSlotComparison($Slot, $Candidate) {
    $matched = @()
    $different = @()
    $missing = @()
    foreach ($field in @("ground", "entrance", "orientation", "crossing", "approach_way", "mask", "span_position", "component_family", "component_role")) {
        $wanted = Get-TilesetProperty $Slot $field ""
        if ([string]::IsNullOrWhiteSpace($wanted)) {
            continue
        }

        $available = Get-TilesetProperty $Candidate $field ""
        if ($field -eq "component_role" -and [string]::IsNullOrWhiteSpace($available)) {
            $available = Get-TilesetProperty $Candidate "role" ""
        }

        if ([string]::IsNullOrWhiteSpace($available)) {
            $missing += $field
        }
        elseif ($wanted -eq $available) {
            $matched += $field
        }
        else {
            $different += "$field=$available"
        }
    }

    return [pscustomobject]@{
        matched_fields = $matched
        different_fields = $different
        missing_candidate_fields = $missing
    }
}

function Get-CandidateSlotHints($Slot, [string]$WorkOrderId, $CandidateSlotsByWorkOrder) {
    if ([string]::IsNullOrWhiteSpace($WorkOrderId) -or $WorkOrderId -eq "<unlinked>" -or -not $CandidateSlotsByWorkOrder.ContainsKey($WorkOrderId)) {
        return @()
    }

    $records = @()
    foreach ($candidate in $CandidateSlotsByWorkOrder[$WorkOrderId]) {
        $comparison = Get-CandidateSlotComparison $Slot $candidate
        $records += [pscustomobject]@{
            slot_key = $candidate.slot_key
            score = Get-CandidateSlotScore $Slot $candidate
            status = $candidate.status
            matched_fields = $comparison.matched_fields
            different_fields = $comparison.different_fields
            missing_candidate_fields = $comparison.missing_candidate_fields
        }
    }

    $hints = @()
    foreach ($group in ($records | Group-Object slot_key | Sort-Object -Property @{ Expression = { ($_.Group | Measure-Object score -Maximum).Maximum }; Descending = $true }, @{ Expression = "Count"; Descending = $true }, Name | Select-Object -First 5)) {
        $maxScore = ($group.Group | Measure-Object score -Maximum).Maximum
        $hints += [pscustomobject]@{
            slot_key = $group.Name
            candidate_or_accepted_count = $group.Count
            best_match_score = [int]$maxScore
            statuses = @($group.Group | ForEach-Object { $_.status } | Sort-Object -Unique)
            matched_fields = @(($group.Group | Sort-Object -Property @{ Expression = "score"; Descending = $true } | Select-Object -First 1).matched_fields)
            different_fields = @(($group.Group | Sort-Object -Property @{ Expression = "score"; Descending = $true } | Select-Object -First 1).different_fields)
            missing_candidate_fields = @(($group.Group | Sort-Object -Property @{ Expression = "score"; Descending = $true } | Select-Object -First 1).missing_candidate_fields)
        }
    }
    return $hints
}

function New-CountItem([string]$Name, [int]$Count) {
    return [pscustomobject]@{
        name = if ([string]::IsNullOrWhiteSpace($Name)) { "<empty>" } else { $Name }
        count = $Count
    }
}

function Format-SampleRoleSummary($Group) {
    $parts = @()
    $spanPositions = @(ConvertTo-TilesetArray (Get-TilesetProperty $Group "sample_span_positions" @()) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    if ($spanPositions.Count -gt 0) {
        $parts += "bridge: " + (($spanPositions | ForEach-Object { "``$_``" }) -join ", ")
    }
    $componentRoles = @(ConvertTo-TilesetArray (Get-TilesetProperty $Group "sample_component_roles" @()) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    if ($componentRoles.Count -gt 0) {
        $parts += "component: " + (($componentRoles | ForEach-Object { "``$_``" }) -join ", ")
    }
    if ($parts.Count -eq 0) {
        return "<none>"
    }
    return $parts -join "<br>"
}

$sourceRoot = Resolve-TilesetPath $SourceRoot
$qaPath = Resolve-TilesetPath $QaReportPath
$jsonPath = Resolve-TilesetPath $OutputJsonPath
if ([string]::IsNullOrWhiteSpace($OutputMarkdownPath)) {
    $OutputMarkdownPath = [System.IO.Path]::ChangeExtension($OutputJsonPath, ".md")
}
$markdownPath = Resolve-TilesetPath $OutputMarkdownPath

if (-not (Test-Path -LiteralPath $qaPath)) {
    throw "Missing QA report: $qaPath"
}
if (-not (Test-Path -LiteralPath $sourceRoot)) {
    throw "Missing v2 source root: $sourceRoot"
}

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $jsonPath) | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $markdownPath) | Out-Null

$bundle = Get-TilesetV2ManifestBundle $sourceRoot
$workOrdersById = Get-WorkOrderMap $bundle
$candidateSlotsByWorkOrder = Get-CandidateSlotRecordsByWorkOrder $bundle
$qaReport = Get-Content -Raw -LiteralPath $qaPath | ConvertFrom-Json
$diagnostics = @(Get-QaDiagnostics $qaReport)
foreach ($diagnostic in $diagnostics) {
    $diagnostic | Add-Member -MemberType NoteProperty -Name diagnostic_kind -Value (Get-TilesetContextualDiagnosticKind $diagnostic)
    $diagnostic | Add-Member -MemberType NoteProperty -Name unsupported_context_values -Value @(Get-UnsupportedContextValues $diagnostic $workOrdersById)
}

$groups = @()
foreach ($group in ($diagnostics | Group-Object label, fallback_reason, work_order_id | Sort-Object -Property @{ Expression = "Count"; Descending = $true }, Name)) {
    $first = $group.Group[0]
    $groups += [pscustomobject]@{
        label = $first.label
        fallback_reason = $first.fallback_reason
        diagnostic_kind = $first.diagnostic_kind
        work_order_id = if ([string]::IsNullOrWhiteSpace($first.work_order_id)) { "<unlinked>" } else { $first.work_order_id }
        count = $group.Count
        request_owners = @($group.Group | ForEach-Object { $_.request_owner } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Sort-Object -Unique)
        candidate_preview_diagnostic_count = @($group.Group | Where-Object { -not [string]::IsNullOrWhiteSpace($_.candidate_preview_asset) }).Count
        grounds = @($group.Group | ForEach-Object { $_.ground } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Sort-Object -Unique)
        entrances = @($group.Group | ForEach-Object { $_.entrance } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Sort-Object -Unique)
        crossings = @($group.Group | ForEach-Object { $_.crossing } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Sort-Object -Unique)
        approach_ways = @($group.Group | ForEach-Object { $_.approach_way } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Sort-Object -Unique)
        masks = @($group.Group | ForEach-Object { $_.mask } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Sort-Object -Unique)
        sample_span_positions = @($group.Group | ForEach-Object { $_.sample_span_positions -split "," } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) -and $_ -ne "none" } | Sort-Object -Unique)
        sample_component_roles = @($group.Group | ForEach-Object { $_.sample_component_roles -split "," } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) -and $_ -ne "none" } | Sort-Object -Unique)
        context_slots = @(Get-ContextSlotSummary $group.Group)
        unsupported_context_diagnostic_count = @($group.Group | Where-Object { @(Get-TilesetProperty $_ "unsupported_context_values" @()).Count -gt 0 }).Count
        unsupported_context_values = @(Get-UnsupportedContextSummary $group.Group)
        next_action = Get-NextAction $first.diagnostic_kind $first.fallback_reason
        samples = @($group.Group | Select-Object -First 5)
    }
    foreach ($slot in (ConvertTo-TilesetArray (Get-TilesetProperty $groups[-1] "context_slots" @()))) {
        $slot | Add-Member -MemberType NoteProperty -Name candidate_slot_hints -Value @(Get-CandidateSlotHints $slot $groups[-1].work_order_id $candidateSlotsByWorkOrder)
    }
}

$byKind = @()
foreach ($group in ($diagnostics | Group-Object diagnostic_kind | Sort-Object Count -Descending)) {
    $byKind += New-CountItem $group.Name $group.Count
}
$byLabel = @()
foreach ($group in ($diagnostics | Group-Object label | Sort-Object Count -Descending)) {
    $byLabel += New-CountItem $group.Name $group.Count
}
$byReason = @()
foreach ($group in ($diagnostics | Group-Object fallback_reason | Sort-Object Count -Descending)) {
    $byReason += New-CountItem $group.Name $group.Count
}

$report = [pscustomobject]@{
    schema_version = 1
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    source_root = ConvertTo-TilesetRelativePath $sourceRoot
    qa_report = ConvertTo-TilesetRelativePath $qaPath
    summary = [pscustomobject]@{
        diagnostic_count = $diagnostics.Count
        candidate_not_runtime_accepted_count = @($diagnostics | Where-Object { $_.diagnostic_kind -eq "candidate_not_runtime_accepted" }).Count
        context_or_art_gap_count = @($diagnostics | Where-Object { $_.diagnostic_kind -eq "context_or_art_gap" }).Count
        generator_exercise_gap_count = @($diagnostics | Where-Object { $_.diagnostic_kind -eq "generator_exercise_gap" }).Count
        unsupported_context_diagnostic_count = @($diagnostics | Where-Object { @(Get-TilesetProperty $_ "unsupported_context_values" @()).Count -gt 0 }).Count
        unlinked_diagnostic_count = @($diagnostics | Where-Object { [string]::IsNullOrWhiteSpace($_.work_order_id) }).Count
    }
    by_kind = $byKind
    by_label = $byLabel
    by_fallback_reason = $byReason
    groups = $groups
}

$report | ConvertTo-Json -Depth 24 | Set-Content -LiteralPath $jsonPath

$markdown = [System.Text.StringBuilder]::new()
[void]$markdown.AppendLine("# Contextual Runtime Gap Report")
[void]$markdown.AppendLine("")
[void]$markdown.AppendLine("- Generated: $($report.generated_at_utc)")
[void]$markdown.AppendLine(("- QA report: ``{0}``" -f $report.qa_report))
[void]$markdown.AppendLine("- Diagnostics: $($report.summary.diagnostic_count)")
[void]$markdown.AppendLine("- Candidate-not-runtime-accepted: $($report.summary.candidate_not_runtime_accepted_count)")
[void]$markdown.AppendLine("- Context or art gaps: $($report.summary.context_or_art_gap_count)")
[void]$markdown.AppendLine("- Generator exercise gaps: $($report.summary.generator_exercise_gap_count)")
[void]$markdown.AppendLine("- Unsupported work-order contexts: $($report.summary.unsupported_context_diagnostic_count)")
[void]$markdown.AppendLine("- Unlinked diagnostics: $($report.summary.unlinked_diagnostic_count)")
[void]$markdown.AppendLine("")
[void]$markdown.AppendLine("## Groups")
[void]$markdown.AppendLine("")
[void]$markdown.AppendLine("| Count | Kind | Label | Fallback reason | Work order | Candidate previews | Unsupported context | Sample roles | Next action |")
[void]$markdown.AppendLine("| ---: | --- | --- | --- | --- | ---: | ---: | --- | --- |")
foreach ($item in $groups) {
    [void]$markdown.AppendLine(('| {0} | `{1}` | `{2}` | `{3}` | `{4}` | {5} | {6} | {7} | {8} |' -f
            $item.count,
            $item.diagnostic_kind,
            $item.label,
            $(if ([string]::IsNullOrWhiteSpace($item.fallback_reason)) { "<empty>" } else { $item.fallback_reason }),
            $item.work_order_id,
            $item.candidate_preview_diagnostic_count,
            $item.unsupported_context_diagnostic_count,
            (Format-SampleRoleSummary $item),
            $item.next_action))
}

[void]$markdown.AppendLine("")
[void]$markdown.AppendLine("## Unsupported Work-Order Contexts")
[void]$markdown.AppendLine("")
[void]$markdown.AppendLine("| Count | Field | Value | Allowed values |")
[void]$markdown.AppendLine("| ---: | --- | --- | --- |")
foreach ($item in (Get-UnsupportedContextSummary $diagnostics)) {
    [void]$markdown.AppendLine(('| {0} | `{1}` | `{2}` | {3} |' -f
            $item.count,
            $item.field,
            $item.value,
            (@($item.allowed_values | ForEach-Object { "``$_``" }) -join ", ")))
}

[void]$markdown.AppendLine("")
[void]$markdown.AppendLine("## Context-Or-Art Slot Groups")
[void]$markdown.AppendLine("")
[void]$markdown.AppendLine("| Count | Label | Fallback reason | Work order | Slot | Candidate previews | Candidate slot hints | Sample seeds |")
[void]$markdown.AppendLine("| ---: | --- | --- | --- | --- | ---: | --- | --- |")
foreach ($item in ($groups | Where-Object { $_.diagnostic_kind -eq "context_or_art_gap" })) {
    foreach ($slot in (ConvertTo-TilesetArray (Get-TilesetProperty $item "context_slots" @()))) {
        $hintText = @(
            ConvertTo-TilesetArray (Get-TilesetProperty $slot "candidate_slot_hints" @()) |
                Select-Object -First 3 |
                ForEach-Object {
                    $details = @()
                    if (@($_.matched_fields).Count -gt 0) {
                        $details += "match $(@($_.matched_fields) -join '+')"
                    }
                    if (@($_.different_fields).Count -gt 0) {
                        $details += "diff $(@($_.different_fields) -join '+')"
                    }
                    if (@($_.missing_candidate_fields).Count -gt 0) {
                        $details += "missing $(@($_.missing_candidate_fields) -join '+')"
                    }
                    $detailText = if ($details.Count -gt 0) { "; $($details -join '; ')" } else { "" }
                    "``$($_.slot_key)`` ($($_.candidate_or_accepted_count)$detailText)"
                }
        ) -join "<br>"
        if ([string]::IsNullOrWhiteSpace($hintText)) {
            $hintText = "<none>"
        }
        [void]$markdown.AppendLine(('| {0} | `{1}` | `{2}` | `{3}` | `{4}` | {5} | {6} | {7} |' -f
                $slot.count,
                $item.label,
                $(if ([string]::IsNullOrWhiteSpace($item.fallback_reason)) { "<empty>" } else { $item.fallback_reason }),
                $item.work_order_id,
                $slot.slot_key,
                $slot.candidate_preview_diagnostic_count,
                $hintText,
                (@($slot.sample_seeds | ForEach-Object { "``$_``" }) -join ", ")))
    }
}

$markdown.ToString() | Set-Content -LiteralPath $markdownPath
Write-Host "Contextual runtime gap JSON wrote $jsonPath"
Write-Host "Contextual runtime gap Markdown wrote $markdownPath"
Write-Host "Diagnostics: $($report.summary.diagnostic_count); context/art gaps: $($report.summary.context_or_art_gap_count); candidate-not-runtime: $($report.summary.candidate_not_runtime_accepted_count); generator-exercise: $($report.summary.generator_exercise_gap_count); unsupported contexts: $($report.summary.unsupported_context_diagnostic_count)"
