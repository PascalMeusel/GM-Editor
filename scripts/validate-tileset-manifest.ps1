param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$OutputPath = "",
    [switch]$FailOnIssues
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")

$resolvedSourceRoot = Resolve-TilesetPath $SourceRoot
if (-not (Test-Path -LiteralPath $resolvedSourceRoot)) {
    throw "Missing v2 source root: $resolvedSourceRoot"
}
if (-not $OutputPath) {
    $OutputPath = Join-Path $resolvedSourceRoot "review/manifest_validation_report.json"
}
$resolvedOutputPath = Resolve-TilesetPath $OutputPath
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $resolvedOutputPath) | Out-Null

$bundle = Get-TilesetV2ManifestBundle $resolvedSourceRoot
$records = @(Get-TilesetV2AssetRecords $bundle)
$errors = @()
$warnings = @()

function Test-WorkOrderContextField($Context, [string]$Field) {
    if ($null -eq $Context) {
        return $false
    }
    if ($null -eq $Context.PSObject.Properties[$Field]) {
        return $false
    }
    $value = $Context.$Field
    if ($null -eq $value) {
        return $false
    }
    if ($value -is [string]) {
        return -not [string]::IsNullOrWhiteSpace($value)
    }
    if ($value -is [array]) {
        return $value.Count -gt 0
    }
    return $true
}

$coreQaLabels = @(
    "road_overlay_shortcut",
    "bridge_middle_repetition",
    "tower_cave_grass_halo",
    "range_not_multitile",
    "component_biome_drift",
    "single_road_family_collapse",
    "style_family_fallback"
)

$workOrderRecords = @(Get-TilesetV2ProductionWorkOrderRecords $bundle)
$workOrderIds = @{}
$workOrderOwnerCounts = @{}
foreach ($record in $workOrderRecords) {
    $workOrderId = Get-TilesetProperty $record.WorkOrder "id" ""
    if (-not [string]::IsNullOrWhiteSpace($workOrderId)) {
        $workOrderIds[$workOrderId] = $true
    }

    $workOrderOwner = Get-TilesetProperty $record.WorkOrder "owner" ""
    if ([string]::IsNullOrWhiteSpace($workOrderOwner)) {
        continue
    }
    if (-not $workOrderOwnerCounts.ContainsKey($workOrderOwner)) {
        $workOrderOwnerCounts[$workOrderOwner] = 0
    }
    $workOrderOwnerCounts[$workOrderOwner] += 1
}

foreach ($record in $workOrderRecords) {
    $workOrder = $record.WorkOrder
    $id = Get-TilesetProperty $workOrder "id" ""
    $assetClass = Get-TilesetProperty $workOrder "asset_class" ""
    $required = @("id", "asset_class", "owner", "scale", "status", "owning_workstream", "required_context", "required_masks_or_roles", "target_variants", "qa_labels")

    foreach ($field in $required) {
        if ($null -eq $workOrder.PSObject.Properties[$field]) {
            $errors += [pscustomobject]@{
                id = $id
                manifest = $record.Manifest
                issue = "missing_work_order_field"
                field = $field
                message = "Production work order is missing required field '$field'."
            }
        }
    }

    $context = Get-TilesetProperty $workOrder "required_context" $null
    $contextFields = switch ($assetClass) {
        "integrated_way_ground" { @("way_family", "ground") }
        "bridge_sequence" { @("bridge_family", "crossing", "approach_way", "span_positions") }
        "component_role_set" { @("component_family", "roles", "modifier") }
        "structure_context_matrix" { @("structure_family", "grounds", "entrance_types") }
        default { @() }
    }

    if ($contextFields.Count -eq 0) {
        $errors += [pscustomobject]@{
            id = $id
            manifest = $record.Manifest
            issue = "unknown_work_order_asset_class"
            asset_class = $assetClass
            message = "Unknown production work order asset_class '$assetClass'."
        }
    }

    foreach ($field in $contextFields) {
        if (-not (Test-WorkOrderContextField $context $field)) {
            $errors += [pscustomobject]@{
                id = $id
                manifest = $record.Manifest
                issue = "missing_work_order_context_field"
                field = $field
                message = "Production work order context is missing '$field'."
            }
        }
    }

    $targetVariants = [int](Get-TilesetProperty $workOrder "target_variants" 0)
    if ($targetVariants -le 0) {
        $errors += [pscustomobject]@{
            id = $id
            manifest = $record.Manifest
            issue = "invalid_work_order_target_variants"
            message = "Production work order target_variants must be greater than zero."
        }
    }

    $labels = @(ConvertTo-TilesetArray (Get-TilesetProperty $workOrder "qa_labels" @()))
    if ($labels.Count -eq 0) {
        $errors += [pscustomobject]@{
            id = $id
            manifest = $record.Manifest
            issue = "missing_work_order_qa_labels"
            message = "Production work order must name at least one QA label."
        }
    }
    foreach ($label in $labels) {
        if ($label -notin $coreQaLabels) {
            $warnings += [pscustomobject]@{
                id = $id
                manifest = $record.Manifest
                issue = "non_core_work_order_qa_label"
                qa_label = $label
                message = "Production work order references a QA label outside the current core art-readiness set."
            }
        }
    }
}

if ($records.Count -eq 0) {
    $warnings += [pscustomobject]@{
        id = ""
        issue = "no_assets"
        message = "No v2 manifest assets have been registered yet."
    }
}

$records | Where-Object { -not [string]::IsNullOrWhiteSpace($_.Id) } | Group-Object Id | Where-Object { $_.Count -gt 1 } | ForEach-Object {
    $errors += [pscustomobject]@{
        id = $_.Name
        issue = "duplicate_id"
        message = "Duplicate asset ID appears $($_.Count) times."
    }
}

foreach ($record in $records) {
    $asset = $record.Asset
    $id = Get-TilesetProperty $asset "id" ""
    $assetClass = Get-TilesetProperty $asset "asset_class" ""
    $owner = Get-TilesetProperty $asset "owner" ""
    $workOrderId = Get-TilesetProperty $asset "work_order_id" ""
    $sourcePath = Get-TilesetProperty $asset "source_path" ""
    $required = @("id", "style", "scale", "asset_class", "owner", "variant", "source_path", "status")

    if ($assetClass -in @("same_auto", "transition_edge", "way_segment", "way_transition")) {
        $required += "mask"
    }
    if ($assetClass -in @("transition_edge", "way_transition")) {
        $required += "neighbor"
    }
    if ($assetClass -eq "transition_edge") {
        $required += "transparent"
        $required += "validation"
    }

    foreach ($field in $required) {
        if ($null -eq $asset.PSObject.Properties[$field]) {
            $errors += [pscustomobject]@{
                id = $id
                issue = "missing_field"
                field = $field
                message = "Missing required field '$field'."
            }
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($workOrderId) -and -not $workOrderIds.ContainsKey($workOrderId)) {
        $errors += [pscustomobject]@{
            id = $id
            manifest = $record.Manifest
            issue = "unknown_asset_work_order_id"
            work_order_id = $workOrderId
            message = "Asset references a production work_order_id that does not exist."
        }
    }

    if ([string]::IsNullOrWhiteSpace($workOrderId) -and
        $workOrderOwnerCounts.ContainsKey($owner) -and
        [int]$workOrderOwnerCounts[$owner] -gt 1) {
        $warnings += [pscustomobject]@{
            id = $id
            manifest = $record.Manifest
            issue = "ambiguous_asset_work_order_owner"
            owner = $owner
            message = "Asset owner matches multiple production work orders; set work_order_id to keep coverage reports unambiguous."
        }
    }

    if ([string]::IsNullOrWhiteSpace($sourcePath)) {
        continue
    }

    $fullPath = Resolve-TilesetPath $sourcePath
    if (-not (Test-Path -LiteralPath $fullPath)) {
        $errors += [pscustomobject]@{
            id = $id
            issue = "missing_source_file"
            source_path = ConvertTo-TilesetRelativePath $sourcePath
            message = "The manifest source_path does not exist."
        }
        continue
    }

    $parse = ConvertFrom-TilesetAssetFileName $fullPath
    if (-not $parse.IsValid) {
        $errors += [pscustomobject]@{
            id = $id
            issue = "bad_filename"
            source_path = ConvertTo-TilesetRelativePath $fullPath
            message = ($parse.Errors -join " ")
        }
    }
    if ($id -and $parse.Id -and $id -ne $parse.Id) {
        $errors += [pscustomobject]@{
            id = $id
            issue = "id_filename_mismatch"
            expected_id = $parse.Id
            source_path = ConvertTo-TilesetRelativePath $fullPath
            message = "Manifest id does not match filename-derived id."
        }
    }

    $info = Get-TilesetImageInfo $fullPath
    $dimensions = Get-TilesetProperty $asset "dimensions" $null
    if ($null -ne $dimensions) {
        $expectedWidth = [int](Get-TilesetProperty $dimensions "width" 0)
        $expectedHeight = [int](Get-TilesetProperty $dimensions "height" 0)
        if ($expectedWidth -ne $info.Width -or $expectedHeight -ne $info.Height) {
            $errors += [pscustomobject]@{
                id = $id
                issue = "dimension_mismatch"
                expected = "$expectedWidth x $expectedHeight"
                actual = "$($info.Width) x $($info.Height)"
                message = "Manifest dimensions do not match the PNG."
            }
        }
    }

    $transparent = Get-TilesetProperty $asset "transparent" $null
    if ($transparent -eq $true -and -not (Test-TilesetImageHasTransparency $fullPath 4)) {
        $warnings += [pscustomobject]@{
            id = $id
            issue = "transparent_without_alpha_pixels"
            source_path = ConvertTo-TilesetRelativePath $fullPath
            message = "Asset is marked transparent but sampled pixels are fully opaque."
        }
    }
}

function Test-PackedManifestAssetStatuses(
    [string]$ManifestPath,
    [string]$ExpectedRole,
    [string[]]$AllowedStatuses,
    [string]$Issue
) {
    if (-not (Test-Path -LiteralPath $ManifestPath)) {
        return
    }

    $manifest = Get-Content -Raw -LiteralPath $ManifestPath | ConvertFrom-Json
    $role = Get-TilesetProperty $manifest "manifest_role" ""
    if (-not [string]::IsNullOrWhiteSpace($ExpectedRole) -and $role -ne $ExpectedRole) {
        $script:errors += [pscustomobject]@{
            manifest = ConvertTo-TilesetRelativePath $ManifestPath
            issue = "${Issue}_role_mismatch"
            expected = $ExpectedRole
            actual = $role
            message = "Packed manifest role does not match the expected runtime/candidate split."
        }
    }

    foreach ($asset in @(Get-TilesetProperty $manifest "assets" @())) {
        $status = Get-TilesetProperty $asset "status" ""
        if ($status -notin $AllowedStatuses) {
            $script:errors += [pscustomobject]@{
                id = Get-TilesetProperty $asset "id" ""
                manifest = ConvertTo-TilesetRelativePath $ManifestPath
                issue = $Issue
                status = $status
                allowed_statuses = $AllowedStatuses
                message = "Packed manifest contains an asset status outside its allowed role."
            }
        }
    }
}

$generatedRoot = Resolve-TilesetPath "Resources/Maps/Tilesets/Generated/FantasyNeutral/v2"
if (Test-Path -LiteralPath $generatedRoot) {
    Test-PackedManifestAssetStatuses `
        -ManifestPath (Join-Path $generatedRoot "packed_runtime_manifest.json") `
        -ExpectedRole "accepted_runtime" `
        -AllowedStatuses @("accepted") `
        -Issue "runtime_manifest_non_accepted_asset"
    Test-PackedManifestAssetStatuses `
        -ManifestPath (Join-Path $generatedRoot "packed_candidate_manifest.json") `
        -ExpectedRole "candidate_preview" `
        -AllowedStatuses @("candidate") `
        -Issue "candidate_manifest_non_candidate_asset"
}

$report = [pscustomobject]@{
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    source_root = ConvertTo-TilesetRelativePath $resolvedSourceRoot
    summary = [pscustomobject]@{
        asset_count = $records.Count
        work_order_count = $workOrderRecords.Count
        error_count = $errors.Count
        warning_count = $warnings.Count
    }
    errors = $errors
    warnings = $warnings
}

$report | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $resolvedOutputPath
Write-Host "Tileset manifest validation wrote $resolvedOutputPath"
Write-Host "Errors: $($errors.Count); warnings: $($warnings.Count)"

if ($FailOnIssues -and $errors.Count -gt 0) {
    throw "Tileset manifest validation found $($errors.Count) error(s)."
}
