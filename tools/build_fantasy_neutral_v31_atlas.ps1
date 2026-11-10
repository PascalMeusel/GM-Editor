param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3",
    [string]$OutputRoot = "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3_1"
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$sourceRootPath = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $SourceRoot))
$outputRootPath = [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputRoot))
$sourceManifest = Get-Content (Join-Path $sourceRootPath 'pack_manifest.json') -Raw | ConvertFrom-Json

# The original source sheet contained visually valid materials but its last
# terrain rows were assigned semantic names one row too early. Keep the owner-
# approved pixels and correct only their runtime meaning in the V3.1 overlay.
$baseSemanticOverrides = New-Object System.Collections.Generic.List[object]
foreach ($asset in $sourceManifest.assets) {
    if ([string]$asset.family -ne 'terrain_water') { continue }
    $variant = [int]$asset.variant
    $semantic = if ($variant -le 8) { 'grass_temperate' }
        elseif ($variant -le 12) { 'forest_floor' }
        elseif ($variant -le 16) { 'earth' }
        elseif ($variant -le 19) { 'mud' }
        elseif ($variant -le 22) { 'gravel' }
        elseif ($variant -le 26) { 'field_soil' }
        elseif ($variant -le 30) { 'coastal_sand' }
        elseif ($variant -le 32) { 'gravel' }
        elseif ($variant -le 36) { 'earth' }
        elseif ($variant -le 38) { 'coastal_sand' }
        elseif ($variant -le 42) { 'water_shallow' }
        elseif ($variant -le 47) { 'water_deep' }
        elseif ($variant -eq 48) { 'cave_floor' }
        else { 'dressed_stone_floor' }
    if ([string]$asset.semantic_role -ne $semantic) {
        $baseSemanticOverrides.Add([ordered]@{ id = [string]$asset.id; semantic_role = $semantic; reason = 'corrected visual classification in V3.1' })
        $asset.semantic_role = $semantic
    }
}

if (Test-Path -LiteralPath $outputRootPath) {
    $resolvedOutput = [System.IO.Path]::GetFullPath($outputRootPath)
    $resolvedExpectedParent = [System.IO.Path]::GetFullPath((Join-Path $sourceRootPath '..'))
    if (-not $resolvedOutput.StartsWith($resolvedExpectedParent, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to replace output outside the generated FantasyNeutral directory: $resolvedOutput"
    }
    Remove-Item -LiteralPath $resolvedOutput -Recurse -Force
}
New-Item -ItemType Directory -Path $outputRootPath | Out-Null

$assetsBySemantic = @{}
foreach ($asset in $sourceManifest.assets) {
    $key = [string]$asset.semantic_role
    if (-not $assetsBySemantic.ContainsKey($key)) { $assetsBySemantic[$key] = @() }
    $assetsBySemantic[$key] += $asset
}

function Get-SourceAsset([string]$Semantic, [int]$Index) {
    $choices = @($assetsBySemantic[$Semantic])
    if ($choices.Count -eq 0) { throw "No approved V3 source asset for semantic role '$Semantic'." }
    return $choices[$Index % $choices.Count]
}

function New-Canvas([int]$Width, [int]$Height, [bool]$Transparent) {
    $bitmap = New-Object System.Drawing.Bitmap($Width, $Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.Clear($(if ($Transparent) { [System.Drawing.Color]::Transparent } else { [System.Drawing.Color]::FromArgb(255, 78, 101, 55) }))
    return @($bitmap, $graphics)
}

function Open-Bitmap([object]$Asset) {
    $path = Join-Path $sourceRootPath ([string]$Asset.file)
    $source = [System.Drawing.Image]::FromFile($path)
    try { return New-Object System.Drawing.Bitmap($source) }
    finally { $source.Dispose() }
}

function Save-Bitmap([System.Drawing.Bitmap]$Bitmap, [string]$RelativePath) {
    $target = Join-Path $outputRootPath $RelativePath
    New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
    $Bitmap.Save($target, [System.Drawing.Imaging.ImageFormat]::Png)
}

$resultAssets = New-Object System.Collections.Generic.List[object]
function Add-ManifestAsset([string]$Id, [string]$Family, [string]$Role, [string]$Semantic,
                           [string]$RelativePath, [int]$Width, [int]$Height,
                           [string[]]$Allowed, [object[]]$Sources, [string]$RenderingClass,
                           [double]$MinimumZoom, [double]$MaximumZoom, [int]$FootprintWidth, [int]$FootprintHeight) {
    $sourceIds = @($Sources | ForEach-Object { [string]$_.id })
    $resultAssets.Add([ordered]@{
        id = $Id; family = $Family; role = $Role; semantic_role = $Semantic
        rendering_class = $RenderingClass; file = $RelativePath
        dimensions = [ordered]@{ width = $Width; height = $Height }
        transparent = $Role -ne 'terrain_material'; status = 'accepted'
        allowed_map_types = $Allowed
        minimum_display_cells = $(if ($Allowed -contains 'world') { 2.0 } else { 1.0 })
        maximum_display_cells = $(if ($Role -eq 'map_symbol') { 2.0 } else { 8.0 })
        footprint = [ordered]@{ width = $FootprintWidth; height = $FootprintHeight }
        placement_density = $(if ($Role -eq 'terrain_material') { 1.0 } elseif ($Role -eq 'map_symbol') { 0.08 } else { 0.16 })
        cluster_spacing = $(if ($Allowed -contains 'world') { 5.0 } else { 3.0 })
        zoom_range = [ordered]@{ minimum = $MinimumZoom; maximum = $MaximumZoom }
        perspective = 'orthographic_top_down'; style = 'fantasy_neutral_warm_painterly'
        lighting_profile = 'neutral_overhead_soft'; palette_profile = 'muted_warm_natural'
        provenance = [ordered]@{ method = 'deterministic_composite_from_accepted_v3'; source_asset_ids = $sourceIds; license = 'inherits accepted FantasyNeutral V3 sources' }
    })
}

# 12 macro materials. They use approved seamless sources without repainting, at
# 1024 px so one image can cover several world/region cells without blur.
$materialRoles = @('grass_temperate','grass_temperate','forest_floor','forest_floor','earth','coastal_sand','water_shallow','water_shallow','water_deep','water_deep','field_soil','mud')
for ($index = 0; $index -lt $materialRoles.Count; $index++) {
    $semantic = $materialRoles[$index]
    $source = Get-SourceAsset $semantic $index
    $parts = New-Canvas 1024 1024 $false; $bitmap = $parts[0]; $graphics = $parts[1]
    $image = Open-Bitmap $source
    try { $graphics.DrawImage($image, 0, 0, 1024, 1024) } finally { $image.Dispose(); $graphics.Dispose() }
    $relative = 'macro_materials/fantasy_neutral__v31__macro_material__{0:d2}.png' -f ($index + 1)
    Save-Bitmap $bitmap $relative; $bitmap.Dispose()
    Add-ManifestAsset ('fantasy_neutral.v31.macro_material.{0:d2}' -f ($index + 1)) 'macro_materials' 'terrain_material' $semantic $relative 1024 1024 @('world','region') @($source) 'material' 0.20 1.8 6 6
}

# 16 large transition masks.
for ($index = 0; $index -lt 16; $index++) {
    $source = Get-SourceAsset 'organic_blend_mask' $index
    $parts = New-Canvas 1024 1024 $true; $bitmap = $parts[0]; $graphics = $parts[1]
    $image = Open-Bitmap $source
    try { $graphics.DrawImage($image, 0, 0, 1024, 1024) } finally { $image.Dispose(); $graphics.Dispose() }
    $relative = 'boundary_masks/fantasy_neutral__v31__boundary_mask__{0:d2}.png' -f ($index + 1)
    Save-Bitmap $bitmap $relative; $bitmap.Dispose()
    Add-ManifestAsset ('fantasy_neutral.v31.boundary_mask.{0:d2}' -f ($index + 1)) 'boundary_masks' 'blend_mask_or_decal' 'organic_blend_mask' $relative 1024 1024 @('world','region') @($source) 'boundary_mask' 0.20 2.0 6 6
}

# 12 connected mountain compositions built from approved ridge/peak assets.
$mountainRoles = @('connected_ridge','peak','foothill','slope','mountain_pass')
for ($index = 0; $index -lt 12; $index++) {
    $sources = @(
        (Get-SourceAsset 'peak' $index),
        (Get-SourceAsset 'peak' ($index + 2)),
        (Get-SourceAsset 'peak' ($index + 4)))
    $parts = New-Canvas 1024 640 $true; $bitmap = $parts[0]; $graphics = $parts[1]
    $placements = @(@(30,170,390,390), @(310,35,480,480), @(640,180,360,360))
    for ($part = 0; $part -lt 3; $part++) {
        $image = Open-Bitmap $sources[$part]
        try {
            $p = $placements[$part]
            # Source-sheet crops can contain tiny disconnected neighbor slivers
            # at their outer edge. Macro ranges use only the coherent center
            # silhouette and overlap it with two approved peaks.
            $sourceInsetX = [math]::Max(24, [int]($image.Width * 0.10))
            $sourceInsetY = [math]::Max(16, [int]($image.Height * 0.06))
            $destination = New-Object Drawing.Rectangle($p[0], $p[1], $p[2], $p[3])
            $sourceRectangle = New-Object Drawing.Rectangle($sourceInsetX, $sourceInsetY,
                ($image.Width - 2 * $sourceInsetX), ($image.Height - 2 * $sourceInsetY))
            $graphics.DrawImage($image, $destination, $sourceRectangle, [Drawing.GraphicsUnit]::Pixel)
        } finally { $image.Dispose() }
    }
    $graphics.Dispose()
    $relative = 'mountain_ranges/fantasy_neutral__v31__mountain_range__{0:d2}.png' -f ($index + 1)
    Save-Bitmap $bitmap $relative; $bitmap.Dispose()
    Add-ManifestAsset ('fantasy_neutral.v31.mountain_range.{0:d2}' -f ($index + 1)) 'mountain_ranges' 'landscape_stamp' 'connected_ridge' $relative 1024 640 @('world','region') $sources 'cluster' 0.25 2.5 7 4
}

# 8 broad forest clusters.
$forestRoles = @('mixed_tree_group','deciduous_group','conifer_group','forest_edge')
for ($index = 0; $index -lt 8; $index++) {
    $sources = @(); for ($part = 0; $part -lt 5; $part++) { $sources += Get-SourceAsset $forestRoles[($index + $part) % $forestRoles.Count] ($index + $part) }
    $parts = New-Canvas 1024 768 $true; $bitmap = $parts[0]; $graphics = $parts[1]
    $placements = @(@(20,160,360,360), @(230,40,390,390), @(500,190,360,360), @(650,40,350,350), @(350,350,390,390))
    for ($part = 0; $part -lt 5; $part++) {
        $image = Open-Bitmap $sources[$part]
        try { $p = $placements[$part]; $graphics.DrawImage($image, $p[0], $p[1], $p[2], $p[3]) } finally { $image.Dispose() }
    }
    $graphics.Dispose()
    $relative = 'forest_clusters/fantasy_neutral__v31__forest_cluster__{0:d2}.png' -f ($index + 1)
    Save-Bitmap $bitmap $relative; $bitmap.Dispose()
    Add-ManifestAsset ('fantasy_neutral.v31.forest_cluster.{0:d2}' -f ($index + 1)) 'forest_clusters' 'vegetation_stamp' 'mixed_tree_group' $relative 1024 768 @('world','region') $sources 'cluster' 0.25 2.5 6 5
}

# 6 overview curve surfaces; geometry remains generated as smooth splines.
$curveRoles = @('road_dirt','road_gravel','road_stone','river','river_deep','path')
for ($index = 0; $index -lt 6; $index++) {
    $semantic = $curveRoles[$index]; $source = Get-SourceAsset $semantic $index
    $parts = New-Canvas 1024 1024 $true; $bitmap = $parts[0]; $graphics = $parts[1]
    $image = Open-Bitmap $source
    try { $graphics.DrawImage($image, 0, 0, 1024, 1024) } finally { $image.Dispose(); $graphics.Dispose() }
    $relative = 'overview_curves/fantasy_neutral__v31__overview_curve__{0:d2}.png' -f ($index + 1)
    Save-Bitmap $bitmap $relative; $bitmap.Dispose()
    Add-ManifestAsset ('fantasy_neutral.v31.overview_curve.{0:d2}' -f ($index + 1)) 'overview_curves' 'curve_profile' $semantic $relative 1024 1024 @('world','region') @($source) 'curve_profile' 0.20 2.5 1 1
}

# 6 compact overview symbols.
$symbolRoles = @('capital','city','harbor','fortress','town','village')
for ($index = 0; $index -lt 6; $index++) {
    $semantic = $symbolRoles[$index]; $source = Get-SourceAsset $semantic 0
    $parts = New-Canvas 256 256 $true; $bitmap = $parts[0]; $graphics = $parts[1]
    $image = Open-Bitmap $source
    try { $graphics.DrawImage($image, 0, 0, 256, 256) } finally { $image.Dispose(); $graphics.Dispose() }
    $relative = 'overview_symbols/fantasy_neutral__v31__overview_symbol__{0:d2}.png' -f ($index + 1)
    Save-Bitmap $bitmap $relative; $bitmap.Dispose()
    Add-ManifestAsset ('fantasy_neutral.v31.overview_symbol.{0:d2}' -f ($index + 1)) 'overview_symbols' 'map_symbol' $semantic $relative 256 256 @('world','region') @($source) 'map_symbol' 0.20 3.0 1 1
}

if ($resultAssets.Count -ne 60) { throw "Expected 60 V3.1 assets, generated $($resultAssets.Count)." }
$hashInput = ($resultAssets | ForEach-Object {
    $fileHash = (Get-FileHash -LiteralPath (Join-Path $outputRootPath $_.file) -Algorithm SHA256).Hash.ToLowerInvariant()
    $_.id + ':' + $_.file + ':' + $fileHash
}) -join "`n"
$hashInput += "`n" + (($baseSemanticOverrides | ForEach-Object { $_.id + ':' + $_.semantic_role }) -join "`n")
$sha = [System.Security.Cryptography.SHA256]::Create()
try { $hash = ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($hashInput)))).Replace('-', '').ToLowerInvariant() }
finally { $sha.Dispose() }
$snapshotId = 'fantasy_neutral_v3_1_' + $hash.Substring(0, 16)

$manifest = [ordered]@{
    schema_version = 1; style_id = 'fantasy_neutral'; style_version = 3; atlas_version = '3.1'
    snapshot_id = $snapshotId; asset_count = 60; accepted_count = 60
    base_snapshot_id = 'fantasy_neutral_v3_15650456ddb04b04'
    build_method = 'deterministic scale-specific composites from the 424 owner-approved V3 assets'
    base_asset_overrides = $baseSemanticOverrides
    assets = $resultAssets
}
$manifest | ConvertTo-Json -Depth 12 | Set-Content (Join-Path $outputRootPath 'pack_manifest.json') -Encoding utf8
$snapshot = [ordered]@{
    schema_version = 1; state = 'production'; style_id = 'fantasy_neutral'; style_version = 3; atlas_version = '3.1'
    snapshot_id = $snapshotId; base_snapshot_id = 'fantasy_neutral_v3_15650456ddb04b04'
    asset_count = 60; manifest_sha256 = $hash
    assets = @($resultAssets | ForEach-Object { [ordered]@{ id = $_.id; file = $_.file; status = 'accepted' } })
}
$snapshot | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $outputRootPath 'production_style_snapshot.json') -Encoding utf8
Write-Host "Generated 60 scale-specific assets in $outputRootPath"
Write-Host "Snapshot: $snapshotId"
