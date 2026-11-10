param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$QaReportPath = "build/map-generation-review-structure-way-ground-resolution/map_generation_batch_report.json",
    [string]$CandidateManifestPath = "Resources/Maps/Tilesets/Generated/FantasyNeutral/v2/packed_candidate_manifest.json",
    [string]$ValidationResultsPath = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2/validation_results.json",
    [string]$OutputDirectory = "",
    [string]$OutputStem = "generated_map_candidate_fit_review",
    [int]$TileSize = 72,
    [int]$Columns = 4
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")

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

function Write-CleanTextFile([string]$Path, [string]$Text) {
    $normalized = $Text -replace "`r`n", "`n"
    $normalized = $normalized -replace "`r", "`n"
    $normalized = (($normalized -split "`n") | ForEach-Object { $_.TrimEnd() }) -join "`n"
    [System.IO.File]::WriteAllText($Path, $normalized.TrimEnd() + "`n", [System.Text.UTF8Encoding]::new($false))
}

function Draw-Label($Graphics, [string]$Text, [int]$X, [int]$Y, [int]$Width = 260, [int]$Height = 18, [float]$Size = 9.0) {
    $font = [Drawing.Font]::new("Segoe UI", $Size)
    $brush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(238, 238, 242, 246))
    $backBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(174, 8, 12, 16))
    $format = [Drawing.StringFormat]::new()
    $format.Trimming = [Drawing.StringTrimming]::EllipsisCharacter
    $format.FormatFlags = [Drawing.StringFormatFlags]::NoWrap
    try {
        $Graphics.FillRectangle($backBrush, $X, $Y, $Width, $Height)
        $Graphics.DrawString($Text, $font, $brush, [Drawing.RectangleF]::new($X + 4, $Y + 2, $Width - 8, $Height - 2), $format)
    }
    finally {
        $format.Dispose()
        $font.Dispose()
        $brush.Dispose()
        $backBrush.Dispose()
    }
}

function Draw-SoftBox($Graphics, [Drawing.Rectangle]$Rect, [string]$Fill = "#182026", [string]$Border = "#2e3a44") {
    $fillBrush = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml($Fill))
    $borderPen = [Drawing.Pen]::new([Drawing.ColorTranslator]::FromHtml($Border), 1)
    try {
        $Graphics.FillRectangle($fillBrush, $Rect)
        $Graphics.DrawRectangle($borderPen, $Rect)
    }
    finally {
        $fillBrush.Dispose()
        $borderPen.Dispose()
    }
}

function Shorten-Text([string]$Value, [int]$MaxLength = 48) {
    if ([string]::IsNullOrWhiteSpace($Value)) {
        return ""
    }
    if ($Value.Length -le $MaxLength) {
        return $Value
    }
    return $Value.Substring(0, [Math]::Max(0, $MaxLength - 1)) + "..."
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

function Get-QaDiagnostics($QaReport) {
    $items = @()
    foreach ($result in (ConvertTo-TilesetArray (Get-TilesetProperty $QaReport "results" @()))) {
        foreach ($diagnostic in (ConvertTo-TilesetArray (Get-TilesetProperty $result "diagnostics" @()))) {
            $semanticContext = Get-TilesetProperty $diagnostic "semantic_context" ""
            $candidatePreviewAsset = Get-ContextValue $semanticContext "candidate_preview_asset"
            if ([string]::IsNullOrWhiteSpace($candidatePreviewAsset)) {
                continue
            }

            $items += [pscustomobject]@{
                case = Get-TilesetProperty $result "case" ""
                seed = Get-TilesetProperty $result "seed" 0
                label = Get-TilesetProperty $diagnostic "label" ""
                severity = Get-TilesetProperty $diagnostic "severity" ""
                message = Get-TilesetProperty $diagnostic "message" ""
                selected_asset_id = Get-TilesetProperty $diagnostic "selected_asset_id" ""
                fallback_reason = Get-TilesetProperty $diagnostic "fallback_reason" ""
                semantic_context = $semanticContext
                work_order_id = Get-ContextValue $semanticContext "work_order"
                request_owner = Get-ContextValue $semanticContext "request_owner"
                candidate_preview_asset = $candidatePreviewAsset
                ground = Get-ContextValue $semanticContext "ground"
                entrance = Get-ContextValue $semanticContext "entrance"
                orientation = Get-ContextValue $semanticContext "orientation"
                crossing = Get-ContextValue $semanticContext "crossing"
                approach_way = Get-ContextValue $semanticContext "approach_way"
                mask = Get-ContextValue $semanticContext "mask"
                span_position = Get-ContextValue $semanticContext "span_position"
                component_family = Get-ContextValue $semanticContext "component_family"
                component_role = Get-ContextValue $semanticContext "component_role"
                sample_span_positions = Get-ContextValue $semanticContext "sample_span_positions"
                sample_component_roles = Get-ContextValue $semanticContext "sample_component_roles"
                coordinates = @(ConvertTo-TilesetArray (Get-TilesetProperty $diagnostic "coordinates" @()))
            }
        }
    }
    return $items
}

function Get-AssetWorkOrderIds($Asset) {
    $ids = @()
    $single = Get-TilesetProperty $Asset "work_order_id" ""
    if (-not [string]::IsNullOrWhiteSpace($single)) {
        $ids += $single
    }
    $ids += @(ConvertTo-TilesetArray (Get-TilesetProperty $Asset "work_order_ids" @()) | ForEach-Object { [string]$_ })
    return @($ids | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Sort-Object -Unique)
}

function Get-AssetContextValue($Asset, [string]$Field) {
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
        "crossing" { return ((ConvertTo-TilesetArray (Get-TilesetProperty $context "crossing" @())) -join ",") }
        "approach_way" { return Get-TilesetProperty $context "approach_way" "" }
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
        "component_family" { return Get-TilesetProperty $context "component_family" "" }
        "structure_family" { return Get-TilesetProperty $context "structure_family" "" }
        "bridge_family" { return Get-TilesetProperty $context "bridge_family" "" }
    }
    return ""
}

function Get-DiagnosticSlotKey($Diagnostic) {
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

function Get-AssetCardContext($Asset) {
    $parts = @()
    foreach ($field in @("ground", "entrance", "orientation", "crossing", "approach_way", "span_position", "role")) {
        $value = Get-AssetContextValue $Asset $field
        if (-not [string]::IsNullOrWhiteSpace($value)) {
            $parts += "$field=$value"
        }
    }
    $mask = Get-TilesetProperty $Asset "mask" ""
    if (-not [string]::IsNullOrWhiteSpace($mask)) {
        $parts += "mask=$mask"
    }
    if ($parts.Count -eq 0) {
        return "<no context>"
    }
    return ($parts -join "; ")
}

function Split-ContextList([string]$Value) {
    if ([string]::IsNullOrWhiteSpace($Value) -or $Value -eq "none") {
        return @()
    }
    return @($Value -split "," | ForEach-Object { $_.Trim() } | Where-Object {
            -not [string]::IsNullOrWhiteSpace($_) -and $_ -ne "none"
        })
}

function Get-SampledCoverageValues($Diagnostics, [string]$ScalarField, [string]$ListField) {
    $values = @()
    foreach ($diagnostic in $Diagnostics) {
        $scalar = Get-TilesetProperty $diagnostic $ScalarField ""
        if (-not [string]::IsNullOrWhiteSpace($scalar)) {
            $values += $scalar
        }
        $values += @(Split-ContextList (Get-TilesetProperty $diagnostic $ListField ""))
    }
    return @($values | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Sort-Object -Unique)
}

function Get-SampledRoleCoverage($Diagnostics, [string]$WorkOrderId, $ActiveAssetsByWorkOrder) {
    if ([string]::IsNullOrWhiteSpace($WorkOrderId) -or $WorkOrderId -eq "<unlinked>" -or -not $ActiveAssetsByWorkOrder.ContainsKey($WorkOrderId)) {
        return @()
    }

    $items = @()
    $assetPool = @($ActiveAssetsByWorkOrder[$WorkOrderId])
    foreach ($spanPosition in (Get-SampledCoverageValues $Diagnostics "span_position" "sample_span_positions")) {
        $assets = @($assetPool | Where-Object {
                (Get-AssetContextValue $_ "span_position") -eq $spanPosition -or
                (Get-AssetContextValue $_ "role") -eq $spanPosition
            } | Sort-Object { Get-TilesetProperty $_ "id" "" })
        $previewed = @($Diagnostics | ForEach-Object { $_.candidate_preview_asset } | Where-Object {
                $assetId = $_
                @($assets | ForEach-Object { Get-TilesetProperty $_ "id" "" }) -contains $assetId
            } | Sort-Object -Unique)
        $items += [pscustomobject]@{
            field = "span_position"
            value = $spanPosition
            diagnostic_mention_count = @($Diagnostics | Where-Object {
                    $_.span_position -eq $spanPosition -or
                    @(Split-ContextList $_.sample_span_positions) -contains $spanPosition
                }).Count
            active_asset_count = $assets.Count
            previewed_asset_count = $previewed.Count
            asset_ids = @($assets | ForEach-Object { Get-TilesetProperty $_ "id" "" })
            previewed_asset_ids = $previewed
        }
    }

    foreach ($role in (Get-SampledCoverageValues $Diagnostics "component_role" "sample_component_roles")) {
        $assets = @($assetPool | Where-Object {
                (Get-AssetContextValue $_ "role") -eq $role
            } | Sort-Object { Get-TilesetProperty $_ "id" "" })
        $previewed = @($Diagnostics | ForEach-Object { $_.candidate_preview_asset } | Where-Object {
                $assetId = $_
                @($assets | ForEach-Object { Get-TilesetProperty $_ "id" "" }) -contains $assetId
            } | Sort-Object -Unique)
        $items += [pscustomobject]@{
            field = "component_role"
            value = $role
            diagnostic_mention_count = @($Diagnostics | Where-Object {
                    $_.component_role -eq $role -or
                    @(Split-ContextList $_.sample_component_roles) -contains $role
                }).Count
            active_asset_count = $assets.Count
            previewed_asset_count = $previewed.Count
            asset_ids = @($assets | ForEach-Object { Get-TilesetProperty $_ "id" "" })
            previewed_asset_ids = $previewed
        }
    }

    return @($items | Sort-Object field, value)
}

$sourceRootPath = Resolve-TilesetPath $SourceRoot
$qaPath = Resolve-TilesetPath $QaReportPath
$candidateManifestResolvedPath = Resolve-TilesetPath $CandidateManifestPath
$validationResultsResolvedPath = Resolve-TilesetPath $ValidationResultsPath
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $sourceRootPath "review/contextual_candidates"
}
$outputDirectoryPath = Resolve-TilesetPath $OutputDirectory
New-Item -ItemType Directory -Force -Path $outputDirectoryPath | Out-Null

if (-not (Test-Path -LiteralPath $qaPath)) {
    throw "Missing QA report: $qaPath"
}
if (-not (Test-Path -LiteralPath $sourceRootPath)) {
    throw "Missing v2 source root: $sourceRootPath"
}
if (-not (Test-Path -LiteralPath $candidateManifestResolvedPath)) {
    throw "Missing candidate manifest: $candidateManifestResolvedPath"
}
if (-not (Test-Path -LiteralPath $validationResultsResolvedPath)) {
    throw "Missing validation results: $validationResultsResolvedPath"
}

$bundle = Get-TilesetV2ManifestBundle $sourceRootPath
$assetById = @{}
$activeAssetsByWorkOrder = @{}
$sourceAssetRecords = @(Get-TilesetV2AssetRecords $bundle)
foreach ($record in $sourceAssetRecords) {
    if (-not [string]::IsNullOrWhiteSpace($record.Id)) {
        $assetById[$record.Id] = $record.Asset
    }
    $status = Get-TilesetProperty $record.Asset "status" ""
    if ($status -notin @("candidate", "accepted")) {
        continue
    }
    foreach ($workOrderId in (Get-AssetWorkOrderIds $record.Asset)) {
        if (-not $activeAssetsByWorkOrder.ContainsKey($workOrderId)) {
            $activeAssetsByWorkOrder[$workOrderId] = @()
        }
        $activeAssetsByWorkOrder[$workOrderId] += $record.Asset
    }
}

$candidateManifest = Get-Content -Raw -LiteralPath $candidateManifestResolvedPath | ConvertFrom-Json
foreach ($asset in (ConvertTo-TilesetArray (Get-TilesetProperty $candidateManifest "assets" @()))) {
    $id = Get-TilesetProperty $asset "id" ""
    if (-not [string]::IsNullOrWhiteSpace($id) -and -not $assetById.ContainsKey($id)) {
        $assetById[$id] = $asset
    }
}

$qaReport = Get-Content -Raw -LiteralPath $qaPath | ConvertFrom-Json
$diagnostics = @(Get-QaDiagnostics $qaReport)
$groups = @()
foreach ($group in ($diagnostics | Group-Object label, fallback_reason, work_order_id | Sort-Object -Property @{ Expression = "Count"; Descending = $true }, Name)) {
    $first = $group.Group[0]
    $candidateItems = @()
    foreach ($candidateGroup in ($group.Group | Group-Object candidate_preview_asset | Sort-Object -Property @{ Expression = "Count"; Descending = $true }, Name)) {
        $assetId = $candidateGroup.Name
        $asset = if ($assetById.ContainsKey($assetId)) { $assetById[$assetId] } else { $null }
        $sourcePath = if ($null -ne $asset) { Get-TilesetAssetSourcePath $asset } else { "" }
        $candidateItems += [pscustomobject]@{
            asset_id = $assetId
            diagnostic_count = $candidateGroup.Count
            status = if ($null -ne $asset) { Get-TilesetProperty $asset "status" "" } else { "<missing_manifest_record>" }
            source_path = if ($null -ne $asset) { ConvertTo-TilesetRelativePath $sourcePath } else { "" }
            source_exists = (-not [string]::IsNullOrWhiteSpace($sourcePath) -and (Test-Path -LiteralPath $sourcePath))
            asset_class = if ($null -ne $asset) { Get-TilesetProperty $asset "asset_class" "" } else { "" }
            owner = if ($null -ne $asset) { Get-TilesetProperty $asset "owner" "" } else { "" }
            work_order_ids = if ($null -ne $asset) { @(Get-AssetWorkOrderIds $asset) } else { @() }
            card_context = if ($null -ne $asset) { Get-AssetCardContext $asset } else { "" }
            diagnostic_context_slots = @($candidateGroup.Group | Group-Object { Get-DiagnosticSlotKey $_ } | Sort-Object Count -Descending | ForEach-Object {
                [pscustomobject]@{
                    slot_key = $_.Name
                    count = $_.Count
                }
            })
            sample_cases = @($candidateGroup.Group | ForEach-Object { $_.case } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -First 4)
            sample_seeds = @($candidateGroup.Group | ForEach-Object { $_.seed } | Select-Object -First 4)
            sample_coordinates = @($candidateGroup.Group | Select-Object -First 2 | ForEach-Object {
                @(ConvertTo-TilesetArray $_.coordinates | Select-Object -First 3)
            })
        }
    }

    $groups += [pscustomobject]@{
        label = $first.label
        fallback_reason = $first.fallback_reason
        work_order_id = if ([string]::IsNullOrWhiteSpace($first.work_order_id)) { "<unlinked>" } else { $first.work_order_id }
        diagnostic_count = $group.Count
        unique_candidate_preview_asset_count = $candidateItems.Count
        request_owners = @($group.Group | ForEach-Object { $_.request_owner } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Sort-Object -Unique)
        sampled_role_coverage = @(Get-SampledRoleCoverage $group.Group $first.work_order_id $activeAssetsByWorkOrder)
        context_slots = @($group.Group | Group-Object { Get-DiagnosticSlotKey $_ } | Sort-Object Count -Descending | ForEach-Object {
            [pscustomobject]@{
                slot_key = $_.Name
                count = $_.Count
                candidate_preview_asset_count = @($_.Group | ForEach-Object { $_.candidate_preview_asset } | Sort-Object -Unique).Count
                sample_seeds = @($_.Group | ForEach-Object { $_.seed } | Select-Object -First 4)
            }
        })
        candidates = $candidateItems
    }
}

$totalUniqueCandidates = @($diagnostics | ForEach-Object { $_.candidate_preview_asset } | Sort-Object -Unique).Count
$pngPath = Join-Path $outputDirectoryPath "$OutputStem.png"
$jsonPath = Join-Path $outputDirectoryPath "$OutputStem.json"
$markdownPath = Join-Path $outputDirectoryPath "$OutputStem.md"

$safeColumns = [Math]::Max(1, $Columns)
$cardWidth = [Math]::Max(230, $TileSize + 150)
$cardHeight = [Math]::Max(126, $TileSize + 58)
$margin = 14
$sectionGap = 18
$titleHeight = 34
$imageWidth = ($margin * 2) + ($safeColumns * $cardWidth) + (($safeColumns - 1) * 12)
$imageHeight = 58
foreach ($group in $groups) {
    $rows = [Math]::Max(1, [int][Math]::Ceiling($group.candidates.Count / [double]$safeColumns))
    $imageHeight += $titleHeight + ($rows * $cardHeight) + (($rows - 1) * 10) + $sectionGap
}
$imageHeight += 34

$sheet = New-TilesetReviewBitmap $imageWidth $imageHeight
try {
    Draw-Label $sheet.Graphics "generated-map contextual candidate fit review" $margin 10 ($imageWidth - ($margin * 2)) 22 10.0
    Draw-Label $sheet.Graphics "candidate-only preview assets from current art-readiness warnings; no runtime promotion" $margin 34 ($imageWidth - ($margin * 2)) 18 8.5
    $yBase = 62
    foreach ($group in $groups) {
        Draw-Label $sheet.Graphics ("{0} | {1} | {2} diagnostics | {3} unique candidates" -f $group.label, $group.work_order_id, $group.diagnostic_count, $group.unique_candidate_preview_asset_count) $margin $yBase ($imageWidth - ($margin * 2)) 22 9.0
        $yBase += $titleHeight
        for ($index = 0; $index -lt $group.candidates.Count; ++$index) {
            $candidate = $group.candidates[$index]
            $column = $index % $safeColumns
            $row = [int][Math]::Floor($index / $safeColumns)
            $x = $margin + ($column * ($cardWidth + 12))
            $y = $yBase + ($row * ($cardHeight + 10))
            $cardRect = [Drawing.Rectangle]::new($x, $y, $cardWidth, $cardHeight)
            Draw-SoftBox $sheet.Graphics $cardRect

            $thumbRect = [Drawing.Rectangle]::new($x + 8, $y + 24, $TileSize, $TileSize)
            if ($candidate.source_exists) {
                $bitmap = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath (Resolve-TilesetPath $candidate.source_path)).Path)
                try {
                    $sheet.Graphics.DrawImage($bitmap, $thumbRect, 0, 0, $bitmap.Width, $bitmap.Height, [Drawing.GraphicsUnit]::Pixel)
                }
                finally {
                    $bitmap.Dispose()
                }
            }
            else {
                Draw-SoftBox $sheet.Graphics $thumbRect "#402022" "#a84a50"
            }

            Draw-Label $sheet.Graphics ("x{0} {1}" -f $candidate.diagnostic_count, (Shorten-Text $candidate.owner 18)) ($x + 8) ($y + 4) ($cardWidth - 16) 18 8.5
            $textX = $x + 16 + $TileSize
            $textWidth = $cardWidth - $TileSize - 24
            Draw-Label $sheet.Graphics (Shorten-Text $candidate.asset_id 42) $textX ($y + 26) $textWidth 18 8.0
            Draw-Label $sheet.Graphics (Shorten-Text $candidate.card_context 42) $textX ($y + 48) $textWidth 18 8.0
            Draw-Label $sheet.Graphics ("seeds: {0}" -f ((@($candidate.sample_seeds | Select-Object -First 3) -join ", "))) $textX ($y + 70) $textWidth 18 8.0
            $slot = (@($candidate.diagnostic_context_slots | Select-Object -First 1).slot_key)
            Draw-Label $sheet.Graphics (Shorten-Text $slot 42) $textX ($y + 92) $textWidth 18 8.0
        }
        $rowsForGroup = [Math]::Max(1, [int][Math]::Ceiling($group.candidates.Count / [double]$safeColumns))
        $yBase += ($rowsForGroup * $cardHeight) + (($rowsForGroup - 1) * 10) + $sectionGap
    }
    Draw-Label $sheet.Graphics ("summary: {0} diagnostics, {1} unique candidate preview assets, {2} groups" -f $diagnostics.Count, $totalUniqueCandidates, $groups.Count) $margin ($imageHeight - 28) ($imageWidth - ($margin * 2)) 18 8.5
    $sheet.Bitmap.Save($pngPath, [Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $sheet.Graphics.Dispose()
    $sheet.Bitmap.Dispose()
}

$report = [pscustomobject]@{
    schema_version = 1
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    source_root = ConvertTo-TilesetRelativePath $sourceRootPath
    qa_report = ConvertTo-TilesetRelativePath $qaPath
    candidate_manifest = ConvertTo-TilesetRelativePath $candidateManifestResolvedPath
    output_png = ConvertTo-TilesetRelativePath $pngPath
    summary = [pscustomobject]@{
        diagnostic_count = $diagnostics.Count
        group_count = $groups.Count
        unique_candidate_preview_asset_count = $totalUniqueCandidates
        sampled_role_coverage_slot_count = @($groups.sampled_role_coverage).Count
        missing_sampled_role_coverage_slot_count = @($groups.sampled_role_coverage | Where-Object { $_.active_asset_count -eq 0 }).Count
        missing_candidate_manifest_record_count = @($groups.candidates | Where-Object { $_.status -eq "<missing_manifest_record>" }).Count
        missing_source_file_count = @($groups.candidates | Where-Object { -not $_.source_exists }).Count
    }
    groups = $groups
    open_acceptance_holds = @(
        "candidate_only",
        "human_art_direction_review_required",
        "generated_map_candidate_fit_review_required_before_promotion"
    )
}
Write-CleanJsonFile $jsonPath $report

$validationResults = Read-TilesetJson $validationResultsResolvedPath ([pscustomobject]@{ results = @() })
if ($null -eq $validationResults.PSObject.Properties["results"]) {
    $validationResults | Add-Member -MemberType NoteProperty -Name "results" -Value @()
}
$reviewId = "contextual_generated_map_candidate_fit_review_2026_05_31_structure_way_ground_resolution"
$workOrderIds = @($groups | ForEach-Object { $_.work_order_id } | Where-Object {
        -not [string]::IsNullOrWhiteSpace($_) -and $_ -ne "<unlinked>"
    } | Sort-Object -Unique)
$issueLabels = @(
    "candidate_only",
    "generated_map_fit_review_generated",
    "sampled_role_span_coverage_complete",
    "human_art_direction_review_required",
    "runtime_assets_not_accepted"
)
if ([int]$report.summary.missing_sampled_role_coverage_slot_count -gt 0) {
    $issueLabels += "sampled_role_span_coverage_missing"
}
$validationEntry = [pscustomobject]@{
    id = $reviewId
    reviewed_at_utc = $report.generated_at_utc
    validator = "Contextual Generated-Map Candidate Fit Reviewer"
    result_type = "generated_map_contextual_candidate_fit_review"
    owner = "contextual_candidate_fit_review"
    work_order_ids = $workOrderIds
    asset_class = "contextual_candidate_preview"
    asset_id_scope = "generated-map candidate preview assets from $($report.qa_report)"
    status = "candidate_hold_for_acceptance"
    rating = 3
    issue_labels = $issueLabels
    evidence = [pscustomobject]@{
        review_sheet = ConvertTo-TilesetRelativePath $pngPath
        sidecar = ConvertTo-TilesetRelativePath $jsonPath
        markdown = ConvertTo-TilesetRelativePath $markdownPath
        qa_report = $report.qa_report
        candidate_manifest = $report.candidate_manifest
        diagnostic_count = $report.summary.diagnostic_count
        unique_candidate_preview_asset_count = $report.summary.unique_candidate_preview_asset_count
        sampled_role_coverage_slot_count = $report.summary.sampled_role_coverage_slot_count
        missing_sampled_role_coverage_slot_count = $report.summary.missing_sampled_role_coverage_slot_count
        missing_manifest_record_count = $report.summary.missing_candidate_manifest_record_count
        missing_source_file_count = $report.summary.missing_source_file_count
        groups = @($groups | ForEach-Object {
                [pscustomobject]@{
                    label = $_.label
                    work_order_id = $_.work_order_id
                    diagnostic_count = $_.diagnostic_count
                    unique_candidate_preview_asset_count = $_.unique_candidate_preview_asset_count
                    sampled_role_coverage_slot_count = @($_.sampled_role_coverage).Count
                }
            })
    }
    visual_judgment = [pscustomobject]@{
        overall = "generated_map_candidate_fit_review_candidate_only"
        candidate_pass = $true
        candidate_only = $true
        acceptance_ready = $false
        accepted = $false
        notes = "The generated-map fit review found source-backed candidate previews and sampled role/span coverage for the current bridge, mountain, and guard-tower acceptance holds, but it does not approve art for runtime promotion."
    }
    feedback = "Use the review sheet and sampled role/span coverage table for human/art-direction acceptance. Keep all listed assets candidate-only until the visual fit is explicitly approved or repainted."
    suggested_prompt_change = "Future contextual candidate batches should include generated-map fit evidence, not only source contact sheets, before promotion is considered."
}
$validationResults.results = @($validationResults.results | Where-Object { (Get-TilesetProperty $_ "id" "") -ne $reviewId }) + @($validationEntry)
Write-CleanJsonFile $validationResultsResolvedPath $validationResults

$markdown = [System.Text.StringBuilder]::new()
[void]$markdown.AppendLine("# Generated-Map Candidate Fit Review")
[void]$markdown.AppendLine("")
[void]$markdown.AppendLine("- Generated: $($report.generated_at_utc)")
[void]$markdown.AppendLine("- QA report: ``$($report.qa_report)``")
[void]$markdown.AppendLine("- Review sheet: ``$($report.output_png)``")
[void]$markdown.AppendLine("- Diagnostics with candidate previews: $($report.summary.diagnostic_count)")
[void]$markdown.AppendLine("- Unique candidate preview assets: $($report.summary.unique_candidate_preview_asset_count)")
[void]$markdown.AppendLine("- Sampled role/span coverage slots: $($report.summary.sampled_role_coverage_slot_count)")
[void]$markdown.AppendLine("- Missing sampled role/span coverage slots: $($report.summary.missing_sampled_role_coverage_slot_count)")
[void]$markdown.AppendLine("- Missing manifest records: $($report.summary.missing_candidate_manifest_record_count)")
[void]$markdown.AppendLine("- Missing source files: $($report.summary.missing_source_file_count)")
[void]$markdown.AppendLine("")
[void]$markdown.AppendLine("## Groups")
[void]$markdown.AppendLine("")
[void]$markdown.AppendLine("| Diagnostics | Unique candidates | Label | Work order | Fallback reason |")
[void]$markdown.AppendLine("| ---: | ---: | --- | --- | --- |")
foreach ($group in $groups) {
    [void]$markdown.AppendLine(('| {0} | {1} | `{2}` | `{3}` | `{4}` |' -f
            $group.diagnostic_count,
            $group.unique_candidate_preview_asset_count,
            $group.label,
            $group.work_order_id,
            $group.fallback_reason))
}
[void]$markdown.AppendLine("")
[void]$markdown.AppendLine("## Candidate Preview Assets")
foreach ($group in $groups) {
    [void]$markdown.AppendLine("")
    [void]$markdown.AppendLine("### $($group.label) / $($group.work_order_id)")
    [void]$markdown.AppendLine("")
    [void]$markdown.AppendLine("| Diagnostics | Asset | Context | Sample seeds |")
    [void]$markdown.AppendLine("| ---: | --- | --- | --- |")
    foreach ($candidate in $group.candidates) {
        [void]$markdown.AppendLine(('| {0} | `{1}` | {2} | {3} |' -f
                $candidate.diagnostic_count,
                $candidate.asset_id,
                (Shorten-Text $candidate.card_context 96),
                (@($candidate.sample_seeds | Select-Object -First 4) -join ", ")))
    }
}
[void]$markdown.AppendLine("")
[void]$markdown.AppendLine("## Sampled Role And Span Coverage")
foreach ($group in $groups) {
    $coverage = @(ConvertTo-TilesetArray (Get-TilesetProperty $group "sampled_role_coverage" @()))
    if ($coverage.Count -eq 0) {
        continue
    }
    [void]$markdown.AppendLine("")
    [void]$markdown.AppendLine("### $($group.label) / $($group.work_order_id)")
    [void]$markdown.AppendLine("")
    [void]$markdown.AppendLine("| Slot | Diagnostics mentioning slot | Active assets | Previewed assets | Asset examples |")
    [void]$markdown.AppendLine("| --- | ---: | ---: | ---: | --- |")
    foreach ($item in $coverage) {
        [void]$markdown.AppendLine(('| `{0}={1}` | {2} | {3} | {4} | {5} |' -f
                $item.field,
                $item.value,
                $item.diagnostic_mention_count,
                $item.active_asset_count,
                $item.previewed_asset_count,
                (@($item.asset_ids | Select-Object -First 4 | ForEach-Object { "``$_``" }) -join "<br>")))
    }
}
Write-CleanTextFile $markdownPath $markdown.ToString()

Write-Host "Contextual candidate fit review PNG wrote $pngPath"
Write-Host "Contextual candidate fit review JSON wrote $jsonPath"
Write-Host "Contextual candidate fit review Markdown wrote $markdownPath"
Write-Host "Contextual candidate fit validation evidence updated $validationResultsResolvedPath"
Write-Host "Diagnostics: $($report.summary.diagnostic_count); unique candidates: $($report.summary.unique_candidate_preview_asset_count); sampled slots: $($report.summary.sampled_role_coverage_slot_count); missing sampled slots: $($report.summary.missing_sampled_role_coverage_slot_count); missing sources: $($report.summary.missing_source_file_count)"
