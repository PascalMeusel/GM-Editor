param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$MaterialBatchDirectory = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2/generation_batches/base_material_retry_2026_05_28",
    [int[]]$Masks = @(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15),
    [int]$TileSize = 128,
    [int]$BaseEdgeWidth = 42,
    [int]$NoiseRange = 18,
    [int]$Softness = 22,
    [int]$MaxAlpha = 205
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")

if (-not ("TilesetMaterialTransitionOverlay" -as [type])) {
    Add-Type -ReferencedAssemblies "System.Drawing" -TypeDefinition @"
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class TilesetMaterialTransitionOverlay
{
    private static byte Clamp(double value)
    {
        if (value < 0.0) return 0;
        if (value > 255.0) return 255;
        return (byte)Math.Round(value);
    }

    private static double SmoothStep(double value)
    {
        double clamped = Math.Max(0.0, Math.Min(1.0, value));
        return clamped * clamped * (3.0 - (2.0 * clamped));
    }

    private static double Noise(int x, int y, int seed)
    {
        double value = Math.Sin(((x + (seed * 17.0)) * 12.9898) + ((y - (seed * 31.0)) * 78.233)) * 43758.5453;
        return value - Math.Floor(value);
    }

    private static double LowNoise(int x, int y, int seed)
    {
        double waveA = Math.Sin(((x + (seed * 0.013)) * 0.085) + (Math.Sin((y + seed) * 0.031) * 1.7));
        double waveB = Math.Sin(((y - (seed * 0.017)) * 0.073) + (Math.Sin((x - seed) * 0.047) * 1.5));
        double waveC = Math.Sin(((x + y + seed) * 0.039));
        return Math.Max(0.0, Math.Min(1.0, 0.5 + (waveA * 0.22) + (waveB * 0.20) + (waveC * 0.08)));
    }

    public static void Create(string neighborSourcePath, string outputPath, int tileSize, int mask, int seed, int edgeWidth, int noiseRange, int softness, int maxAlpha, int cropX, int cropY, int cropSize)
    {
        using (Bitmap source = new Bitmap(neighborSourcePath))
        using (Bitmap texture = new Bitmap(tileSize, tileSize, PixelFormat.Format32bppArgb))
        using (Graphics graphics = Graphics.FromImage(texture))
        {
            graphics.SmoothingMode = SmoothingMode.HighQuality;
            graphics.InterpolationMode = InterpolationMode.HighQualityBicubic;
            graphics.PixelOffsetMode = PixelOffsetMode.HighQuality;
            graphics.DrawImage(
                source,
                new Rectangle(0, 0, tileSize, tileSize),
                new Rectangle(cropX, cropY, cropSize, cropSize),
                GraphicsUnit.Pixel
            );

            using (Bitmap overlay = new Bitmap(tileSize, tileSize, PixelFormat.Format32bppArgb))
            {
                Rectangle rect = new Rectangle(0, 0, tileSize, tileSize);
                BitmapData textureData = texture.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
                BitmapData overlayData = overlay.LockBits(rect, ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
                int bytes = Math.Abs(textureData.Stride) * tileSize;
                byte[] textureBytes = new byte[bytes];
                byte[] overlayBytes = new byte[bytes];
                Marshal.Copy(textureData.Scan0, textureBytes, 0, bytes);
                texture.UnlockBits(textureData);

                for (int y = 0; y < tileSize; ++y)
                {
                    for (int x = 0; x < tileSize; ++x)
                    {
                        double fine = Noise(x, y, seed);
                        double coarse = LowNoise(x, y, seed);
                        double edgeOffset = ((coarse - 0.5) * noiseRange) + ((fine - 0.5) * (noiseRange * 0.35));
                        double boundary = edgeWidth + edgeOffset;
                        double alpha01 = 0.0;

                        if ((mask & 1) != 0)
                            alpha01 = Math.Max(alpha01, SmoothStep((boundary - y) / softness));
                        if ((mask & 2) != 0)
                            alpha01 = Math.Max(alpha01, SmoothStep((boundary - ((tileSize - 1) - x)) / softness));
                        if ((mask & 4) != 0)
                            alpha01 = Math.Max(alpha01, SmoothStep((boundary - ((tileSize - 1) - y)) / softness));
                        if ((mask & 8) != 0)
                            alpha01 = Math.Max(alpha01, SmoothStep((boundary - x) / softness));

                        int index = (y * overlayData.Stride) + (x * 4);
                        if (alpha01 <= 0.001)
                        {
                            overlayBytes[index + 0] = 0;
                            overlayBytes[index + 1] = 0;
                            overlayBytes[index + 2] = 0;
                            overlayBytes[index + 3] = 0;
                            continue;
                        }

                        overlayBytes[index + 0] = textureBytes[index + 0];
                        overlayBytes[index + 1] = textureBytes[index + 1];
                        overlayBytes[index + 2] = textureBytes[index + 2];
                        overlayBytes[index + 3] = Clamp(maxAlpha * alpha01);
                    }
                }

                Marshal.Copy(overlayBytes, 0, overlayData.Scan0, bytes);
                overlay.UnlockBits(overlayData);
                overlay.Save(outputPath, ImageFormat.Png);
            }
        }
    }
}
"@
}

function Clamp-Channel([double]$Value) {
    return [Math]::Max(0, [Math]::Min(255, [int][Math]::Round($Value)))
}

function SmoothStep([double]$T) {
    $clamped = [Math]::Max(0.0, [Math]::Min(1.0, $T))
    return $clamped * $clamped * (3.0 - (2.0 * $clamped))
}

function Get-DeterministicNumber([string]$Key, [int]$Modulo) {
    $hash = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [Text.Encoding]::UTF8.GetBytes($Key)
        $digest = $hash.ComputeHash($bytes)
        $value = [BitConverter]::ToUInt32($digest, 0)
        return [int]($value % [uint32]$Modulo)
    }
    finally {
        $hash.Dispose()
    }
}

function Get-Noise([int]$X, [int]$Y, [int]$Seed) {
    $value = [Math]::Sin((($X + ($Seed * 17)) * 12.9898) + (($Y - ($Seed * 31)) * 78.233)) * 43758.5453
    return $value - [Math]::Floor($value)
}

function Get-LowNoise([int]$X, [int]$Y, [int]$Seed) {
    $sx = [int][Math]::Floor($X / 11)
    $sy = [int][Math]::Floor($Y / 11)
    return Get-Noise $sx $sy ($Seed + 101)
}

function Get-DirectionsFromMask([int]$Mask) {
    $directions = @()
    if (($Mask -band 1) -ne 0) { $directions += "north" }
    if (($Mask -band 2) -ne 0) { $directions += "east" }
    if (($Mask -band 4) -ne 0) { $directions += "south" }
    if (($Mask -band 8) -ne 0) { $directions += "west" }
    return $directions
}

function Get-SingleDirection([int]$Mask) {
    switch ($Mask) {
        1 { return "north" }
        2 { return "east" }
        4 { return "south" }
        8 { return "west" }
        default { return "" }
    }
}

function New-TextureTile([Drawing.Bitmap]$Source, [string]$Key, [int]$TileSize) {
    $cropSizes = @(224, 256, 288, 320, 352, 384)
    $cropSize = [Math]::Min($cropSizes[(Get-DeterministicNumber "$Key-crop" $cropSizes.Count)], [Math]::Min($Source.Width, $Source.Height))
    $maxX = [Math]::Max(1, $Source.Width - $cropSize)
    $maxY = [Math]::Max(1, $Source.Height - $cropSize)
    $x = Get-DeterministicNumber "$Key-x" $maxX
    $y = Get-DeterministicNumber "$Key-y" $maxY
    $tile = [Drawing.Bitmap]::new($TileSize, $TileSize, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($tile)
    try {
        $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.DrawImage(
            $Source,
            [Drawing.Rectangle]::new(0, 0, $TileSize, $TileSize),
            [Drawing.Rectangle]::new($x, $y, $cropSize, $cropSize),
            [Drawing.GraphicsUnit]::Pixel
        )
    }
    finally {
        $graphics.Dispose()
    }
    return $tile
}

function New-MaterialTransitionEntry([string]$Owner, [string]$Neighbor, [int]$Mask, [int]$Variant, [string]$SourcePath, [string]$OwnerSheet, [string]$NeighborSheet, [int]$EdgeWidth, [int]$MaxAlpha) {
    $maskText = $Mask.ToString("00")
    $variantText = $Variant.ToString("000")
    $entry = [ordered]@{
        id = "fantasy_neutral.world.transition_edge.$Owner.to.$Neighbor.mask$maskText.v$variantText"
        style = "fantasy_neutral"
        scale = "world"
        asset_class = "transition_edge"
        owner = $Owner
        neighbor = $Neighbor
        mask = $Mask
        variant = $Variant
        source_path = $SourcePath
        transparent = $true
        status = "candidate"
        validation = [ordered]@{
            edge_delta_ok = $false
            human_review = "pending"
            notes = "AI-material blend overlay candidate generated from AI-painted terrain source sheets; smooth organic feather mask; pending pairwise and whole-map validation."
        }
        generation = [ordered]@{
            method = "ai_material_source_smooth_transition_overlay"
            owner_source_sheet = $OwnerSheet
            neighbor_source_sheet = $NeighborSheet
            edge_width_px = $EdgeWidth
            max_alpha = $MaxAlpha
            prompt_summary = "Smooth material-blend transition pass using AI-generated terrain sheets as source art, replacing decorative fringe overlays with terrain-specific feathered neighbor material."
            addresses_validation_issues = @(
                "decorative_pink_purple_fringe",
                "hard_square_contact",
                "repeated_transition_stamp",
                "missing_natural_feathering",
                "multi_edge_transition_quality"
            )
        }
    }
    $directions = Get-DirectionsFromMask $Mask
    $singleDirection = Get-SingleDirection $Mask
    if ($singleDirection) {
        $entry.single_direction = $singleDirection
    }
    else {
        $entry.directions = $directions
    }
    return $entry
}

function Get-EdgeWidthForPair([string]$Owner, [string]$Neighbor, [int]$BaseEdgeWidth) {
    if ($Owner -eq "water_shallow" -or $Neighbor -eq "water_shallow") {
        return $BaseEdgeWidth + 6
    }
    if ($Owner -eq "snow" -or $Neighbor -eq "snow") {
        return $BaseEdgeWidth + 2
    }
    return $BaseEdgeWidth
}

$resolvedSourceRoot = Resolve-TilesetPath $SourceRoot
$resolvedBatchDirectory = Resolve-TilesetPath $MaterialBatchDirectory
$transitionPath = Join-Path $resolvedSourceRoot "transition_pairs.json"
$manifest = Read-TilesetJson $transitionPath
$assets = @(Get-TilesetManifestAssets $manifest)
$orderedPairs = ConvertTo-TilesetArray (Get-TilesetProperty $manifest "ordered_pairs" @())

$sourceByFamily = @{
    grass_open = "grass_open_source_ai_2026_05_28.png"
    dirt = "dirt_source_ai_2026_05_28.png"
    snow = "snow_source_ai_2026_05_28.png"
    water_shallow = "water_shallow_source_ai_2026_05_28.png"
}
$bitmapByFamily = @{}
foreach ($family in $sourceByFamily.Keys) {
    $path = Join-Path $resolvedBatchDirectory $sourceByFamily[$family]
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing material source sheet for $family`: $path"
    }
    $bitmapByFamily[$family] = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $path).Path)
}

$existingIds = @{}
foreach ($asset in $assets) {
    $existingIds[(Get-TilesetProperty $asset "id" "")] = $true
}

$added = @()
try {
    foreach ($pair in $orderedPairs) {
        $owner = Get-TilesetProperty $pair "owner" ""
        $neighbor = Get-TilesetProperty $pair "neighbor" ""
        if (-not $owner -or -not $neighbor) {
            continue
        }
        if (-not $bitmapByFamily.ContainsKey($owner) -or -not $bitmapByFamily.ContainsKey($neighbor)) {
            continue
        }

        foreach ($mask in $Masks) {
            $existingForMask = @($assets | Where-Object {
                (Get-TilesetProperty $_ "asset_class" "") -eq "transition_edge" -and
                (Get-TilesetProperty $_ "owner" "") -eq $owner -and
                (Get-TilesetProperty $_ "neighbor" "") -eq $neighbor -and
                [int](Get-TilesetProperty $_ "mask" 0) -eq $mask
            })
            $nextVariant = 1
            if ($existingForMask.Count -gt 0) {
                $nextVariant = (($existingForMask | ForEach-Object { [int](Get-TilesetProperty $_ "variant" 0) } | Measure-Object -Maximum).Maximum) + 1
            }
            $maskText = $mask.ToString("00")
            $variantText = $nextVariant.ToString("000")
            $id = "fantasy_neutral.world.transition_edge.$owner.to.$neighbor.mask$maskText.v$variantText"
            if ($existingIds.ContainsKey($id)) {
                continue
            }

            $ownerSheetPath = Join-Path $resolvedBatchDirectory $sourceByFamily[$owner]
            $neighborSheetPath = Join-Path $resolvedBatchDirectory $sourceByFamily[$neighbor]
            $ownerSheet = ConvertTo-TilesetRelativePath $ownerSheetPath
            $neighborSheet = ConvertTo-TilesetRelativePath $neighborSheetPath
            $seed = Get-DeterministicNumber "$owner-$neighbor-mask$maskText-v$variantText-seed" 2147483000
            $edgeWidth = Get-EdgeWidthForPair $owner $neighbor $BaseEdgeWidth
            $cropSizes = @(224, 256, 288, 320, 352, 384)
            $cropSize = [Math]::Min($cropSizes[(Get-DeterministicNumber "$owner-$neighbor-mask$maskText-v$variantText-neighbor-crop" $cropSizes.Count)], [Math]::Min($bitmapByFamily[$neighbor].Width, $bitmapByFamily[$neighbor].Height))
            $maxX = [Math]::Max(1, $bitmapByFamily[$neighbor].Width - $cropSize)
            $maxY = [Math]::Max(1, $bitmapByFamily[$neighbor].Height - $cropSize)
            $cropX = Get-DeterministicNumber "$owner-$neighbor-mask$maskText-v$variantText-neighbor-x" $maxX
            $cropY = Get-DeterministicNumber "$owner-$neighbor-mask$maskText-v$variantText-neighbor-y" $maxY

            $outputDirectory = Join-Path $resolvedSourceRoot "transition_edge/$owner/to/$neighbor"
            New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
            $fileName = "fantasy_neutral__world__transition_edge__$owner`__to__$neighbor`__mask$maskText`__v$variantText.png"
            $outputPath = Join-Path $outputDirectory $fileName
            [TilesetMaterialTransitionOverlay]::Create(
                $neighborSheetPath,
                $outputPath,
                $TileSize,
                $mask,
                $seed,
                $edgeWidth,
                $NoiseRange,
                $Softness,
                $MaxAlpha,
                $cropX,
                $cropY,
                $cropSize
            )

            $relativeOutputPath = ConvertTo-TilesetRelativePath $outputPath
            $entry = New-MaterialTransitionEntry $owner $neighbor $mask $nextVariant $relativeOutputPath $ownerSheet $neighborSheet $edgeWidth $MaxAlpha
            $assets += [pscustomobject]$entry
            $existingIds[$id] = $true
            $added += $id
        }
    }
}
finally {
    foreach ($bitmap in $bitmapByFamily.Values) {
        $bitmap.Dispose()
    }
}

$manifest.assets = $assets
$manifest | ConvertTo-Json -Depth 14 | Set-Content -LiteralPath $transitionPath

$report = [pscustomobject]@{
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    source_root = ConvertTo-TilesetRelativePath $resolvedSourceRoot
    material_batch_directory = ConvertTo-TilesetRelativePath $resolvedBatchDirectory
    mask_count = $Masks.Count
    added_count = $added.Count
    added_ids = $added
}
$reportPath = Join-Path $resolvedBatchDirectory "material_transition_overlay_append_report.json"
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $reportPath

Write-Host "Generated $($added.Count) material transition overlay candidate(s)."
Write-Host "Report: $reportPath"
