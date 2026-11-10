param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$OutputDirectory = "",
    [string]$WorkOrderId = "fn_v2_way_dirt_road_on_grass_open",
    [string]$GroundOwner = "grass_open",
    [string]$WayOwner = "way_dirt_road",
    [int]$TileSize = 48
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")

$resolvedSourceRoot = Resolve-TilesetPath $SourceRoot
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $resolvedSourceRoot "review/contextual_candidates"
}
$resolvedOutputDirectory = Resolve-TilesetPath $OutputDirectory
New-Item -ItemType Directory -Force -Path $resolvedOutputDirectory | Out-Null

function Write-CleanJsonFile([string]$Path, $Value) {
    $json = $Value | ConvertTo-Json -Depth 14
    if ($json -is [array]) {
        $json = $json -join "`n"
    }
    $json = $json -replace "`r`n", "`n"
    $json = $json -replace "`r", "`n"
    $json = (($json -split "`n") | ForEach-Object { $_.TrimEnd() }) -join "`n"
    [System.IO.File]::WriteAllText($Path, $json.TrimEnd() + "`n", [System.Text.UTF8Encoding]::new($false))
}

function ConvertFrom-MaskToken([string]$MaskToken) {
    if ($MaskToken -match "^mask([0-9]{3})$") {
        return [int]$Matches[1]
    }
    return $null
}

function Format-MaskToken([int]$Mask) {
    return "mask$($Mask.ToString("000"))"
}

function New-BoolGrid([int]$Width, [int]$Height) {
    $grid = @()
    for ($y = 0; $y -lt $Height; ++$y) {
        $row = @()
        for ($x = 0; $x -lt $Width; ++$x) {
            $row += $false
        }
        $grid += ,$row
    }
    return $grid
}

function Set-RoadCell($Grid, [int]$X, [int]$Y) {
    if ($Y -ge 0 -and $Y -lt $Grid.Count -and $X -ge 0 -and $X -lt $Grid[$Y].Count) {
        $Grid[$Y][$X] = $true
    }
}

function Test-RoadCell($Grid, [int]$X, [int]$Y) {
    return $Y -ge 0 -and $Y -lt $Grid.Count -and $X -ge 0 -and $X -lt $Grid[$Y].Count -and $Grid[$Y][$X]
}

function Get-RoadMaskForGrid($Grid, [int]$X, [int]$Y) {
    $mask = 0
    if (Test-RoadCell $Grid $X ($Y - 1)) { $mask = $mask -bor 1 }
    if (Test-RoadCell $Grid ($X + 1) ($Y - 1)) { $mask = $mask -bor 2 }
    if (Test-RoadCell $Grid ($X + 1) $Y) { $mask = $mask -bor 4 }
    if (Test-RoadCell $Grid ($X + 1) ($Y + 1)) { $mask = $mask -bor 8 }
    if (Test-RoadCell $Grid $X ($Y + 1)) { $mask = $mask -bor 16 }
    if (Test-RoadCell $Grid ($X - 1) ($Y + 1)) { $mask = $mask -bor 32 }
    if (Test-RoadCell $Grid ($X - 1) $Y) { $mask = $mask -bor 64 }
    if (Test-RoadCell $Grid ($X - 1) ($Y - 1)) { $mask = $mask -bor 128 }
    return $mask
}

function Add-RoadCells($Grid, [object[]]$Cells) {
    foreach ($cell in $Cells) {
        Set-RoadCell $Grid ([int]$cell[0]) ([int]$cell[1])
    }
}

function Select-GrassAsset([int]$X, [int]$Y) {
    return Select-TilesetAsset $script:tileAssets $GroundOwner @("base") $null $X $Y 811
}

function Select-RoadAsset([int]$Mask, [int]$X, [int]$Y, [int]$Salt = 0) {
    $pool = @($script:roadAssets | Where-Object { [int](Get-TilesetProperty $_ "mask" -1) -eq $Mask } | Sort-Object { Get-TilesetProperty $_ "id" "" })
    if ($pool.Count -eq 0) {
        return $null
    }
    return $pool[(Get-TilesetStableIndex $X $Y (823 + $Salt) $pool.Count)]
}

function Draw-MissingRoadCell($Graphics, [Drawing.Rectangle]$Destination, [int]$Mask) {
    $brush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(150, 120, 24, 42))
    $pen = [Drawing.Pen]::new([Drawing.Color]::FromArgb(230, 255, 212, 92), 2)
    try {
        $Graphics.FillRectangle($brush, $Destination)
        $Graphics.DrawRectangle($pen, $Destination.X, $Destination.Y, $Destination.Width - 1, $Destination.Height - 1)
    }
    finally {
        $brush.Dispose()
        $pen.Dispose()
    }
}

function Draw-Label($Graphics, [string]$Text, [int]$X, [int]$Y, [int]$Width = 260) {
    $font = [Drawing.Font]::new("Segoe UI", 9)
    $brush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(235, 238, 242, 246))
    $backBrush = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(168, 8, 12, 16))
    try {
        $Graphics.FillRectangle($backBrush, $X, $Y, $Width, 18)
        $Graphics.DrawString($Text, $font, $brush, [Drawing.PointF]::new($X + 4, $Y + 2))
    }
    finally {
        $font.Dispose()
        $brush.Dispose()
        $backBrush.Dispose()
    }
}

$bundle = Get-TilesetV2ManifestBundle $resolvedSourceRoot
$script:tileAssets = @(Get-TilesetManifestAssets $bundle.Tiles)
$wayAssets = @(Get-TilesetManifestAssets $bundle.Ways)
$script:roadAssets = @($wayAssets | Where-Object {
    (Get-TilesetProperty $_ "work_order_id" "") -eq $WorkOrderId -and
    (Get-TilesetProperty $_ "asset_class" "") -eq "way_segment" -and
    (Get-TilesetProperty $_ "owner" "") -eq $WayOwner -and
    (Get-TilesetProperty $_ "status" "") -in @("candidate", "accepted")
})

$workOrder = $null
foreach ($candidate in (ConvertTo-TilesetArray (Get-TilesetProperty $bundle.Ways "production_work_orders" @()))) {
    if ((Get-TilesetProperty $candidate "id" "") -eq $WorkOrderId) {
        $workOrder = $candidate
        break
    }
}
if ($null -eq $workOrder) {
    throw "Could not find work order '$WorkOrderId' in ways.json."
}

$requiredMasks = @(
    ConvertTo-TilesetArray (Get-TilesetProperty $workOrder "required_masks_or_roles" @()) |
        ForEach-Object { ConvertFrom-MaskToken ([string]$_) } |
        Where-Object { $null -ne $_ } |
        Sort-Object
)

$variantCounts = @()
foreach ($mask in $requiredMasks) {
    $assetsForMask = @($script:roadAssets | Where-Object { [int](Get-TilesetProperty $_ "mask" -1) -eq $mask })
    $variantCounts += [pscustomobject]@{
        mask = Format-MaskToken $mask
        active_count = $assetsForMask.Count
        asset_ids = @($assetsForMask | Sort-Object { Get-TilesetProperty $_ "id" "" } | ForEach-Object { Get-TilesetProperty $_ "id" "" })
    }
}

$missingRequiredMasks = @($variantCounts | Where-Object { $_.active_count -eq 0 } | ForEach-Object { $_.mask })
$underfilledRequiredMasks = @($variantCounts | Where-Object { $_.active_count -gt 0 -and $_.active_count -lt 4 } | ForEach-Object { $_.mask })
$activeMask255Variants = @(
    $script:roadAssets |
        Where-Object { [int](Get-TilesetProperty $_ "mask" -1) -eq 255 } |
        ForEach-Object { [int](Get-TilesetProperty $_ "variant" 0) } |
        Sort-Object
)
$mask255RepaintActive = ($activeMask255Variants.Count -gt 0 -and ($activeMask255Variants | Measure-Object -Minimum).Minimum -ge 14)
$mask255IssueLabel = if ($mask255RepaintActive) { "mask255_repaint_needs_app_scale_review" } else { "mask255_starburst_still_open" }
$mask255ReviewText = if ($mask255RepaintActive) { "mask255 all-exit repaint variants need app-scale review" } else { "mask255 stress tile variants remain the visible acceptance hold" }

$activeVariants = @($script:roadAssets | ForEach-Object { [int](Get-TilesetProperty $_ "variant" 0) } | Sort-Object -Unique)
$variantLabel = if ($activeVariants.Count -gt 0) {
    "v$($activeVariants[0].ToString("000"))_v$($activeVariants[$activeVariants.Count - 1].ToString("000"))"
}
else {
    "no_active_variants"
}

$contactPath = Join-Path $resolvedOutputDirectory "way_dirt_road_on_grass_open_full_$variantLabel`_contact_sheet.png"
$routePath = Join-Path $resolvedOutputDirectory "way_dirt_road_on_grass_open_full_$variantLabel`_route_review.png"
$jsonPath = Join-Path $resolvedOutputDirectory "way_dirt_road_on_grass_open_full_$variantLabel`_review.json"

$labelWidth = 104
$rowHeight = $TileSize + 22
$maxVariants = [Math]::Max(4, (($variantCounts | ForEach-Object { $_.active_count }) | Measure-Object -Maximum).Maximum)
$contactWidth = $labelWidth + ($maxVariants * $TileSize) + 18
$contactHeight = 30 + ($requiredMasks.Count * $rowHeight) + 10
$contact = New-TilesetReviewBitmap $contactWidth $contactHeight
try {
    Draw-Label $contact.Graphics "full road-on-grass candidate set, $($variantLabel -replace "_", "-")" 8 6 330
    for ($row = 0; $row -lt $requiredMasks.Count; ++$row) {
        $mask = [int]$requiredMasks[$row]
        $y = 30 + ($row * $rowHeight)
        Draw-Label $contact.Graphics "$(Format-MaskToken $mask)" 8 ($y + 14) 84
        $assetsForMask = @($script:roadAssets | Where-Object { [int](Get-TilesetProperty $_ "mask" -1) -eq $mask } | Sort-Object { [int](Get-TilesetProperty $_ "variant" 0) }, { Get-TilesetProperty $_ "id" "" })
        for ($variant = 0; $variant -lt $assetsForMask.Count; ++$variant) {
            $x = $labelWidth + ($variant * $TileSize)
            $dest = [Drawing.Rectangle]::new($x, $y, $TileSize, $TileSize)
            Draw-TilesetAsset $contact.Graphics (Select-GrassAsset $variant $row) $dest
            Draw-TilesetAsset $contact.Graphics $assetsForMask[$variant] $dest
            Draw-Label $contact.Graphics "v$((Get-TilesetProperty $assetsForMask[$variant] "variant" 0).ToString("000"))" $x ($y + $TileSize) $TileSize
        }
    }
    $contact.Bitmap.Save($contactPath, [Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $contact.Graphics.Dispose()
    $contact.Bitmap.Dispose()
}

$routeWidth = 28
$routeHeight = 14
$footerHeight = 112
$roadGrid = New-BoolGrid $routeWidth $routeHeight

Add-RoadCells $roadGrid @(@(1,2), @(2,2), @(3,2), @(4,2), @(5,2), @(6,2), @(7,2), @(8,2), @(9,2))
Add-RoadCells $roadGrid @(@(13,1), @(13,2), @(13,3), @(13,4), @(13,5), @(13,6), @(13,7))
Add-RoadCells $roadGrid @(@(2,6), @(3,7), @(4,8), @(5,9), @(6,10), @(7,11))
Add-RoadCells $roadGrid @(@(25,6), @(24,7), @(23,8), @(22,9), @(21,10), @(20,11))
Add-RoadCells $roadGrid @(@(16,2), @(17,2), @(18,2), @(19,2), @(20,2), @(20,3), @(20,4), @(20,5), @(20,6))
Add-RoadCells $roadGrid @(@(10,9), @(10,10), @(10,11), @(10,12), @(11,12), @(12,12), @(13,12))

$routeReview = New-TilesetReviewBitmap ($routeWidth * $TileSize) (($routeHeight * $TileSize) + $footerHeight)
$routeMaskUsage = @{}
$missingRouteMasks = @{}
try {
    for ($y = 0; $y -lt $routeHeight; ++$y) {
        for ($x = 0; $x -lt $routeWidth; ++$x) {
            $dest = [Drawing.Rectangle]::new($x * $TileSize, $y * $TileSize, $TileSize, $TileSize)
            Draw-TilesetAsset $routeReview.Graphics (Select-GrassAsset $x $y) $dest
        }
    }

    for ($y = 0; $y -lt $routeHeight; ++$y) {
        for ($x = 0; $x -lt $routeWidth; ++$x) {
            if (-not (Test-RoadCell $roadGrid $x $y)) {
                continue
            }
            $mask = Get-RoadMaskForGrid $roadGrid $x $y
            $maskToken = Format-MaskToken $mask
            if (-not $routeMaskUsage.ContainsKey($maskToken)) { $routeMaskUsage[$maskToken] = 0 }
            $routeMaskUsage[$maskToken] += 1

            $dest = [Drawing.Rectangle]::new($x * $TileSize, $y * $TileSize, $TileSize, $TileSize)
            $roadAsset = Select-RoadAsset $mask $x $y
            if ($null -eq $roadAsset) {
                Draw-MissingRoadCell $routeReview.Graphics $dest $mask
                Draw-Label $routeReview.Graphics $maskToken $dest.X ($dest.Y + 14) 92
                if (-not $missingRouteMasks.ContainsKey($maskToken)) { $missingRouteMasks[$maskToken] = 0 }
                $missingRouteMasks[$maskToken] += 1
            }
            else {
                Draw-TilesetAsset $routeReview.Graphics $roadAsset $dest
            }
        }
    }

    Draw-Label $routeReview.Graphics "full-set route review: cardinal, diagonal, corner, and route-extension masks" 8 8 500
    Draw-Label $routeReview.Graphics "horizontal mask068 / vertical mask017" (1 * $TileSize) (3 * $TileSize) 280
    Draw-Label $routeReview.Graphics "diagonal mask136" (2 * $TileSize) (12 * $TileSize) 160
    Draw-Label $routeReview.Graphics "diagonal mask034" (20 * $TileSize) (12 * $TileSize) 160
    Draw-Label $routeReview.Graphics "corner masks 080 and 005/020/065" (15 * $TileSize) (7 * $TileSize) 280

    $footerY = $routeHeight * $TileSize
    Draw-Label $routeReview.Graphics $mask255ReviewText 8 ($footerY + 8) 390
    if ($missingRouteMasks.Count -gt 0) {
        Draw-Label $routeReview.Graphics "missing composite route masks: $(($missingRouteMasks.Keys | Sort-Object) -join ', ')" 384 ($footerY + 8) 520
    }
    $mask255Assets = @($script:roadAssets | Where-Object { [int](Get-TilesetProperty $_ "mask" -1) -eq 255 } | Sort-Object { [int](Get-TilesetProperty $_ "variant" 0) })
    for ($index = 0; $index -lt $mask255Assets.Count; ++$index) {
        $x = 8 + ($index * ($TileSize + 8))
        $dest = [Drawing.Rectangle]::new($x, $footerY + 34, $TileSize, $TileSize)
        Draw-TilesetAsset $routeReview.Graphics (Select-GrassAsset $index 99) $dest
        Draw-TilesetAsset $routeReview.Graphics $mask255Assets[$index] $dest
        Draw-Label $routeReview.Graphics "v$((Get-TilesetProperty $mask255Assets[$index] "variant" 0).ToString("000"))" $x ($footerY + 34 + $TileSize) $TileSize
    }

    $routeReview.Bitmap.Save($routePath, [Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $routeReview.Graphics.Dispose()
    $routeReview.Bitmap.Dispose()
}

$routeMaskUsageRecords = @(
    $routeMaskUsage.Keys |
        Sort-Object |
        ForEach-Object {
            [pscustomobject]@{
                mask = $_
                draw_count = $routeMaskUsage[$_]
            }
        }
)

$acceptanceIssueLabels = @(
    "candidate_only",
    "acceptance_hold",
    "app_scale_contrast_needs_human_acceptance",
    $mask255IssueLabel
)
if ($missingRequiredMasks.Count -gt 0) {
    $acceptanceIssueLabels += "declared_required_masks_missing"
}
if ($underfilledRequiredMasks.Count -gt 0) {
    $acceptanceIssueLabels += "declared_required_masks_underfilled"
}
if ($missingRouteMasks.Count -gt 0) {
    $acceptanceIssueLabels += "route_layout_missing_composite_masks"
}

$acceptanceNotes = @(
    "The full active candidate set ($variantLabel) now has contact-sheet and route-layout review evidence in one repeatable script output.",
    "The route review exercises the straight and diagonal extension masks that were missing from the older v002-v005 route proof.",
    "This script does not waive the app-scale contrast or mask255 all-exit treatment concerns; promotion still requires an art-direction decision."
)
if ($missingRouteMasks.Count -gt 0) {
    $acceptanceNotes += "The generated route layout still produces composite eight-neighbor masks without candidate art: $(($missingRouteMasks.Keys | Sort-Object) -join ', ')."
}
if ($missingRequiredMasks.Count -gt 0) {
    $acceptanceNotes += "The work order now declares required masks without candidate art: $(($missingRequiredMasks | Sort-Object) -join ', ')."
}
if ($underfilledRequiredMasks.Count -gt 0) {
    $acceptanceNotes += "Some declared masks have fewer active variants than the work-order target: $(($underfilledRequiredMasks | Sort-Object) -join ', ')."
}

$report = [pscustomobject]@{
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    source_root = ConvertTo-TilesetRelativePath $resolvedSourceRoot
    work_order_id = $WorkOrderId
    active_variant_label = $variantLabel
    active_variants = $activeVariants
    output_contact_sheet = ConvertTo-TilesetRelativePath $contactPath
    output_route_review = ConvertTo-TilesetRelativePath $routePath
    tile_size = $TileSize
    active_candidate_count = $script:roadAssets.Count
    required_mask_count = $requiredMasks.Count
    required_masks = @($requiredMasks | ForEach-Object { Format-MaskToken ([int]$_) })
    variant_counts = $variantCounts
    missing_required_masks = $missingRequiredMasks
    underfilled_required_masks = $underfilledRequiredMasks
    route_mask_usage = $routeMaskUsageRecords
    missing_route_masks = @($missingRouteMasks.Keys | Sort-Object | ForEach-Object {
        [pscustomobject]@{
            mask = $_
            draw_count = $missingRouteMasks[$_]
        }
    })
    acceptance_review = [pscustomobject]@{
        status = "candidate_hold_for_acceptance"
        accepted = $false
        full_set_review_generated = $true
        declared_required_masks_represented = ($missingRequiredMasks.Count -eq 0 -and $underfilledRequiredMasks.Count -eq 0)
        route_layout_masks_represented = ($missingRouteMasks.Count -eq 0)
        issue_labels = $acceptanceIssueLabels
        notes = $acceptanceNotes
    }
}

Write-CleanJsonFile $jsonPath $report

Write-Host "Generated full road-on-grass review:"
Write-Host "  Contact sheet: $contactPath"
Write-Host "  Route review:  $routePath"
Write-Host "  Report:        $jsonPath"
