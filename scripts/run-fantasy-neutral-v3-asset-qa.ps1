param(
    [string]$ManifestPath = "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3/pack_manifest.json",
    [string]$QaProfilePath = "Resources/Maps/Tilesets/Source/FantasyNeutral/v3/qa_profile.json",
    [string]$OutputRoot = "build/asset-qa",
    [int]$MapCount = 1000,
    [int]$Seed = 31001,
    [string]$VisionResultsPath = "",
    [switch]$SkipMapImages,
    [switch]$Strict
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

function Resolve-ProjectPath([string]$Path) {
    if ([IO.Path]::IsPathRooted($Path)) { return [IO.Path]::GetFullPath($Path) }
    return [IO.Path]::GetFullPath((Join-Path $repositoryRoot $Path))
}

function Get-Sha256([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '').ToLowerInvariant() }
    finally { $sha.Dispose(); $stream.Dispose() }
}

function Get-RelativePath([string]$FromDirectory, [string]$ToPath) {
    $fromUri = [Uri]((([IO.Path]::GetFullPath($FromDirectory)).TrimEnd('\') + '\'))
    $toUri = [Uri]([IO.Path]::GetFullPath($ToPath))
    return [Uri]::UnescapeDataString($fromUri.MakeRelativeUri($toUri).ToString()).Replace('\', '/')
}

function Get-Median([double[]]$Values) {
    if ($Values.Count -eq 0) { return 0.0 }
    $sorted = @($Values | Sort-Object)
    $middle = [int][Math]::Floor($sorted.Count / 2)
    if (($sorted.Count % 2) -eq 1) { return [double]$sorted[$middle] }
    return ([double]$sorted[$middle - 1] + [double]$sorted[$middle]) / 2.0
}

function Get-ImageMetrics([string]$Path) {
    $bitmap = [Drawing.Bitmap]::new($Path)
    try {
        $step = if ($bitmap.Width -ge 512) { 8 } else { 4 }
        [double]$sumL = 0; [double]$sumL2 = 0; [double]$sumS = 0
        [double]$edge = 0; [double]$alpha = 0; [double]$samples = 0
        for ($y = 0; $y -lt $bitmap.Height; $y += $step) {
            for ($x = 0; $x -lt $bitmap.Width; $x += $step) {
                $pixel = $bitmap.GetPixel($x, $y)
                $a = $pixel.A / 255.0
                $maximum = [Math]::Max($pixel.R, [Math]::Max($pixel.G, $pixel.B))
                $minimum = [Math]::Min($pixel.R, [Math]::Min($pixel.G, $pixel.B))
                $l = (0.2126 * $pixel.R + 0.7152 * $pixel.G + 0.0722 * $pixel.B) / 255.0
                $s = if ($maximum -eq 0) { 0.0 } else { ($maximum - $minimum) / [double]$maximum }
                $sumL += $l * $a; $sumL2 += $l * $l * $a; $sumS += $s * $a; $alpha += $a; ++$samples
                if ($x + $step -lt $bitmap.Width) {
                    $right = $bitmap.GetPixel($x + $step, $y)
                    $edge += [Math]::Abs($pixel.R - $right.R) + [Math]::Abs($pixel.G - $right.G) + [Math]::Abs($pixel.B - $right.B)
                }
                if ($y + $step -lt $bitmap.Height) {
                    $below = $bitmap.GetPixel($x, $y + $step)
                    $edge += [Math]::Abs($pixel.R - $below.R) + [Math]::Abs($pixel.G - $below.G) + [Math]::Abs($pixel.B - $below.B)
                }
            }
        }
        $opaqueWeight = [Math]::Max(0.0001, $alpha)
        $mean = $sumL / $opaqueWeight
        $contrast = [Math]::Sqrt([Math]::Max(0.0, $sumL2 / $opaqueWeight - $mean * $mean))
        [double]$borderAlpha = 0; [double]$borderSamples = 0; [double]$seamDelta = 0
        $borderStep = [Math]::Max(1, [int]($step / 2))
        for ($x = 0; $x -lt $bitmap.Width; $x += $borderStep) {
            $top = $bitmap.GetPixel($x, 0); $bottom = $bitmap.GetPixel($x, $bitmap.Height - 1)
            $borderAlpha += ($top.A + $bottom.A) / 510.0; $borderSamples += 2
            $seamDelta += ([Math]::Abs($top.R - $bottom.R) + [Math]::Abs($top.G - $bottom.G) + [Math]::Abs($top.B - $bottom.B)) / 765.0
        }
        for ($y = 0; $y -lt $bitmap.Height; $y += $borderStep) {
            $left = $bitmap.GetPixel(0, $y); $right = $bitmap.GetPixel($bitmap.Width - 1, $y)
            $borderAlpha += ($left.A + $right.A) / 510.0; $borderSamples += 2
            $seamDelta += ([Math]::Abs($left.R - $right.R) + [Math]::Abs($left.G - $right.G) + [Math]::Abs($left.B - $right.B)) / 765.0
        }
        return [pscustomobject]@{
            width = $bitmap.Width; height = $bitmap.Height
            alpha_coverage = [Math]::Round($alpha / [Math]::Max(1.0, $samples), 5)
            border_alpha = [Math]::Round($borderAlpha / [Math]::Max(1.0, $borderSamples), 5)
            luminance = [Math]::Round($mean, 5)
            saturation = [Math]::Round($sumS / $opaqueWeight, 5)
            contrast = [Math]::Round($contrast, 5)
            edge_energy = [Math]::Round($edge / [Math]::Max(1.0, $samples * 1530.0), 5)
            seam_delta = [Math]::Round($seamDelta / [Math]::Max(1.0, $borderSamples), 5)
        }
    } finally { $bitmap.Dispose() }
}

function Add-Finding($List, [string]$Code, [string]$Severity, [string]$AssetId, [string]$MapId, [int]$FindingSeed, [string]$Message, [string]$Evidence) {
    $List.Add([pscustomobject]@{ code=$Code; severity=$Severity; asset_id=$AssetId; map_id=$MapId; seed=$FindingSeed; message=$Message; evidence_path=$Evidence })
}

function Draw-Rotated([Drawing.Graphics]$Graphics, [Drawing.Image]$Image, [Drawing.RectangleF]$Rectangle, [int]$QuarterTurns) {
    $state = $Graphics.Save()
    try {
        $Graphics.TranslateTransform($Rectangle.X + $Rectangle.Width / 2.0, $Rectangle.Y + $Rectangle.Height / 2.0)
        $Graphics.RotateTransform([single](90 * ($QuarterTurns % 4)))
        $Graphics.DrawImage($Image, [Drawing.RectangleF]::new(-$Rectangle.Width / 2.0, -$Rectangle.Height / 2.0, $Rectangle.Width, $Rectangle.Height))
    } finally { $Graphics.Restore($state) }
}

$manifestFile = Resolve-ProjectPath $ManifestPath
$profileFile = Resolve-ProjectPath $QaProfilePath
$manifest = Get-Content -Raw -LiteralPath $manifestFile | ConvertFrom-Json
$profile = Get-Content -Raw -LiteralPath $profileFile | ConvertFrom-Json
if ([int]$manifest.schema_version -lt 3) { throw "V3 manifest must be enriched to schema version 3 first." }
$MapCount = [Math]::Max(1, [Math]::Min(1000, $MapCount))
$manifestHash = Get-Sha256 $manifestFile
$runId = "fnv3_{0}_{1}_{2}" -f $manifestHash.Substring(0, 12), $MapCount, $Seed
$outputDirectory = Join-Path (Resolve-ProjectPath $OutputRoot) $runId
$mapDirectory = Join-Path $outputDirectory "maps"
New-Item -ItemType Directory -Force -Path $mapDirectory | Out-Null

$findings = [Collections.Generic.List[object]]::new()
$metrics = [Collections.Generic.List[object]]::new()
$assetRoot = Split-Path -Parent $manifestFile
foreach ($asset in @($manifest.assets)) {
    $assetPath = Join-Path $assetRoot ([string]$asset.file).Replace('/', '\')
    if (-not (Test-Path -LiteralPath $assetPath -PathType Leaf)) {
        Add-Finding $findings "file.missing" "blocker" $asset.id "" 0 "Asset file is missing." $asset.file
        continue
    }
    $metric = Get-ImageMetrics $assetPath
    $metric | Add-Member -NotePropertyName asset_id -NotePropertyValue ([string]$asset.id)
    $metric | Add-Member -NotePropertyName family -NotePropertyValue ([string]$asset.family)
    $metric | Add-Member -NotePropertyName semantic_role -NotePropertyValue ([string]$asset.semantic_role)
    $metric | Add-Member -NotePropertyName file -NotePropertyValue ([string]$asset.file)
    $metrics.Add($metric)
    if ($metric.width -ne [int]$asset.dimensions.width -or $metric.height -ne [int]$asset.dimensions.height) {
        Add-Finding $findings "dimensions.mismatch" "blocker" $asset.id "" 0 "Actual dimensions differ from the manifest." $asset.file
    }
    foreach ($field in @("semantic_role", "allowed_map_types", "footprint", "anchors", "edge_contract", "lighting_profile", "palette_profile")) {
        if ($null -eq $asset.PSObject.Properties[$field]) { Add-Finding $findings "manifest.semantic_metadata_missing" "blocker" $asset.id "" 0 "Missing semantic field: $field" $asset.file }
    }
    if ([bool]$asset.transparent -and ($metric.alpha_coverage -le 0.02 -or $metric.alpha_coverage -ge 0.94)) {
        Add-Finding $findings "alpha.coverage_invalid" "blocker" $asset.id "" 0 "Transparent asset has implausible alpha coverage." $asset.file
    }
    if ([bool]$asset.transparent -and $metric.border_alpha -gt 0.35) {
        Add-Finding $findings "alpha.border_contamination" "warning" $asset.id "" 0 "Transparent stamp reaches too much of its image border." $asset.file
    }
    if (-not [bool]$asset.transparent -and $metric.alpha_coverage -lt 0.995) {
        Add-Finding $findings "alpha.opaque_material_has_holes" "blocker" $asset.id "" 0 "Opaque material contains transparent samples." $asset.file
    }
    if ([string]$asset.edge_contract -eq "seamless_material" -and $metric.seam_delta -gt 0.12) {
        Add-Finding $findings "edge.seam_delta_high" "blocker" $asset.id "" 0 "Opposite material edges differ too strongly for seamless repetition." $asset.file
    }
    if ([string]$asset.license_review_status -ne "approved") {
        Add-Finding $findings "license.review_pending" "warning" $asset.id "" 0 "Distribution/license review is still pending." $asset.file
    }
}

$metricNames = @("luminance", "saturation", "contrast", "edge_energy", "alpha_coverage", "border_alpha")
foreach ($familyGroup in @($metrics | Group-Object { "$($_.family)|$($_.semantic_role)" })) {
    if ($familyGroup.Count -lt 4) { continue }
    foreach ($metricName in $metricNames) {
        [double[]]$values = @($familyGroup.Group | ForEach-Object { [double]$_.$metricName })
        $median = Get-Median $values
        [double[]]$deviations = @($values | ForEach-Object { [Math]::Abs($_ - $median) })
        $mad = [Math]::Max(0.0001, (Get-Median $deviations))
        foreach ($entry in $familyGroup.Group) {
            $z = 0.6745 * [Math]::Abs([double]$entry.$metricName - $median) / $mad
            $propertyName = "z_$metricName"
            $entry | Add-Member -NotePropertyName $propertyName -NotePropertyValue ([Math]::Round($z, 3)) -Force
        }
    }
    foreach ($entry in $familyGroup.Group) {
        $strong = 0; $moderate = 0
        foreach ($metricName in $metricNames) {
            $z = [double]$entry.("z_$metricName")
            if ($z -gt [double]$profile.style_outlier.single_metric_robust_z) { ++$strong }
            if ($z -gt [double]$profile.style_outlier.multi_metric_robust_z) { ++$moderate }
        }
        if ($strong -gt 0 -or $moderate -ge [int]$profile.style_outlier.multi_metric_count) {
            Add-Finding $findings "style.statistical_outlier" "warning" $entry.asset_id "" 0 "Asset is a statistical style outlier inside its family." $entry.file
        }
    }
}

$groups = @("world", "region_travel", "city_settlement", "tactical_outdoor", "dungeon_cave")
$allowedFamilies = @{
    world = @("terrain_water", "transitions_details", "roads_rivers", "crossings", "vegetation", "landscape", "settlements", "map_symbols")
    region_travel = @("terrain_water", "transitions_details", "roads_rivers", "crossings", "vegetation", "landscape", "settlements", "tactical_outdoor", "map_symbols")
    city_settlement = @("terrain_water", "transitions_details", "roads_rivers", "crossings", "settlements", "tactical_outdoor", "map_symbols")
    tactical_outdoor = @("terrain_water", "transitions_details", "roads_rivers", "crossings", "vegetation", "landscape", "settlements", "tactical_outdoor", "map_symbols")
    dungeon_cave = @("terrain_water", "transitions_details", "cave_dungeon", "map_symbols")
}
$usage = [Collections.Generic.List[object]]::new()
$maps = [Collections.Generic.List[object]]::new()
$imageCache = @{}
$usageByAsset = @{}
$rotationCountByAsset = @{}
try {
    for ($mapIndex = 0; $mapIndex -lt $MapCount; ++$mapIndex) {
        $group = $groups[$mapIndex % $groups.Count]
        $localIndex = [int][Math]::Floor($mapIndex / [double]$groups.Count)
        $band = if ($localIndex -lt 20) { "reference" } elseif ($localIndex -lt 100) { "coverage" } elseif ($localIndex -lt 170) { "random" } else { "stress" }
        $mapSeed = $Seed + $mapIndex * 7919
        $pool = @($manifest.assets | Where-Object { $_.family -in $allowedFamilies[$group] } | Sort-Object family, variant)
        $mapId = "{0}_{1:d4}" -f $group, $localIndex
        $mapPath = Join-Path $mapDirectory "$mapId.png"
        $selected = [Collections.Generic.List[object]]::new()
        $selectedRotations = [Collections.Generic.List[int]]::new()
        $placementCount = 24
        $start = ($localIndex * 23 + ($mapIndex % 5) * 37) % [Math]::Max(1, $pool.Count)
        for ($slot = 0; $slot -lt $placementCount; ++$slot) {
            $asset = $pool[($start + $slot) % $pool.Count]
            $selected.Add($asset)
            $used = if ($rotationCountByAsset.ContainsKey($asset.id)) { [int]$rotationCountByAsset[$asset.id] } else { 0 }
            $allowed = @($asset.allowed_rotations_quarter_turns)
            $selectedRotations.Add([int]$allowed[$used % [Math]::Max(1, $allowed.Count)])
            $rotationCountByAsset[$asset.id] = $used + 1
        }

        if (-not $SkipMapImages) {
            $canvas = [Drawing.Bitmap]::new(960, 540, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
            $graphics = [Drawing.Graphics]::FromImage($canvas)
            try {
                $graphics.Clear([Drawing.Color]::FromArgb(255, 45, 42, 36))
                $graphics.CompositingQuality = [Drawing.Drawing2D.CompositingQuality]::HighQuality
                $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $terrainPool = @($pool | Where-Object family -eq "terrain_water")
                if ($terrainPool.Count -gt 0) {
                    $background = $terrainPool[($localIndex * 3) % $terrainPool.Count]
                    $backgroundPath = Join-Path $assetRoot ([string]$background.file).Replace('/', '\')
                    if (-not $imageCache.ContainsKey($background.id)) { $imageCache[$background.id] = [Drawing.Bitmap]::new($backgroundPath) }
                    for ($y = 0; $y -lt 540; $y += 135) { for ($x = 0; $x -lt 960; $x += 135) { $graphics.DrawImage($imageCache[$background.id], $x, $y, 136, 136) } }
                }
                for ($slot = 0; $slot -lt $selected.Count; ++$slot) {
                    $asset = $selected[$slot]
                    $column = $slot % 8; $row = [int][Math]::Floor($slot / 8.0)
                    $x = 30 + $column * 116 + (($mapSeed + $slot * 13) % 17) - 8
                    $y = 45 + $row * 158 + (($mapSeed + $slot * 29) % 19) - 9
                    $size = if ($asset.family -eq "landscape") { 104 } elseif ($asset.family -in @("settlements", "vegetation")) { 92 } else { 76 }
                    $rotation = $selectedRotations[$slot]
                    $sourcePath = Join-Path $assetRoot ([string]$asset.file).Replace('/', '\')
                    if (-not $imageCache.ContainsKey($asset.id)) { $imageCache[$asset.id] = [Drawing.Bitmap]::new($sourcePath) }
                    Draw-Rotated $graphics $imageCache[$asset.id] ([Drawing.RectangleF]::new($x, $y, $size, $size)) $rotation
                }
                $canvas.Save($mapPath, [Drawing.Imaging.ImageFormat]::Png)
            } finally { $graphics.Dispose(); $canvas.Dispose() }
        }

        for ($slot = 0; $slot -lt $selected.Count; ++$slot) {
            $asset = $selected[$slot]; $column = $slot % 8; $row = [int][Math]::Floor($slot / 8.0)
            $rotation = $selectedRotations[$slot]
            $entry = [pscustomobject]@{ asset_id=$asset.id; map_id=$mapId; map_group=$group; band=$band; seed=$mapSeed; x=(30+$column*116); y=(45+$row*158); width=92; height=92; rotation_quarter_turns=$rotation; image_path=("maps/$mapId.png") }
            $usage.Add($entry)
            if (-not $usageByAsset.ContainsKey($asset.id)) { $usageByAsset[$asset.id] = [Collections.Generic.List[object]]::new() }
            $usageByAsset[$asset.id].Add($entry)
        }
        $distinct = @($selected | Select-Object -ExpandProperty id -Unique).Count
        $repetition = 1.0 - ($distinct / [double][Math]::Max(1, $selected.Count))
        if ($repetition -gt 0.08) { Add-Finding $findings "composition.repetition_high" "warning" "" $mapId $mapSeed "More than eight percent of placements repeat an identical asset." "maps/$mapId.png" }
        $maps.Add([pscustomobject]@{ id=$mapId; group=$group; band=$band; seed=$mapSeed; image_path=("maps/$mapId.png"); asset_ids=@($selected | ForEach-Object id); repetition_score=[Math]::Round($repetition, 4) })
    }
} finally {
    foreach ($bitmap in $imageCache.Values) { if ($null -ne $bitmap) { $bitmap.Dispose() } }
}

$coverage = [Collections.Generic.List[object]]::new()
foreach ($asset in @($manifest.assets)) {
    $assetUsage = if ($usageByAsset.ContainsKey($asset.id)) { @($usageByAsset[$asset.id]) } else { @() }
    $distinctMaps = @($assetUsage | Select-Object -ExpandProperty map_id -Unique).Count
    $rotations = @($assetUsage | Select-Object -ExpandProperty rotation_quarter_turns -Unique | Sort-Object)
    $requiredRotations = @($asset.allowed_rotations_quarter_turns | Sort-Object)
    $missingRotations = @($requiredRotations | Where-Object { $_ -notin $rotations })
    $coverageState = if ($assetUsage.Count -ge [int]$profile.coverage.minimum_placements_per_asset -and $distinctMaps -ge [int]$profile.coverage.minimum_distinct_maps_per_asset -and $missingRotations.Count -eq 0) { "covered" } else { "insufficient" }
    if ($coverageState -ne "covered") { Add-Finding $findings "coverage.insufficient" "blocker" $asset.id "" 0 "Asset does not meet placement, map, or rotation coverage." $asset.file }
    $coverage.Add([pscustomobject]@{ asset_id=$asset.id; placements=$assetUsage.Count; distinct_maps=$distinctMaps; rotations=$rotations; missing_rotations=$missingRotations; status=$coverageState; evidence=@($assetUsage | Select-Object -First 3) })
}

$visionTriageAvailable = $false
if (-not [string]::IsNullOrWhiteSpace($VisionResultsPath)) {
    $visionFile = Resolve-ProjectPath $VisionResultsPath
    if (Test-Path -LiteralPath $visionFile) {
        $vision = Get-Content -Raw -LiteralPath $visionFile | ConvertFrom-Json
        $visionTriageAvailable = $true
        foreach ($item in @($vision.findings)) {
            Add-Finding $findings ("vision." + [string]$item.code) "warning" ([string]$item.asset_id) ([string]$item.map_id) ([int]$item.seed) ([string]$item.message) ([string]$item.evidence_path)
        }
    }
}

# Image-model output is triage only. Without it every generated map remains a
# required human review. With it, retain a deterministic minimum set plus every
# map that any automatic or image-model check marked.
$markedMapIds = @($findings | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_.map_id) } | Select-Object -ExpandProperty map_id -Unique)
if ($visionTriageAvailable) {
    $findingCounts = @{}
    foreach ($finding in @($findings | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_.map_id) })) {
        if (-not $findingCounts.ContainsKey([string]$finding.map_id)) { $findingCounts[[string]$finding.map_id] = 0 }
        $findingCounts[[string]$finding.map_id]++
    }
    $worst = @($maps | Sort-Object @{Expression={ if ($findingCounts.ContainsKey([string]$_.id)) { -[int]$findingCounts[[string]$_.id] } else { 0 } }}, @{Expression={ -[double]$_.repetition_score }}, id | Select-Object -First ([Math]::Min(50, $maps.Count)) -ExpandProperty id)
    $coverageReview = @($maps | Where-Object band -eq "coverage" | Sort-Object id | Select-Object -First ([Math]::Min(25, $maps.Count)) -ExpandProperty id)
    $randomReview = @($maps | Where-Object band -eq "random" | Sort-Object @{Expression={ ([int64]$_.seed * 1103515245L + 12345L) % 2147483647L }} | Select-Object -First ([Math]::Min(25, $maps.Count)) -ExpandProperty id)
    $requiredMapReviewIds = @($worst + $coverageReview + $randomReview + $markedMapIds | Select-Object -Unique)
    $mapReviewMode = "triaged_minimum"
} else {
    $requiredMapReviewIds = @($maps | Select-Object -ExpandProperty id)
    $mapReviewMode = "all_maps_no_vision_triage"
}
$reviewRequirements = [ordered]@{
    vision_triage_available = $visionTriageAvailable
    map_review_mode = $mapReviewMode
    minimum_full_map_reviews = if ($visionTriageAvailable) { [Math]::Min(100, $maps.Count) } else { $maps.Count }
    required_map_count = $requiredMapReviewIds.Count
    required_map_ids = $requiredMapReviewIds
    marked_map_ids = $markedMapIds
}

$report = [ordered]@{
    schema_version = 1; run_id = $runId; generated_at_utc = [DateTime]::UtcNow.ToString("o")
    style_id = $manifest.style_id; style_version = $manifest.style_version; art_direction_id = $profile.art_direction_id
    art_direction_approval_status = $profile.approval_status; manifest_sha256 = $manifestHash; seed = $Seed
    requested_map_count = $MapCount; generated_map_count = $maps.Count; asset_count = @($manifest.assets).Count
    summary = [ordered]@{
        blocker_count = @($findings | Where-Object severity -eq "blocker").Count
        warning_count = @($findings | Where-Object severity -eq "warning").Count
        covered_asset_count = @($coverage | Where-Object status -eq "covered").Count
        insufficient_asset_count = @($coverage | Where-Object status -ne "covered").Count
        production_ready = $false
    }
    review_requirements = $reviewRequirements
    metrics = $metrics; coverage = $coverage; findings = $findings; maps = $maps
}
$reportPath = Join-Path $outputDirectory "asset_qa_report.json"
$report | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $reportPath -Encoding utf8

$requestsPath = Join-Path $outputDirectory "vision_triage_requests.jsonl"
$requestLines = [Collections.Generic.List[string]]::new()
foreach ($map in $maps) {
    $requestLines.Add(([ordered]@{ schema_version=1; kind="map"; id=$map.id; image=$map.image_path; seed=$map.seed; prompt="Flag style breaks, cartoon outlines, perspective errors, scale mismatch, unreadable overlaps, repetition, and unnatural composition. Never approve assets." } | ConvertTo-Json -Compress))
}
foreach ($asset in @($manifest.assets)) {
    $requestLines.Add(([ordered]@{ schema_version=1; kind="asset"; id=$asset.id; image=(Get-RelativePath $outputDirectory (Join-Path $assetRoot ([string]$asset.file).Replace('/', '\'))); family=$asset.family; semantic_role=$asset.semantic_role; prompt="Compare against fantasy_neutral_warm_painterly@1. Flag cartoon/mobile styling, hard black outlines, perspective, light, palette, transparency, scale, and semantic-role problems. Never approve assets." } | ConvertTo-Json -Compress))
}
$requestLines | Set-Content -LiteralPath $requestsPath -Encoding utf8

$dashboardAssets = @($manifest.assets | ForEach-Object {
    $id = [string]$_.id; $metric = $metrics | Where-Object asset_id -eq $id | Select-Object -First 1; $cover = $coverage | Where-Object asset_id -eq $id | Select-Object -First 1
    [ordered]@{ id=$id; family=$_.family; semantic_role=$_.semantic_role; status=$_.status; image=(Get-RelativePath $outputDirectory (Join-Path $assetRoot ([string]$_.file).Replace('/', '\'))); metrics=$metric; coverage=$cover; findings=@($findings | Where-Object asset_id -eq $id) }
})
$dashboardData = [ordered]@{ run_id=$runId; manifest_sha256=$manifestHash; summary=$report.summary; review_requirements=$reviewRequirements; assets=$dashboardAssets; maps=$maps; findings=$findings }
$dashboardJson = ($dashboardData | ConvertTo-Json -Depth 12 -Compress).Replace('</', '<\/')
$html = @'
<!doctype html><html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>FantasyNeutral V3 – Asset-QA</title><style>
:root{color-scheme:dark;--bg:#151512;--surface:#22221f;--line:#45423a;--text:#eee8dc;--muted:#aaa79f;--amber:#d69a36;--teal:#42b9ad;--red:#e46f64}*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:14px Inter,Segoe UI,sans-serif}header{position:sticky;top:0;z-index:4;padding:18px 24px;background:#191916ee;border-bottom:1px solid var(--line)}h1{font-size:21px;margin:0 0 10px}.toolbar{display:flex;gap:10px;flex-wrap:wrap}input,select,button{background:#2d2b25;color:var(--text);border:1px solid var(--line);border-radius:6px;padding:8px}button{cursor:pointer}button.primary{background:#6b4b1f;border-color:var(--amber)}main{padding:18px 24px}.summary{display:flex;gap:12px;flex-wrap:wrap;margin-bottom:16px}.pill{padding:7px 10px;border-radius:999px;background:var(--surface)}.bad{color:var(--red)}.ok{color:var(--teal)}.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(330px,1fr));gap:14px}.card{background:var(--surface);border:1px solid var(--line);border-radius:10px;overflow:hidden}.card.blocker,.card.required:not(.reviewed){border-color:var(--red)}.visual{height:190px;background:#111;display:flex;align-items:center;justify-content:center}.visual img{max-width:100%;max-height:100%;image-rendering:auto}.body{padding:12px}.muted{color:var(--muted)}.ratings{display:grid;grid-template-columns:1fr 58px;gap:5px;margin:8px 0}.evidence a{color:#e6b65d;margin-right:8px}.finding{font-size:12px;color:#e8ad6b}.finding.blocker{color:var(--red)}.maps{display:none}.maps.active{display:grid}.assets.hidden{display:none}
</style></head><body><header><h1 data-de="FantasyNeutral V3 – Prüfstand" data-en="FantasyNeutral V3 – Review">FantasyNeutral V3 – Prüfstand</h1><div class="toolbar"><input id="reviewer" placeholder="Prüfer / Reviewer"><input id="search" placeholder="Asset suchen"><select id="family"><option value="">Alle Familien</option></select><select id="state"><option value="">Alle Zustände</option><option>candidate</option><option>needs_repaint</option><option>accepted</option></select><button onclick="setLanguage()">DE / EN</button><button onclick="showMaps()">Karten</button><button class="primary" onclick="exportDecisions()">Entscheidungen exportieren</button></div></header><main><div class="summary" id="summary"></div><section class="grid assets" id="assets"></section><section class="grid maps" id="maps"></section></main>
<script>const DATA=__DASHBOARD_JSON__;let lang='de';const assetDecisions=JSON.parse(localStorage.getItem(DATA.run_id+':assets')||localStorage.getItem(DATA.run_id)||'{}'),mapDecisions=JSON.parse(localStorage.getItem(DATA.run_id+':maps')||'{}');const requiredMaps=new Set(DATA.review_requirements.required_map_ids||[]);const esc=s=>String(s??'').replace(/[&<>\"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','\"':'&quot;'}[c]));
function t(de,en){return lang==='de'?de:en}function setLanguage(){lang=lang==='de'?'en':'de';document.documentElement.lang=lang;render()}function assetDecision(id){return assetDecisions[id]||{verdict:'pending',style_fit:0,perspective:0,palette_lighting:0,scale_readability:0,contextual_fit:0,license_approved:false,reviewer:'',reason:''}}function mapDecision(id){return mapDecisions[id]||{reviewed:false,style:0,composition:0,readability:0,reason:''}}
function save(id,key,value){const d=assetDecision(id);d[key]=key.includes('fit')||key==='perspective'||key.includes('lighting')||key.includes('readability')?Number(value):value;assetDecisions[id]=d;localStorage.setItem(DATA.run_id+':assets',JSON.stringify(assetDecisions))}function saveMap(id,key,value){const d=mapDecision(id);d[key]=key==='reviewed'?Boolean(value):(key==='reason'?value:Number(value));mapDecisions[id]=d;localStorage.setItem(DATA.run_id+':maps',JSON.stringify(mapDecisions));render()}
function render(){const reviewed=DATA.review_requirements.required_map_ids.filter(id=>mapDecision(id).reviewed).length;document.getElementById('summary').innerHTML=`<span class="pill">${DATA.assets.length} Assets</span><span class="pill bad">${DATA.summary.blocker_count} Blocker</span><span class="pill">${DATA.summary.warning_count} ${t('Warnungen','Warnings')}</span><span class="pill ok">${DATA.summary.covered_asset_count} ${t('abgedeckt','covered')}</span><span class="pill ${reviewed<DATA.review_requirements.required_map_count?'bad':'ok'}">${reviewed}/${DATA.review_requirements.required_map_count} ${t('Pflichtkarten geprüft','required maps reviewed')}</span>`;const families=[...new Set(DATA.assets.map(a=>a.family))];const f=document.getElementById('family');if(f.options.length===1)families.forEach(x=>f.add(new Option(x,x)));filterAssets();document.getElementById('maps').innerHTML=DATA.maps.map(mapCard).join('')}
function filterAssets(){const q=document.getElementById('search').value.toLowerCase(),fam=document.getElementById('family').value,state=document.getElementById('state').value;const list=DATA.assets.filter(a=>(!q||(a.id+' '+a.semantic_role).toLowerCase().includes(q))&&(!fam||a.family===fam)&&(!state||a.status===state));document.getElementById('assets').innerHTML=list.map(card).join('')}
function card(a){const d=assetDecision(a.id),block=a.findings.some(f=>f.severity==='blocker');const evidence=(a.coverage?.evidence||[]).map(e=>`<a href="${e.image_path}">${e.map_id}</a>`).join('');const rating=(key,de,en)=>`<label>${t(de,en)}</label><input type="number" min="0" max="5" value="${d[key]||0}" onchange="save('${a.id}','${key}',this.value)">`;return `<article class="card ${block?'blocker':''}"><div class="visual"><img loading="lazy" src="${a.image}"></div><div class="body"><b>${a.id}</b><div class="muted">${a.family} · ${a.semantic_role} · ${a.status}</div><div>${a.coverage?.placements||0} ${t('Platzierungen','placements')} / ${a.coverage?.distinct_maps||0} ${t('Karten','maps')}</div><div class="evidence">${evidence}</div>${a.findings.map(f=>`<div class="finding ${f.severity}">${f.code}: ${esc(f.message)}</div>`).join('')}<select onchange="save('${a.id}','verdict',this.value)"><option value="pending" ${d.verdict==='pending'?'selected':''}>${t('Ausstehend','Pending')}</option><option value="accept" ${d.verdict==='accept'?'selected':''}>${t('Freigeben','Accept')}</option><option value="repaint" ${d.verdict==='repaint'?'selected':''}>${t('Nachbearbeiten','Repaint')}</option><option value="replace" ${d.verdict==='replace'?'selected':''}>${t('Ersetzen','Replace')}</option></select><div class="ratings">${rating('style_fit','Stil','Style')}${rating('perspective','Perspektive','Perspective')}${rating('palette_lighting','Palette/Licht','Palette/light')}${rating('scale_readability','Maßstab/Lesbarkeit','Scale/readability')}${rating('contextual_fit','Kontext','Context')}</div><label><input type="checkbox" ${d.license_approved?'checked':''} onchange="save('${a.id}','license_approved',this.checked)"> ${t('Herkunft/Lizenz geprüft','Provenance/license reviewed')}</label><input style="width:100%" placeholder="${t('Begründung / Reviewer','Reason / reviewer')}" value="${esc(d.reason)}" onchange="save('${a.id}','reason',this.value)"></div></article>`}
function mapCard(m){const d=mapDecision(m.id),required=requiredMaps.has(m.id);const rating=(key,de,en)=>`<label>${t(de,en)}</label><input type="number" min="0" max="5" value="${d[key]||0}" onchange="saveMap('${m.id}','${key}',this.value)">`;return `<article class="card ${required?'required':''} ${d.reviewed?'reviewed':''}"><div class="visual"><a href="${m.image_path}"><img loading="lazy" src="${m.image_path}"></a></div><div class="body"><b>${m.id}</b> ${required?`<span class="bad">${t('Pflicht','Required')}</span>`:''}<div class="muted">${m.group} · ${m.band} · Seed ${m.seed}</div><label><input type="checkbox" ${d.reviewed?'checked':''} onchange="saveMap('${m.id}','reviewed',this.checked)"> ${t('Vollkarte geprüft','Full map reviewed')}</label><div class="ratings">${rating('style','Stil','Style')}${rating('composition','Komposition','Composition')}${rating('readability','Lesbarkeit','Readability')}</div><input style="width:100%" value="${esc(d.reason)}" placeholder="${t('Notiz / Begründung','Note / reason')}" onchange="saveMap('${m.id}','reason',this.value)"></div></article>`}
function showMaps(){document.getElementById('assets').classList.toggle('hidden');document.getElementById('maps').classList.toggle('active')}function exportDecisions(){const reviewer=document.getElementById('reviewer').value.trim();if(!reviewer){alert(t('Bitte Prüfer eintragen.','Enter a reviewer.'));return}const payload={schema_version:2,run_id:DATA.run_id,manifest_sha256:DATA.manifest_sha256,reviewer,exported_at_utc:new Date().toISOString(),decisions:Object.entries(assetDecisions).map(([asset_id,d])=>({asset_id,...d})),map_reviews:Object.entries(mapDecisions).map(([map_id,d])=>({map_id,...d}))};const blob=new Blob([JSON.stringify(payload,null,2)],{type:'application/json'}),a=document.createElement('a');a.href=URL.createObjectURL(blob);a.download='review_decisions.json';a.click();URL.revokeObjectURL(a.href)}document.getElementById('search').oninput=filterAssets;document.getElementById('family').onchange=filterAssets;document.getElementById('state').onchange=filterAssets;render();</script></body></html>
'@
$html = $html.Replace("__DASHBOARD_JSON__", $dashboardJson)
$htmlPath = Join-Path $outputDirectory "index.html"
$html | Set-Content -LiteralPath $htmlPath -Encoding utf8

Write-Host "FantasyNeutral V3 asset QA: $outputDirectory"
Write-Host "Maps: $($maps.Count); assets covered: $($report.summary.covered_asset_count)/$($report.asset_count); blockers: $($report.summary.blocker_count)"
Write-Host "Dashboard: $htmlPath"
if ($Strict -and [int]$report.summary.blocker_count -gt 0) { exit 2 }
