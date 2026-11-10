param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$SourceImage = "",
    [string]$OutputDirectory = "",
    [string]$WorkOrderId = "fn_v2_mountain_range_rocky_roles",
    [string]$Owner = "mountain_range_rocky",
    [int]$TileSize = 128
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")
Add-Type -AssemblyName System.Drawing

$resolvedSourceRoot = Resolve-TilesetPath $SourceRoot
if (-not $SourceImage) {
    $SourceImage = Join-Path $resolvedSourceRoot "ai_source/mountain_ranges_prototype_001_raw.png"
}
$resolvedSourceImage = Resolve-TilesetPath $SourceImage
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $resolvedSourceRoot "review/contextual_candidates"
}
$resolvedOutputDirectory = Resolve-TilesetPath $OutputDirectory
New-Item -ItemType Directory -Force -Path $resolvedOutputDirectory | Out-Null

$mountainDirectory = Join-Path $resolvedSourceRoot "multitile/$Owner"
New-Item -ItemType Directory -Force -Path $mountainDirectory | Out-Null

function Write-CleanJsonFile([string]$Path, $Value) {
    $json = $Value | ConvertTo-Json -Depth 24
    if ($json -is [array]) {
        $json = $json -join "`n"
    }
    $json = $json -replace "`r`n", "`n"
    $json = $json -replace "`r", "`n"
    $json = (($json -split "`n") | ForEach-Object { $_.TrimEnd() }) -join "`n"
    [System.IO.File]::WriteAllText($Path, $json.TrimEnd() + "`n", [System.Text.UTF8Encoding]::new($false))
}

function Format-VariantToken([int]$Variant) {
    return "v$($Variant.ToString("000"))"
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

function Test-CheckerBackgroundPixel([Drawing.Color]$Color) {
    if ($Color.A -lt 16) {
        return $true
    }
    $max = [Math]::Max($Color.R, [Math]::Max($Color.G, $Color.B))
    $min = [Math]::Min($Color.R, [Math]::Min($Color.G, $Color.B))
    return ($Color.R -ge 218 -and $Color.G -ge 218 -and $Color.B -ge 218 -and ($max - $min) -le 28)
}

function Remove-BorderConnectedChecker([Drawing.Bitmap]$Bitmap) {
    $width = $Bitmap.Width
    $height = $Bitmap.Height
    $seen = [bool[]]::new($width * $height)
    $queue = [System.Collections.Generic.Queue[int]]::new()

    function Add-Seed([int]$X, [int]$Y) {
        if ($X -lt 0 -or $Y -lt 0 -or $X -ge $width -or $Y -ge $height) {
            return
        }
        $index = ($Y * $width) + $X
        if ($seen[$index]) {
            return
        }
        if (-not (Test-CheckerBackgroundPixel $Bitmap.GetPixel($X, $Y))) {
            return
        }
        $seen[$index] = $true
        $queue.Enqueue($index)
    }

    for ($x = 0; $x -lt $width; ++$x) {
        Add-Seed $x 0
        Add-Seed $x ($height - 1)
    }
    for ($y = 0; $y -lt $height; ++$y) {
        Add-Seed 0 $y
        Add-Seed ($width - 1) $y
    }

    while ($queue.Count -gt 0) {
        $index = $queue.Dequeue()
        $x = $index % $width
        $y = [int][Math]::Floor($index / $width)
        $Bitmap.SetPixel($x, $y, [Drawing.Color]::Transparent)

        foreach ($offset in @(
            @(-1, 0),
            @(1, 0),
            @(0, -1),
            @(0, 1)
        )) {
            $nx = $x + [int]$offset[0]
            $ny = $y + [int]$offset[1]
            if ($nx -lt 0 -or $ny -lt 0 -or $nx -ge $width -or $ny -ge $height) {
                continue
            }
            $nIndex = ($ny * $width) + $nx
            if ($seen[$nIndex]) {
                continue
            }
            if (Test-CheckerBackgroundPixel $Bitmap.GetPixel($nx, $ny)) {
                $seen[$nIndex] = $true
                $queue.Enqueue($nIndex)
            }
        }
    }
}

function Get-OpaqueBounds([Drawing.Bitmap]$Bitmap) {
    $minX = $Bitmap.Width
    $minY = $Bitmap.Height
    $maxX = -1
    $maxY = -1
    for ($y = 0; $y -lt $Bitmap.Height; ++$y) {
        for ($x = 0; $x -lt $Bitmap.Width; ++$x) {
            if ($Bitmap.GetPixel($x, $y).A -lt 16) {
                continue
            }
            $minX = [Math]::Min($minX, $x)
            $minY = [Math]::Min($minY, $y)
            $maxX = [Math]::Max($maxX, $x)
            $maxY = [Math]::Max($maxY, $y)
        }
    }
    if ($maxX -lt $minX -or $maxY -lt $minY) {
        return [Drawing.Rectangle]::new(0, 0, $Bitmap.Width, $Bitmap.Height)
    }
    return [Drawing.Rectangle]::new($minX, $minY, ($maxX - $minX) + 1, ($maxY - $minY) + 1)
}

function Select-LargestOpaqueComponent([Drawing.Bitmap]$Bitmap) {
    $width = $Bitmap.Width
    $height = $Bitmap.Height
    $visited = [bool[]]::new($width * $height)
    $largest = [System.Collections.Generic.List[int]]::new()
    $queue = [System.Collections.Generic.Queue[int]]::new()

    for ($startY = 0; $startY -lt $height; ++$startY) {
        for ($startX = 0; $startX -lt $width; ++$startX) {
            $startIndex = ($startY * $width) + $startX
            if ($visited[$startIndex]) {
                continue
            }
            $visited[$startIndex] = $true
            if ($Bitmap.GetPixel($startX, $startY).A -lt 16) {
                continue
            }

            $component = [System.Collections.Generic.List[int]]::new()
            $queue.Enqueue($startIndex)
            while ($queue.Count -gt 0) {
                $index = $queue.Dequeue()
                $component.Add($index)
                $x = $index % $width
                $y = [int][Math]::Floor($index / $width)
                for ($dy = -1; $dy -le 1; ++$dy) {
                    for ($dx = -1; $dx -le 1; ++$dx) {
                        if ($dx -eq 0 -and $dy -eq 0) {
                            continue
                        }
                        $nx = $x + $dx
                        $ny = $y + $dy
                        if ($nx -lt 0 -or $ny -lt 0 -or $nx -ge $width -or $ny -ge $height) {
                            continue
                        }
                        $nIndex = ($ny * $width) + $nx
                        if ($visited[$nIndex]) {
                            continue
                        }
                        $visited[$nIndex] = $true
                        if ($Bitmap.GetPixel($nx, $ny).A -ge 16) {
                            $queue.Enqueue($nIndex)
                        }
                    }
                }
            }

            if ($component.Count -gt $largest.Count) {
                $largest = $component
            }
        }
    }

    $keep = [bool[]]::new($width * $height)
    foreach ($index in $largest) {
        $keep[$index] = $true
    }
    for ($y = 0; $y -lt $height; ++$y) {
        for ($x = 0; $x -lt $width; ++$x) {
            $index = ($y * $width) + $x
            if (-not $keep[$index]) {
                $Bitmap.SetPixel($x, $y, [Drawing.Color]::Transparent)
            }
        }
    }
}

function ConvertTo-NormalizedMountainTile(
    [Drawing.Bitmap]$Source,
    [Drawing.Rectangle]$Crop,
    [int]$Size,
    [string]$OutputPath
) {
    $cropBitmap = [Drawing.Bitmap]::new($Crop.Width, $Crop.Height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $cropGraphics = [Drawing.Graphics]::FromImage($cropBitmap)
    try {
        $cropGraphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
        $cropGraphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $cropGraphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $cropGraphics.Clear([Drawing.Color]::Transparent)
        $cropGraphics.DrawImage($Source, [Drawing.Rectangle]::new(0, 0, $Crop.Width, $Crop.Height), $Crop, [Drawing.GraphicsUnit]::Pixel)
    }
    finally {
        $cropGraphics.Dispose()
    }

    try {
        Remove-BorderConnectedChecker $cropBitmap
        Select-LargestOpaqueComponent $cropBitmap
        $bounds = Get-OpaqueBounds $cropBitmap
        $trimmed = [Drawing.Bitmap]::new($bounds.Width, $bounds.Height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $trimGraphics = [Drawing.Graphics]::FromImage($trimmed)
        try {
            $trimGraphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
            $trimGraphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $trimGraphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $trimGraphics.Clear([Drawing.Color]::Transparent)
            $trimGraphics.DrawImage($cropBitmap, [Drawing.Rectangle]::new(0, 0, $bounds.Width, $bounds.Height), $bounds, [Drawing.GraphicsUnit]::Pixel)
        }
        finally {
            $trimGraphics.Dispose()
        }

        try {
            $output = [Drawing.Bitmap]::new($Size, $Size, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
            $graphics = [Drawing.Graphics]::FromImage($output)
            try {
                $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
                $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                $graphics.Clear([Drawing.Color]::Transparent)

                $scale = [Math]::Min(($Size - 8) / [double]$trimmed.Width, ($Size - 8) / [double]$trimmed.Height)
                $drawWidth = [int][Math]::Round($trimmed.Width * $scale)
                $drawHeight = [int][Math]::Round($trimmed.Height * $scale)
                $x = [int][Math]::Round(($Size - $drawWidth) / 2.0)
                $y = [int][Math]::Round(($Size - $drawHeight) / 2.0)
                $graphics.DrawImage($trimmed, [Drawing.Rectangle]::new($x, $y, $drawWidth, $drawHeight), 0, 0, $trimmed.Width, $trimmed.Height, [Drawing.GraphicsUnit]::Pixel)
                $output.Save($OutputPath, [Drawing.Imaging.ImageFormat]::Png)
            }
            finally {
                $graphics.Dispose()
                $output.Dispose()
            }
        }
        finally {
            $trimmed.Dispose()
        }

        return $bounds
    }
    finally {
        $cropBitmap.Dispose()
    }
}

function Measure-AlphaStats([string]$Path) {
    $bitmap = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $Path).Path)
    try {
        $opaque = 0
        $partial = 0
        $minX = $bitmap.Width
        $minY = $bitmap.Height
        $maxX = -1
        $maxY = -1
        for ($y = 0; $y -lt $bitmap.Height; ++$y) {
            for ($x = 0; $x -lt $bitmap.Width; ++$x) {
                $a = $bitmap.GetPixel($x, $y).A
                if ($a -lt 16) {
                    continue
                }
                if ($a -ge 220) {
                    ++$opaque
                }
                else {
                    ++$partial
                }
                $minX = [Math]::Min($minX, $x)
                $minY = [Math]::Min($minY, $y)
                $maxX = [Math]::Max($maxX, $x)
                $maxY = [Math]::Max($maxY, $y)
            }
        }
        $bounds = if ($maxX -ge $minX -and $maxY -ge $minY) {
            [pscustomobject]@{
                x = $minX
                y = $minY
                width = ($maxX - $minX) + 1
                height = ($maxY - $minY) + 1
            }
        }
        else {
            [pscustomobject]@{ x = 0; y = 0; width = 0; height = 0 }
        }
        return [pscustomobject]@{
            opaque_pixels = $opaque
            partial_alpha_pixels = $partial
            alpha_bounds = $bounds
        }
    }
    finally {
        $bitmap.Dispose()
    }
}

$roleCells = [ordered]@{
    isolated = @(@(0, 0), @(0, 1), @(1, 0))
    end = @(@(0, 4), @(0, 5), @(1, 4))
    spine = @(@(0, 3), @(2, 2), @(2, 3))
    bend = @(@(2, 0), @(2, 3), @(3, 2))
    fork = @(@(2, 4), @(3, 4), @(3, 3))
    knot = @(@(3, 0), @(3, 1), @(3, 5))
    interior = @(@(4, 2), @(4, 3), @(4, 4))
    pass = @(@(4, 1), @(4, 5), @(7, 3))
    foothill = @(@(5, 0), @(5, 3), @(6, 0))
    scree = @(@(5, 5), @(6, 1), @(6, 2))
}

$taxonomyPath = Join-Path $resolvedSourceRoot "taxonomy.json"
$taxonomy = Get-Content -LiteralPath $taxonomyPath -Raw | ConvertFrom-Json
if ($null -eq $taxonomy.PSObject.Properties["assets"]) {
    $taxonomy | Add-Member -MemberType NoteProperty -Name "assets" -Value @()
}
$workOrder = @(ConvertTo-TilesetArray (Get-TilesetProperty $taxonomy "production_work_orders" @()) | Where-Object {
    (Get-TilesetProperty $_ "id" "") -eq $WorkOrderId
}) | Select-Object -First 1
if ($null -eq $workOrder) {
    throw "Work order '$WorkOrderId' was not found in taxonomy.json."
}

$sourceBitmap = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $resolvedSourceImage).Path)
$generatedAssets = @()
try {
    $columns = 6
    $rows = 8
    foreach ($role in $roleCells.Keys) {
        $cells = @($roleCells[$role])
        for ($i = 0; $i -lt $cells.Count; ++$i) {
            $row = [int]$cells[$i][0]
            $column = [int]$cells[$i][1]
            $variant = $i + 1
            $variantToken = Format-VariantToken $variant
            $fileName = "fantasy_neutral__world__multitile__$Owner`__$role`__$variantToken.png"
            $outputPath = Join-Path $mountainDirectory $fileName

            $x0 = [int][Math]::Round(($column * $sourceBitmap.Width) / [double]$columns)
            $x1 = [int][Math]::Round((($column + 1) * $sourceBitmap.Width) / [double]$columns)
            $y0 = [int][Math]::Round(($row * $sourceBitmap.Height) / [double]$rows)
            $y1 = [int][Math]::Round((($row + 1) * $sourceBitmap.Height) / [double]$rows)
            $crop = [Drawing.Rectangle]::new($x0, $y0, $x1 - $x0, $y1 - $y0)
            $sourceBounds = ConvertTo-NormalizedMountainTile $sourceBitmap $crop $TileSize $outputPath
            $alphaStats = Measure-AlphaStats $outputPath

            $generatedAssets += [pscustomobject]@{
                id = "fantasy_neutral.world.multitile.$Owner.$role.$variantToken"
                owner = $Owner
                work_order_id = $WorkOrderId
                scale = "world"
                role = $role
                variant = $variant
                variant_token = $variantToken
                grid_row = $row
                grid_column = $column
                crop = [pscustomobject]@{
                    x = $crop.X
                    y = $crop.Y
                    width = $crop.Width
                    height = $crop.Height
                }
                source_bounds = [pscustomobject]@{
                    x = $sourceBounds.X
                    y = $sourceBounds.Y
                    width = $sourceBounds.Width
                    height = $sourceBounds.Height
                }
                alpha_stats = $alphaStats
                path = $outputPath
                source_path = ConvertTo-TilesetRelativePath $outputPath
            }
        }
    }
}
finally {
    $sourceBitmap.Dispose()
}

$roleSummary = @()
foreach ($role in $roleCells.Keys) {
    $roleAssets = @($generatedAssets | Where-Object role -eq $role)
    $roleSummary += [pscustomobject]@{
        role = $role
        generated_assets = $roleAssets.Count
        variants = @($roleAssets | Sort-Object variant | ForEach-Object { $_.variant })
        opaque_pixels_min = ($roleAssets.alpha_stats | Measure-Object -Property opaque_pixels -Minimum).Minimum
        opaque_pixels_max = ($roleAssets.alpha_stats | Measure-Object -Property opaque_pixels -Maximum).Maximum
    }
}

$incompleteRoles = @($roleSummary | Where-Object generated_assets -ne 3)
if ($incompleteRoles.Count -gt 0) {
    throw "Mountain role generation incomplete for: $(@($incompleteRoles | ForEach-Object role) -join ', ')."
}

$existingIds = @{}
foreach ($asset in (ConvertTo-TilesetArray (Get-TilesetProperty $taxonomy "assets" @()))) {
    $existingIds[(Get-TilesetProperty $asset "id" "")] = $true
}
$newManifestAssets = @()
$requiredContext = Get-TilesetProperty $workOrder "required_context" $null
foreach ($generated in $generatedAssets) {
    $manifestAsset = [pscustomobject]@{
        id = $generated.id
        style = "fantasy_neutral"
        scale = $generated.scale
        asset_class = "multitile"
        owner = $generated.owner
        work_order_id = $generated.work_order_id
        role = $generated.role
        component_role = $generated.role
        variant = $generated.variant
        source_path = $generated.source_path
        dimensions = [pscustomobject]@{
            width = $TileSize
            height = $TileSize
        }
        tile_shape = "square"
        transparent = $true
        edge_contract = "component_role_visual_library_candidate_v001_v003"
        tags = @(
            "component_role_candidate",
            "mountain_range",
            $generated.owner,
            $generated.role,
            "ai_source_crop",
            "candidate_only"
        )
        context = [pscustomobject]@{
            component_family = Get-TilesetProperty $requiredContext "component_family" "mountain_range"
            modifier = Get-TilesetProperty $requiredContext "modifier" "rocky"
            role = $generated.role
            source_sheet = ConvertTo-TilesetRelativePath $resolvedSourceImage
            source_grid = [pscustomobject]@{
                columns = 6
                rows = 8
                row = $generated.grid_row
                column = $generated.grid_column
            }
            background_removal = "border_connected_checker_flood_fill"
        }
        status = "candidate"
        validation = [pscustomobject]@{
            human_review = "source_sheet_crop_contact_review_generated_candidate_only"
            notes = "Generated from the rocky mountain source sheet as candidate-only component-role art. Acceptance requires human/art-direction review for multitile role semantics and biome drift."
        }
    }
    if ($existingIds.ContainsKey($generated.id)) {
        $taxonomy.assets = @($taxonomy.assets | ForEach-Object {
            if ((Get-TilesetProperty $_ "id" "") -eq $generated.id) { $manifestAsset } else { $_ }
        })
    }
    else {
        $newManifestAssets += $manifestAsset
    }
}
if ($newManifestAssets.Count -gt 0) {
    $taxonomy.assets = @($taxonomy.assets) + $newManifestAssets
}
Write-CleanJsonFile $taxonomyPath $taxonomy

$contactCell = 76
$labelWidth = 120
$rowHeight = $contactCell + 24
$contactPath = Join-Path $resolvedOutputDirectory "mountain_range_rocky_roles_v001_v003_contact_sheet.png"
$contact = New-TilesetReviewBitmap ($labelWidth + (3 * $contactCell) + 24) (34 + ($roleCells.Keys.Count * $rowHeight) + 12)
try {
    Draw-Label $contact.Graphics "rocky mountain role candidates v001-v003" 8 8 420
    $rowIndex = 0
    foreach ($role in $roleCells.Keys) {
        $y = 34 + ($rowIndex * $rowHeight)
        Draw-Label $contact.Graphics $role 8 ($y + 24) 102
        for ($variant = 1; $variant -le 3; ++$variant) {
            $asset = $generatedAssets | Where-Object { $_.role -eq $role -and $_.variant -eq $variant } | Select-Object -First 1
            $x = $labelWidth + (($variant - 1) * $contactCell)
            $dest = [Drawing.Rectangle]::new($x, $y, $contactCell, $contactCell)
            $bitmap = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $asset.path).Path)
            try {
                $contact.Graphics.DrawImage($bitmap, $dest, 0, 0, $bitmap.Width, $bitmap.Height, [Drawing.GraphicsUnit]::Pixel)
            }
            finally {
                $bitmap.Dispose()
            }
            Draw-Label $contact.Graphics (Format-VariantToken $variant) $x ($y + $contactCell) $contactCell
        }
        ++$rowIndex
    }
    $contact.Bitmap.Save($contactPath, [Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $contact.Graphics.Dispose()
    $contact.Bitmap.Dispose()
}

$sequencePath = Join-Path $resolvedOutputDirectory "mountain_range_rocky_roles_v001_v003_sequence_review.png"
$sequence = New-TilesetReviewBitmap 860 410
try {
    Draw-Label $sequence.Graphics "rocky mountain role sequence preview: role adjacency is illustrative only" 8 8 560
    $layout = @(
        @{ Role = "foothill"; Variant = 1; X = 34; Y = 246 },
        @{ Role = "end"; Variant = 1; X = 130; Y = 174 },
        @{ Role = "spine"; Variant = 1; X = 226; Y = 118 },
        @{ Role = "fork"; Variant = 1; X = 322; Y = 98 },
        @{ Role = "spine"; Variant = 2; X = 418; Y = 120 },
        @{ Role = "bend"; Variant = 1; X = 514; Y = 162 },
        @{ Role = "end"; Variant = 2; X = 610; Y = 226 },
        @{ Role = "scree"; Variant = 1; X = 704; Y = 268 },
        @{ Role = "pass"; Variant = 1; X = 306; Y = 224 },
        @{ Role = "interior"; Variant = 1; X = 408; Y = 214 },
        @{ Role = "knot"; Variant = 1; X = 516; Y = 250 }
    )
    foreach ($item in $layout) {
        $asset = $generatedAssets | Where-Object { $_.role -eq $item.Role -and $_.variant -eq $item.Variant } | Select-Object -First 1
        $dest = [Drawing.Rectangle]::new([int]$item.X, [int]$item.Y, 96, 96)
        $bitmap = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $asset.path).Path)
        try {
            $sequence.Graphics.DrawImage($bitmap, $dest, 0, 0, $bitmap.Width, $bitmap.Height, [Drawing.GraphicsUnit]::Pixel)
        }
        finally {
            $bitmap.Dispose()
        }
    }
    Draw-Label $sequence.Graphics "candidate-only: use as source coverage, not accepted runtime mountain rules" 8 378 540
    $sequence.Bitmap.Save($sequencePath, [Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $sequence.Graphics.Dispose()
    $sequence.Bitmap.Dispose()
}

$roleReportPath = Resolve-TilesetPath "build/mountain_range_rocky_roles_v001_v003_role_report.json"
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $roleReportPath) | Out-Null
Write-CleanJsonFile $roleReportPath ([pscustomobject]@{
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    work_order_id = $WorkOrderId
    source_sheet = ConvertTo-TilesetRelativePath $resolvedSourceImage
    summary = [pscustomobject]@{
        generated_assets = $generatedAssets.Count
        roles = $roleCells.Keys.Count
        incomplete_roles = $incompleteRoles.Count
    }
    role_summary = $roleSummary
    generated_assets = @($generatedAssets | ForEach-Object {
        [pscustomobject]@{
            id = $_.id
            role = $_.role
            variant = $_.variant
            source_path = $_.source_path
            grid_row = $_.grid_row
            grid_column = $_.grid_column
            crop = $_.crop
            source_bounds = $_.source_bounds
            alpha_stats = $_.alpha_stats
        }
    })
})

$reportPath = Join-Path $resolvedOutputDirectory "mountain_range_rocky_roles_v001_v003_review.json"
Write-CleanJsonFile $reportPath ([pscustomobject]@{
    schema_version = 1
    generated_at_local = (Get-Date).ToString("yyyy-MM-ddTHH:mm:ssK")
    work_order_id = $WorkOrderId
    status = "candidate_only_mountain_range_rocky_roles_v001_v003"
    roles = @($roleCells.Keys)
    variants = @(1..3)
    generated_asset_count = $generatedAssets.Count
    generated_assets = @($generatedAssets | ForEach-Object {
        [pscustomobject]@{
            id = $_.id
            source_path = $_.source_path
            role = $_.role
            variant = $_.variant
            grid_row = $_.grid_row
            grid_column = $_.grid_column
            alpha_bounds = $_.alpha_stats.alpha_bounds
        }
    })
    evidence = [pscustomobject]@{
        contact_sheet = ConvertTo-TilesetRelativePath $contactPath
        sequence_review_sheet = ConvertTo-TilesetRelativePath $sequencePath
        role_report = ConvertTo-TilesetRelativePath $roleReportPath
    }
    open_acceptance_holds = @(
        "candidate_only",
        "human_art_direction_review_required",
        "multitile_role_semantics_review_required_before_acceptance"
    )
})

Write-Host "Generated mountain role candidates:"
Write-Host "  Assets:          $($generatedAssets.Count)"
Write-Host "  Roles complete:  $($roleSummary.Count - $incompleteRoles.Count) / $($roleSummary.Count)"
Write-Host "  Contact sheet:   $contactPath"
Write-Host "  Sequence review: $sequencePath"
Write-Host "  Review:          $reportPath"
Write-Host "  Role report:     $roleReportPath"
