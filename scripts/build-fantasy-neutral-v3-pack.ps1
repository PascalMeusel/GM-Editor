param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v3/source_sheets",
    [string]$OutputRoot = "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3",
    [switch]$CleanOutput
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
function Resolve-ProjectPath([string]$Path) {
    if ([IO.Path]::IsPathRooted($Path)) { return [IO.Path]::GetFullPath($Path) }
    return [IO.Path]::GetFullPath((Join-Path $repositoryRoot $Path))
}

$sourcePath = Resolve-ProjectPath $SourceRoot
$outputPath = Resolve-ProjectPath $OutputRoot
if (-not (Test-Path -LiteralPath $sourcePath -PathType Container)) {
    throw "Fantasy/Neutral v3 source sheet directory does not exist: $sourcePath"
}
if ($CleanOutput -and (Test-Path -LiteralPath $outputPath)) {
    $resolvedOutput = [IO.Path]::GetFullPath($outputPath)
    $resolvedGeneratedRoot = [IO.Path]::GetFullPath((Join-Path $repositoryRoot "Resources/Maps/Tilesets/Generated/FantasyNeutral"))
    if (-not $resolvedOutput.StartsWith($resolvedGeneratedRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean output outside the FantasyNeutral generated directory."
    }
    Remove-Item -LiteralPath $resolvedOutput -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $outputPath | Out-Null

$families = @(
    [pscustomobject]@{ Name="terrain_water"; File="terrain_water_9x6.png"; Columns=9; Rows=6; Count=54; StartIndex=1; CellSize=512; Role="terrain_material"; Transparent=$false; SeamLock=$true },
    [pscustomobject]@{ Name="transitions_details"; File="transitions_details_8x4.png"; Columns=8; Rows=4; Count=32; StartIndex=1; CellSize=256; Role="blend_mask_or_decal"; Transparent=$true; SeamLock=$false },
    [pscustomobject]@{ Name="roads_rivers"; File="roads_rivers_7x6.png"; Columns=7; Rows=6; Count=42; StartIndex=1; CellSize=256; Role="curve_profile"; Transparent=$true; SeamLock=$false },
    [pscustomobject]@{ Name="crossings"; File="crossings_7x4.png"; Columns=7; Rows=4; Count=28; StartIndex=1; CellSize=256; Role="bridge_stamp"; Transparent=$true; SeamLock=$false },
    [pscustomobject]@{ Name="vegetation"; File="vegetation_8x6.png"; Columns=8; Rows=6; Count=48; StartIndex=1; CellSize=256; Role="vegetation_stamp"; Transparent=$true; SeamLock=$false },
    [pscustomobject]@{ Name="landscape"; File="landscape_8x5.png"; Columns=8; Rows=5; Count=40; StartIndex=1; CellSize=256; Role="landscape_stamp"; Transparent=$true; SeamLock=$false },
    [pscustomobject]@{ Name="settlements"; File="settlements_01_rural_4x4.png"; Columns=4; Rows=4; Count=16; StartIndex=1; CellSize=256; Role="settlement_stamp"; Transparent=$true; SeamLock=$false },
    [pscustomobject]@{ Name="settlements"; File="settlements_02_town_4x4.png"; Columns=4; Rows=4; Count=16; StartIndex=17; CellSize=256; Role="settlement_stamp"; Transparent=$true; SeamLock=$false },
    [pscustomobject]@{ Name="settlements"; File="settlements_03_fortification_4x4.png"; Columns=4; Rows=4; Count=15; StartIndex=33; CellSize=256; Role="settlement_stamp"; Transparent=$true; SeamLock=$false },
    [pscustomobject]@{ Name="settlements"; File="settlements_04_waterfront_4x4.png"; Columns=4; Rows=4; Count=13; StartIndex=48; CellSize=256; Role="settlement_stamp"; Transparent=$true; SeamLock=$false },
    [pscustomobject]@{ Name="tactical_outdoor"; File="tactical_outdoor_8x5.png"; Columns=8; Rows=5; Count=40; StartIndex=1; CellSize=256; Role="tactical_prop"; Transparent=$true; SeamLock=$false },
    [pscustomobject]@{ Name="cave_dungeon"; File="cave_dungeon_01_natural_4x4.png"; Columns=4; Rows=4; Count=16; StartIndex=1; CellSize=256; Role="dungeon_stamp"; Transparent=$true; SeamLock=$false },
    [pscustomobject]@{ Name="cave_dungeon"; File="cave_dungeon_02_architecture_4x4.png"; Columns=4; Rows=4; Count=16; StartIndex=17; CellSize=256; Role="dungeon_stamp"; Transparent=$true; SeamLock=$false },
    [pscustomobject]@{ Name="cave_dungeon"; File="cave_dungeon_03_furnishing_4x4.png"; Columns=4; Rows=4; Count=16; StartIndex=33; CellSize=256; Role="dungeon_stamp"; Transparent=$true; SeamLock=$false },
    [pscustomobject]@{ Name="cave_dungeon"; File="cave_dungeon_04_details_4x4.png"; Columns=4; Rows=4; Count=8; StartIndex=49; CellSize=256; Role="dungeon_stamp"; Transparent=$true; SeamLock=$false },
    [pscustomobject]@{ Name="map_symbols"; File="map_symbols_6x4.png"; Columns=6; Rows=4; Count=24; StartIndex=1; CellSize=256; Role="map_symbol"; Transparent=$true; SeamLock=$false }
)

function Blend-Color([Drawing.Color]$A, [Drawing.Color]$B, [double]$Weight) {
    $inverse = 1.0 - $Weight
    return [Drawing.Color]::FromArgb(
        [int]($A.A * $inverse + $B.A * $Weight),
        [int]($A.R * $inverse + $B.R * $Weight),
        [int]($A.G * $inverse + $B.G * $Weight),
        [int]($A.B * $inverse + $B.B * $Weight))
}

function Lock-SeamEdges([Drawing.Bitmap]$Bitmap, [int]$Band = 24) {
    $band = [Math]::Min($Band, [int]($Bitmap.Width / 6))
    for ($distance = 0; $distance -lt $band; ++$distance) {
        $weight = 0.5 * [Math]::Pow(1.0 - $distance / [double]$band, 2.0)
        $leftX = $distance
        $rightX = $Bitmap.Width - 1 - $distance
        for ($y = 0; $y -lt $Bitmap.Height; ++$y) {
            $left = $Bitmap.GetPixel($leftX, $y)
            $right = $Bitmap.GetPixel($rightX, $y)
            $average = Blend-Color $left $right 0.5
            $Bitmap.SetPixel($leftX, $y, (Blend-Color $left $average $weight))
            $Bitmap.SetPixel($rightX, $y, (Blend-Color $right $average $weight))
        }
        $topY = $distance
        $bottomY = $Bitmap.Height - 1 - $distance
        for ($x = 0; $x -lt $Bitmap.Width; ++$x) {
            $top = $Bitmap.GetPixel($x, $topY)
            $bottom = $Bitmap.GetPixel($x, $bottomY)
            $average = Blend-Color $top $bottom 0.5
            $Bitmap.SetPixel($x, $topY, (Blend-Color $top $average $weight))
            $Bitmap.SetPixel($x, $bottomY, (Blend-Color $bottom $average $weight))
        }
    }
}

function Remove-RenderedTransparencyGrid([Drawing.Bitmap]$Bitmap) {
    # Built-in generation sometimes paints a white/gray checkerboard instead
    # of emitting alpha. Remove only bright, near-neutral pixels. Assets that
    # fail the later alpha-coverage bounds remain candidates for human review.
    $rectangle = [Drawing.Rectangle]::new(0, 0, $Bitmap.Width, $Bitmap.Height)
    $data = $Bitmap.LockBits($rectangle, [Drawing.Imaging.ImageLockMode]::ReadWrite, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $byteCount = [Math]::Abs($data.Stride) * $Bitmap.Height
        $pixels = [byte[]]::new($byteCount)
        [Runtime.InteropServices.Marshal]::Copy($data.Scan0, $pixels, 0, $byteCount)
        for ($y = 0; $y -lt $Bitmap.Height; ++$y) {
            $rowOffset = $y * [Math]::Abs($data.Stride)
            for ($x = 0; $x -lt $Bitmap.Width; ++$x) {
                $offset = $rowOffset + $x * 4
                $blue = [int]$pixels[$offset]
                $green = [int]$pixels[$offset + 1]
                $red = [int]$pixels[$offset + 2]
                $maximum = [Math]::Max($red, [Math]::Max($green, $blue))
                $minimum = [Math]::Min($red, [Math]::Min($green, $blue))
                if ($red -ge 218 -and $green -ge 218 -and $blue -ge 218 -and ($maximum - $minimum) -le 12) {
                    $pixels[$offset + 3] = 0
                }
            }
        }
        [Runtime.InteropServices.Marshal]::Copy($pixels, 0, $data.Scan0, $byteCount)
    } finally {
        $Bitmap.UnlockBits($data)
    }
}

function Get-AlphaCoverage([Drawing.Bitmap]$Bitmap) {
    $opaque = 0
    $total = 0
    for ($y = 0; $y -lt $Bitmap.Height; $y += 8) {
        for ($x = 0; $x -lt $Bitmap.Width; $x += 8) {
            ++$total
            if ($Bitmap.GetPixel($x, $y).A -gt 16) { ++$opaque }
        }
    }
    if ($total -eq 0) { return 0.0 }
    return [Math]::Round($opaque / [double]$total, 4)
}

function Set-FullyOpaqueAlpha([Drawing.Bitmap]$Bitmap) {
    $rectangle = [Drawing.Rectangle]::new(0, 0, $Bitmap.Width, $Bitmap.Height)
    $data = $Bitmap.LockBits($rectangle, [Drawing.Imaging.ImageLockMode]::ReadWrite, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $byteCount = [Math]::Abs($data.Stride) * $Bitmap.Height
        $pixels = [byte[]]::new($byteCount)
        [Runtime.InteropServices.Marshal]::Copy($data.Scan0, $pixels, 0, $byteCount)
        for ($y = 0; $y -lt $Bitmap.Height; ++$y) {
            $rowOffset = $y * [Math]::Abs($data.Stride)
            for ($x = 0; $x -lt $Bitmap.Width; ++$x) { $pixels[$rowOffset + $x * 4 + 3] = 255 }
        }
        [Runtime.InteropServices.Marshal]::Copy($pixels, 0, $data.Scan0, $byteCount)
    } finally { $Bitmap.UnlockBits($data) }
}

$records = [Collections.Generic.List[object]]::new()
foreach ($family in $families) {
    $familySourcePath = Join-Path $sourcePath $family.File
    if (-not (Test-Path -LiteralPath $familySourcePath -PathType Leaf)) {
        throw "Missing source sheet: $familySourcePath"
    }
    $familyOutput = Join-Path $outputPath $family.Name
    New-Item -ItemType Directory -Force -Path $familyOutput | Out-Null
    $loadedSource = [Drawing.Bitmap]::new($familySourcePath)
    $sourceHadAlpha = $loadedSource.PixelFormat.ToString().Contains("Argb")
    $source = [Drawing.Bitmap]::new($loadedSource.Width, $loadedSource.Height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $sourceGraphics = [Drawing.Graphics]::FromImage($source)
    try {
        $sourceGraphics.CompositingMode = [Drawing.Drawing2D.CompositingMode]::SourceCopy
        $sourceGraphics.DrawImageUnscaled($loadedSource, 0, 0)
    } finally {
        $sourceGraphics.Dispose()
        $loadedSource.Dispose()
    }
    if ($family.Transparent -and -not $sourceHadAlpha) {
        Remove-RenderedTransparencyGrid $source
    }
    try {
        $targetRatio = $family.Columns / [double]$family.Rows
        $sourceRatio = $source.Width / [double]$source.Height
        if ($sourceRatio -gt $targetRatio) {
            $cropHeight = $source.Height
            $cropWidth = [int][Math]::Floor($cropHeight * $targetRatio)
            $cropX = [int](($source.Width - $cropWidth) / 2)
            $cropY = 0
        } else {
            $cropWidth = $source.Width
            $cropHeight = [int][Math]::Floor($cropWidth / $targetRatio)
            $cropX = 0
            $cropY = [int](($source.Height - $cropHeight) / 2)
        }
        $sourceCellWidth = $cropWidth / [double]$family.Columns
        $sourceCellHeight = $cropHeight / [double]$family.Rows
        for ($index = 0; $index -lt $family.Count; ++$index) {
            $column = $index % $family.Columns
            $row = [int][Math]::Floor($index / [double]$family.Columns)
            $sourceRect = [Drawing.RectangleF]::new(
                [single]($cropX + $column * $sourceCellWidth),
                [single]($cropY + $row * $sourceCellHeight),
                [single]$sourceCellWidth,
                [single]$sourceCellHeight)
            $tile = [Drawing.Bitmap]::new($family.CellSize, $family.CellSize, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
            try {
                $graphics = [Drawing.Graphics]::FromImage($tile)
                try {
                    $graphics.Clear([Drawing.Color]::Transparent)
                    $graphics.CompositingMode = [Drawing.Drawing2D.CompositingMode]::SourceCopy
                    $graphics.CompositingQuality = [Drawing.Drawing2D.CompositingQuality]::HighQuality
                    $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                    $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                    $destinationRect = [Drawing.RectangleF]::new(0.0, 0.0, [single]$family.CellSize, [single]$family.CellSize)
                    $graphics.DrawImage($source, $destinationRect, $sourceRect, [Drawing.GraphicsUnit]::Pixel)
                } finally { $graphics.Dispose() }
                if ($family.SeamLock) { Lock-SeamEdges $tile }
                if (-not $family.Transparent) { Set-FullyOpaqueAlpha $tile }
                $variantNumber = $family.StartIndex + $index
                $variant = $variantNumber.ToString("000")
                $fileName = "fantasy_neutral__{0}__v{1}.png" -f $family.Name, $variant
                $assetPath = Join-Path $familyOutput $fileName
                $tile.Save($assetPath, [Drawing.Imaging.ImageFormat]::Png)
                $coverage = Get-AlphaCoverage $tile
                $alphaPass = -not $family.Transparent -or ($coverage -gt 0.02 -and $coverage -lt 0.94)
                $records.Add([ordered]@{
                    id = "fantasy_neutral.v3.$($family.Name).v$variant"
                    family = $family.Name
                    role = $family.Role
                    variant = $variantNumber
                    source_sheet = "source_sheets/$($family.File)"
                    source_cell = [ordered]@{ column=$column; row=$row; columns=$family.Columns; rows=$family.Rows }
                    file = "$($family.Name)/$fileName"
                    dimensions = [ordered]@{ width=$family.CellSize; height=$family.CellSize }
                    transparent = [bool]$family.Transparent
                    alpha_coverage = $coverage
                    exact_dimensions_pass = $true
                    alpha_coverage_pass = $alphaPass
                    seam_lock_applied = [bool]$family.SeamLock
                    perspective = "orthographic_top_down"
                    style = "fantasy_neutral_warm_painterly"
                    tool = "OpenAI built-in image generation + deterministic System.Drawing normalization"
                    license = "project-generated; distribution review required"
                    status = if ($alphaPass) { "candidate" } else { "needs_repaint" }
                })
            } finally { $tile.Dispose() }
        }
    } finally { $source.Dispose() }
}

$manifest = [ordered]@{
    schema_version = 2
    style_id = "fantasy_neutral"
    style_version = 3
    generated_at_utc = [DateTime]::UtcNow.ToString("o")
    asset_count = $records.Count
    accepted_count = @($records | Where-Object status -eq "accepted").Count
    candidate_count = @($records | Where-Object status -eq "candidate").Count
    needs_repaint_count = @($records | Where-Object status -eq "needs_repaint").Count
    runtime_policy = "Only accepted assets may be added to a runtime manifest. Generated files remain review candidates until map-scale, close-up, seam, perspective, footprint, and licensing review pass."
    assets = $records
}
$manifestPath = Join-Path $outputPath "pack_manifest.json"
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding utf8
Write-Host "Generated $($records.Count) exact-size Fantasy/Neutral v3 candidate assets."
Write-Host "Manifest: $manifestPath"
& (Join-Path $PSScriptRoot "enrich-fantasy-neutral-v3-manifest.ps1") -ManifestPath $manifestPath
