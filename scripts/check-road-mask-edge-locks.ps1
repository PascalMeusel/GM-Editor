param(
    [string]$AtlasPath = "Resources/Maps/Tilesets/Generated/FantasyNeutralPrototype/MaskCorrectRoadTiles.png",
    [string]$OutputPath = "build/road-mask-edge-lock-report.json",
    [int]$TileSize = 128,
    [int]$VariantCount = 4,
    [int]$AnchorHalfWidth = 22,
    [int]$AnchorDepth = 8,
    [double]$PresentMinCoverage = 0.65,
    [double]$AbsentMaxCoverage = 0.18,
    [int]$MinProfileWidth = 44,
    [int]$MaxProfileWidth = 78,
    [double]$MaxCenterDrift = 5.0
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")

function Test-RoadMaskPixel([System.Drawing.Color]$Pixel) {
    if ($Pixel.A -lt 24) {
        return $false
    }

    # Road, shoulder, and rut colors in the prototype roadbed are warm browns.
    # Ground is green-biased, so this deliberately catches road material and
    # rejects grass/foliage noise.
    return ($Pixel.R -gt ($Pixel.G + 6)) -and
        ($Pixel.R -gt ($Pixel.B + 22)) -and
        ($Pixel.G -gt ($Pixel.B + 12))
}

function Get-RoadMaskBit([string]$Direction) {
    switch ($Direction) {
        "N" { return 1 }
        "E" { return 2 }
        "S" { return 4 }
        "W" { return 8 }
    }
    throw "Unknown direction '$Direction'."
}

function Get-RoadMaskAnchorStats(
    [System.Drawing.Bitmap]$Bitmap,
    [int]$OriginX,
    [int]$OriginY,
    [string]$Direction,
    [int]$TileSize,
    [int]$HalfWidth,
    [int]$Depth
) {
    $center = [int]($TileSize / 2)
    $windowRoad = 0
    $windowTotal = 0

    $windowX0 = $OriginX
    $windowX1 = $OriginX
    $windowY0 = $OriginY
    $windowY1 = $OriginY
    switch ($Direction) {
        "N" {
            $windowX0 = $OriginX + $center - $HalfWidth
            $windowX1 = $OriginX + $center + $HalfWidth
            $windowY0 = $OriginY
            $windowY1 = $OriginY + $Depth - 1
        }
        "E" {
            $windowX0 = $OriginX + $TileSize - $Depth
            $windowX1 = $OriginX + $TileSize - 1
            $windowY0 = $OriginY + $center - $HalfWidth
            $windowY1 = $OriginY + $center + $HalfWidth
        }
        "S" {
            $windowX0 = $OriginX + $center - $HalfWidth
            $windowX1 = $OriginX + $center + $HalfWidth
            $windowY0 = $OriginY + $TileSize - $Depth
            $windowY1 = $OriginY + $TileSize - 1
        }
        "W" {
            $windowX0 = $OriginX
            $windowX1 = $OriginX + $Depth - 1
            $windowY0 = $OriginY + $center - $HalfWidth
            $windowY1 = $OriginY + $center + $HalfWidth
        }
    }

    for ($y = $windowY0; $y -le $windowY1; ++$y) {
        for ($x = $windowX0; $x -le $windowX1; ++$x) {
            ++$windowTotal
            if (Test-RoadMaskPixel $Bitmap.GetPixel($x, $y)) {
                ++$windowRoad
            }
        }
    }

    $profileThreshold = [Math]::Max(2, [Math]::Ceiling($Depth * 0.35))
    $coveredCoordinates = @()
    for ($tangent = 0; $tangent -lt $TileSize; ++$tangent) {
        $roadSamples = 0
        for ($normal = 0; $normal -lt $Depth; ++$normal) {
            $px = $OriginX
            $py = $OriginY
            switch ($Direction) {
                "N" {
                    $px = $OriginX + $tangent
                    $py = $OriginY + $normal
                }
                "E" {
                    $px = $OriginX + $TileSize - 1 - $normal
                    $py = $OriginY + $tangent
                }
                "S" {
                    $px = $OriginX + $tangent
                    $py = $OriginY + $TileSize - 1 - $normal
                }
                "W" {
                    $px = $OriginX + $normal
                    $py = $OriginY + $tangent
                }
            }
            if (Test-RoadMaskPixel $Bitmap.GetPixel($px, $py)) {
                ++$roadSamples
            }
        }
        if ($roadSamples -ge $profileThreshold) {
            $coveredCoordinates += $tangent
        }
    }

    $profileWidth = 0
    $profileCenter = $null
    $centerDrift = $null
    if ($coveredCoordinates.Count -gt 0) {
        $min = ($coveredCoordinates | Measure-Object -Minimum).Minimum
        $max = ($coveredCoordinates | Measure-Object -Maximum).Maximum
        $profileWidth = [int]($max - $min + 1)
        $profileCenter = ($coveredCoordinates | Measure-Object -Average).Average
        $centerDrift = [Math]::Abs($profileCenter - (($TileSize - 1) / 2.0))
    }

    return [pscustomobject]@{
        coverage = if ($windowTotal -gt 0) { $windowRoad / $windowTotal } else { 0.0 }
        profile_width = $profileWidth
        profile_center = if ($null -eq $profileCenter) { $null } else { [Math]::Round($profileCenter, 2) }
        center_drift = if ($null -eq $centerDrift) { $null } else { [Math]::Round($centerDrift, 2) }
    }
}

$resolvedAtlasPath = Resolve-TilesetPath $AtlasPath
$resolvedOutputPath = Resolve-TilesetPath $OutputPath
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $resolvedOutputPath) | Out-Null

$bitmap = [System.Drawing.Bitmap]::new((Resolve-Path -LiteralPath $resolvedAtlasPath).Path)
$errors = @()
$results = @()
try {
    $expectedWidth = $TileSize * 8
    $expectedHeight = $TileSize * 8
    if ($bitmap.Width -ne $expectedWidth -or $bitmap.Height -ne $expectedHeight) {
        $errors += [pscustomobject]@{
            issue = "road_mask_atlas_dimension_mismatch"
            expected = "${expectedWidth}x${expectedHeight}"
            actual = "$($bitmap.Width)x$($bitmap.Height)"
        }
    }

    for ($mask = 0; $mask -lt 16; ++$mask) {
        for ($variant = 0; $variant -lt $VariantCount; ++$variant) {
            $col = $mask % 8
            $row = [Math]::Floor($mask / 8) * $VariantCount + $variant
            $originX = $col * $TileSize
            $originY = $row * $TileSize

            foreach ($direction in @("N", "E", "S", "W")) {
                $expectedExit = (($mask -band (Get-RoadMaskBit $direction)) -ne 0)
                $stats = Get-RoadMaskAnchorStats $bitmap $originX $originY $direction $TileSize $AnchorHalfWidth $AnchorDepth
                $result = [pscustomobject]@{
                    mask = $mask
                    variant = $variant
                    direction = $direction
                    expected_exit = $expectedExit
                    coverage = [Math]::Round($stats.coverage, 3)
                    profile_width = $stats.profile_width
                    profile_center = $stats.profile_center
                    center_drift = $stats.center_drift
                    status = "ok"
                }

                if ($expectedExit) {
                    if ($stats.coverage -lt $PresentMinCoverage) {
                        $result.status = "fail"
                        $errors += [pscustomobject]@{
                            issue = "way_mask_anchor_missing"
                            mask = $mask
                            variant = $variant
                            direction = $direction
                            coverage = [Math]::Round($stats.coverage, 3)
                            min_coverage = $PresentMinCoverage
                        }
                    }
                    if ($stats.profile_width -lt $MinProfileWidth -or
                        $stats.profile_width -gt $MaxProfileWidth -or
                        $null -eq $stats.center_drift -or
                        $stats.center_drift -gt $MaxCenterDrift) {
                        $result.status = "fail"
                        $errors += [pscustomobject]@{
                            issue = "way_mask_anchor_drift"
                            mask = $mask
                            variant = $variant
                            direction = $direction
                            profile_width = $stats.profile_width
                            min_profile_width = $MinProfileWidth
                            max_profile_width = $MaxProfileWidth
                            center_drift = $stats.center_drift
                            max_center_drift = $MaxCenterDrift
                        }
                    }
                }
                elseif ($stats.coverage -gt $AbsentMaxCoverage) {
                    $result.status = "fail"
                    $errors += [pscustomobject]@{
                        issue = "way_mask_anchor_leak"
                        mask = $mask
                        variant = $variant
                        direction = $direction
                        coverage = [Math]::Round($stats.coverage, 3)
                        max_coverage = $AbsentMaxCoverage
                    }
                }

                $results += $result
            }
        }
    }
}
finally {
    $bitmap.Dispose()
}

$report = [pscustomobject]@{
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    atlas_path = ConvertTo-TilesetRelativePath $resolvedAtlasPath
    layout = [pscustomobject]@{
        tile_size = $TileSize
        mask_count = 16
        variant_count = $VariantCount
        column_formula = "mask % 8"
        row_formula = "(mask / 8) * variant_count + variant"
        bits = [pscustomobject]@{
            N = 1
            E = 2
            S = 4
            W = 8
        }
    }
    thresholds = [pscustomobject]@{
        anchor_half_width = $AnchorHalfWidth
        anchor_depth = $AnchorDepth
        present_min_coverage = $PresentMinCoverage
        absent_max_coverage = $AbsentMaxCoverage
        min_profile_width = $MinProfileWidth
        max_profile_width = $MaxProfileWidth
        max_center_drift = $MaxCenterDrift
    }
    summary = [pscustomobject]@{
        checked_cells = 16 * $VariantCount
        checked_anchors = $results.Count
        error_count = $errors.Count
    }
    errors = $errors
    anchors = $results
}

$report | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $resolvedOutputPath

if ($errors.Count -gt 0) {
    Write-Host "Road mask edge-lock report wrote $resolvedOutputPath"
    Write-Error "Road mask edge-lock validation failed with $($errors.Count) issue(s)."
    exit 1
}

Write-Host "Road mask edge-lock report wrote $resolvedOutputPath"
Write-Host "Road mask edge-lock validation passed: $($results.Count) anchors checked."
