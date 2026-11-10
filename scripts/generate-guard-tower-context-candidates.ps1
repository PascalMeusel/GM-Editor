param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$SourceImage = "",
    [string]$OutputDirectory = "",
    [string]$WorkOrderId = "fn_v2_guard_tower_ground_entrance_matrix",
    [string]$Owner = "guard_tower",
    [int]$TileSize = 128
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")
Add-Type -AssemblyName System.Drawing

$resolvedSourceRoot = Resolve-TilesetPath $SourceRoot
if (-not $SourceImage) {
    $SourceImage = Join-Path $resolvedSourceRoot "ai_source/guard_tower_contexts_prototype_001_raw.png"
}
$resolvedSourceImage = Resolve-TilesetPath $SourceImage
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $resolvedSourceRoot "review/contextual_candidates"
}
$resolvedOutputDirectory = Resolve-TilesetPath $OutputDirectory
New-Item -ItemType Directory -Force -Path $resolvedOutputDirectory | Out-Null

$structureDirectory = Join-Path $resolvedSourceRoot "structure/$Owner"
New-Item -ItemType Directory -Force -Path $structureDirectory | Out-Null

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

function ConvertTo-NormalizedStructureTile(
    [Drawing.Bitmap]$Source,
    [Drawing.Rectangle]$Crop,
    [int]$Size,
    [string]$OutputPath,
    [int]$TopTrimPixels = 0
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
        if ($TopTrimPixels -gt 0) {
            $clearHeight = [Math]::Min($TopTrimPixels, $cropBitmap.Height)
            for ($y = 0; $y -lt $clearHeight; ++$y) {
                for ($x = 0; $x -lt $cropBitmap.Width; ++$x) {
                    $cropBitmap.SetPixel($x, $y, [Drawing.Color]::Transparent)
                }
            }
        }
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

                $scale = [Math]::Min(($Size - 6) / [double]$trimmed.Width, ($Size - 6) / [double]$trimmed.Height)
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
            }
        }
        $bounds = Get-OpaqueBounds $bitmap
        return [pscustomobject]@{
            opaque_pixels = $opaque
            partial_alpha_pixels = $partial
            alpha_bounds = [pscustomobject]@{
                x = $bounds.X
                y = $bounds.Y
                width = $bounds.Width
                height = $bounds.Height
            }
        }
    }
    finally {
        $bitmap.Dispose()
    }
}

$candidateSpecs = @(
    [pscustomobject]@{ Role = "ground_only"; Variant = 1; Row = 0; Column = 0; Ground = "grass_open"; EntranceType = "none"; Orientation = "none" },
    [pscustomobject]@{ Role = "ground_only"; Variant = 2; Row = 1; Column = 0; Ground = "stone_paved"; EntranceType = "none"; Orientation = "none" },
    [pscustomobject]@{ Role = "entrance_n"; Variant = 1; Row = 0; Column = 2; Ground = "grass_open"; EntranceType = "way_dirt_road"; Orientation = "n" },
    [pscustomobject]@{ Role = "entrance_n"; Variant = 2; Row = 1; Column = 3; Ground = "stone_paved"; EntranceType = "way_stone_road"; Orientation = "n" },
    [pscustomobject]@{ Role = "entrance_e"; Variant = 1; Row = 0; Column = 4; Ground = "dirt"; EntranceType = "way_dirt_road"; Orientation = "e" },
    [pscustomobject]@{ Role = "entrance_e"; Variant = 2; Row = 6; Column = 4; Ground = "grass_open"; EntranceType = "way_dirt_road"; Orientation = "e" },
    [pscustomobject]@{ Role = "entrance_s"; Variant = 1; Row = 0; Column = 1; Ground = "dirt"; EntranceType = "way_dirt_road"; Orientation = "s" },
    [pscustomobject]@{ Role = "entrance_s"; Variant = 2; Row = 6; Column = 1; Ground = "grass_open"; EntranceType = "way_dirt_road"; Orientation = "s" },
    [pscustomobject]@{ Role = "entrance_w"; Variant = 1; Row = 0; Column = 5; Ground = "grass_open"; EntranceType = "way_dirt_road"; Orientation = "w" },
    [pscustomobject]@{ Role = "entrance_w"; Variant = 2; Row = 7; Column = 4; Ground = "stone_paved"; EntranceType = "way_stone_road"; Orientation = "w" },
    [pscustomobject]@{ Role = "entrance_dirt_n"; Variant = 1; SourceRole = "entrance_s"; SourceVariant = 1; Rotation = 180; Ground = "dirt"; EntranceType = "way_dirt_road"; Orientation = "n" },
    [pscustomobject]@{ Role = "entrance_dirt_n"; Variant = 2; SourceRole = "entrance_e"; SourceVariant = 1; Rotation = 270; Ground = "dirt"; EntranceType = "way_dirt_road"; Orientation = "n" },
    [pscustomobject]@{ Role = "entrance_dirt_w"; Variant = 1; SourceRole = "entrance_e"; SourceVariant = 1; Rotation = 180; Ground = "dirt"; EntranceType = "way_dirt_road"; Orientation = "w" },
    [pscustomobject]@{ Role = "entrance_dirt_w"; Variant = 2; SourceRole = "entrance_s"; SourceVariant = 1; Rotation = 90; Ground = "dirt"; EntranceType = "way_dirt_road"; Orientation = "w" },
    [pscustomobject]@{ Role = "entrance_stone_e"; Variant = 1; SourceRole = "entrance_w"; SourceVariant = 2; Rotation = 180; Ground = "stone_paved"; EntranceType = "way_stone_road"; Orientation = "e" },
    [pscustomobject]@{ Role = "entrance_stone_e"; Variant = 2; SourceRole = "entrance_n"; SourceVariant = 2; Rotation = 90; Ground = "stone_paved"; EntranceType = "way_stone_road"; Orientation = "e" },
    [pscustomobject]@{ Role = "entrance_stone_s"; Variant = 1; SourceRole = "entrance_n"; SourceVariant = 2; Rotation = 180; Ground = "stone_paved"; EntranceType = "way_stone_road"; Orientation = "s" },
    [pscustomobject]@{ Role = "entrance_stone_s"; Variant = 2; SourceRole = "entrance_w"; SourceVariant = 2; Rotation = 270; Ground = "stone_paved"; EntranceType = "way_stone_road"; Orientation = "s" },
    [pscustomobject]@{ Role = "bridgehead"; Variant = 1; Row = 0; Column = 6; Ground = "grass_open"; EntranceType = "way_wood_bridge"; Orientation = "s" },
    [pscustomobject]@{ Role = "bridgehead"; Variant = 2; Row = 7; Column = 6; Ground = "stone_paved"; EntranceType = "way_wood_bridge"; Orientation = "s" },
    [pscustomobject]@{ Role = "cave_threshold"; Variant = 1; Row = 2; Column = 0; Ground = "cave_floor"; EntranceType = "cave_tunnel"; Orientation = "s" },
    [pscustomobject]@{ Role = "cave_threshold"; Variant = 2; Row = 2; Column = 3; Ground = "cave_floor"; EntranceType = "cave_tunnel"; Orientation = "n" },
    [pscustomobject]@{ Role = "dungeon_threshold"; Variant = 1; Row = 7; Column = 0; Ground = "dungeon_floor_stone"; EntranceType = "dungeon_corridor"; Orientation = "s" },
    [pscustomobject]@{ Role = "dungeon_threshold"; Variant = 2; Row = 7; Column = 5; Ground = "dungeon_floor_stone"; EntranceType = "dungeon_corridor"; Orientation = "n" }
)

function Get-RotateFlipType([int]$Rotation) {
    switch ($Rotation) {
        90 { return [Drawing.RotateFlipType]::Rotate90FlipNone }
        180 { return [Drawing.RotateFlipType]::Rotate180FlipNone }
        270 { return [Drawing.RotateFlipType]::Rotate270FlipNone }
    }
    return [Drawing.RotateFlipType]::RotateNoneFlipNone
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
    $columns = 7
    $rows = 8
    foreach ($spec in $candidateSpecs) {
        $variantToken = Format-VariantToken $spec.Variant
        $fileName = "fantasy_neutral__local__structure__$Owner`__$($spec.Role)`__$variantToken.png"
        $outputPath = Join-Path $structureDirectory $fileName

        $sourceRole = [string](Get-TilesetProperty $spec "SourceRole" "")
        $derivedFrom = ""
        if (-not [string]::IsNullOrWhiteSpace($sourceRole)) {
            $sourceVariantToken = Format-VariantToken ([int](Get-TilesetProperty $spec "SourceVariant" 1))
            $sourceFileName = "fantasy_neutral__local__structure__$Owner`__$sourceRole`__$sourceVariantToken.png"
            $sourcePath = Join-Path $structureDirectory $sourceFileName
            $derivedFrom = ConvertTo-TilesetRelativePath $sourcePath
            $sourceTile = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $sourcePath).Path)
            try {
                $sourceTile.RotateFlip((Get-RotateFlipType ([int](Get-TilesetProperty $spec "Rotation" 0))))
                $sourceTile.Save($outputPath, [Drawing.Imaging.ImageFormat]::Png)
            }
            finally {
                $sourceTile.Dispose()
            }
            $crop = [Drawing.Rectangle]::new(0, 0, $TileSize, $TileSize)
            $sourceBounds = [Drawing.Rectangle]::new(0, 0, $TileSize, $TileSize)
        }
        else {
            $x0 = [int][Math]::Round(($spec.Column * $sourceBitmap.Width) / [double]$columns)
            $x1 = [int][Math]::Round((($spec.Column + 1) * $sourceBitmap.Width) / [double]$columns)
            $y0 = [int][Math]::Round(($spec.Row * $sourceBitmap.Height) / [double]$rows)
            $y1 = [int][Math]::Round((($spec.Row + 1) * $sourceBitmap.Height) / [double]$rows)
            $crop = [Drawing.Rectangle]::new($x0, $y0, $x1 - $x0, $y1 - $y0)
            $topTrimPixels = if ($spec.Row -eq 0) { 0 } else { 24 }
            $sourceBounds = ConvertTo-NormalizedStructureTile $sourceBitmap $crop $TileSize $outputPath $topTrimPixels
        }
        $alphaStats = Measure-AlphaStats $outputPath

        $generatedAssets += [pscustomobject]@{
            id = "fantasy_neutral.local.structure.$Owner.$($spec.Role).$variantToken"
            owner = $Owner
            work_order_id = $WorkOrderId
            scale = "local"
            role = $spec.Role
            variant = $spec.Variant
            variant_token = $variantToken
            ground = $spec.Ground
            entrance_type = $spec.EntranceType
            orientation = $spec.Orientation
            grid_row = [int](Get-TilesetProperty $spec "Row" -1)
            grid_column = [int](Get-TilesetProperty $spec "Column" -1)
            derived_from = $derivedFrom
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
finally {
    $sourceBitmap.Dispose()
}

$requiredRoles = @(ConvertTo-TilesetArray (Get-TilesetProperty $workOrder "required_masks_or_roles" @()) | ForEach-Object { [string]$_ })
$targetVariants = [int](Get-TilesetProperty $workOrder "target_variants" 2)
$roleSummary = @()
foreach ($role in $requiredRoles) {
    $roleAssets = @($generatedAssets | Where-Object role -eq $role)
    $roleSummary += [pscustomobject]@{
        role = $role
        generated_assets = $roleAssets.Count
        variants = @($roleAssets | Sort-Object variant | ForEach-Object { $_.variant })
        grounds = @($roleAssets | ForEach-Object ground | Sort-Object -Unique)
        entrance_types = @($roleAssets | ForEach-Object entrance_type | Sort-Object -Unique)
        orientations = @($roleAssets | ForEach-Object orientation | Sort-Object -Unique)
    }
}
$incompleteRoles = @($roleSummary | Where-Object generated_assets -ne $targetVariants)
if ($incompleteRoles.Count -gt 0) {
    throw "Guard tower context generation incomplete for: $(@($incompleteRoles | ForEach-Object role) -join ', ')."
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
        asset_class = "structure"
        owner = $generated.owner
        work_order_id = $generated.work_order_id
        role = $generated.role
        structure_role = $generated.role
        variant = $generated.variant
        source_path = $generated.source_path
        dimensions = [pscustomobject]@{
            width = $TileSize
            height = $TileSize
        }
        tile_shape = "square"
        transparent = $true
        edge_contract = "structure_context_visual_library_candidate_v001_v002"
        tags = @(
            "structure_context_candidate",
            "guard_tower",
            $generated.role,
            $generated.ground,
            $generated.entrance_type,
            "ai_source_crop",
            "candidate_only"
        )
        context = [pscustomobject]@{
            structure_family = Get-TilesetProperty $requiredContext "structure_family" "guard_tower"
            ground = $generated.ground
            entrance_type = $generated.entrance_type
            orientation = $generated.orientation
            footprint = "1x1"
            source_sheet = ConvertTo-TilesetRelativePath $resolvedSourceImage
            source_grid = [pscustomobject]@{
                columns = 7
                rows = 8
                row = $generated.grid_row
                column = $generated.grid_column
            }
            background_removal = "border_connected_checker_flood_fill_largest_component"
        }
        status = "candidate"
        validation = [pscustomobject]@{
            human_review = "source_sheet_crop_contact_review_generated_candidate_only"
            notes = "Generated as candidate-only guard-tower structure-context art. Acceptance requires human/art-direction review for ground, entrance, orientation, and bridge/cave/dungeon context fit."
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

$contactCell = 78
$labelWidth = 150
$rowHeight = $contactCell + 26
$contactPath = Join-Path $resolvedOutputDirectory "guard_tower_contexts_v001_v002_contact_sheet.png"
$contact = New-TilesetReviewBitmap ($labelWidth + (2 * $contactCell) + 24) (34 + ($requiredRoles.Count * $rowHeight) + 12)
try {
    Draw-Label $contact.Graphics "guard tower context candidates v001-v002" 8 8 420
    for ($rowIndex = 0; $rowIndex -lt $requiredRoles.Count; ++$rowIndex) {
        $role = $requiredRoles[$rowIndex]
        $y = 34 + ($rowIndex * $rowHeight)
        Draw-Label $contact.Graphics $role 8 ($y + 24) 132
        for ($variant = 1; $variant -le $targetVariants; ++$variant) {
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
            Draw-Label $contact.Graphics "$(Format-VariantToken $variant) $($asset.ground)" $x ($y + $contactCell) 120
        }
    }
    $contact.Bitmap.Save($contactPath, [Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $contact.Graphics.Dispose()
    $contact.Bitmap.Dispose()
}

$matrixPath = Join-Path $resolvedOutputDirectory "guard_tower_contexts_v001_v002_matrix_review.png"
$matrix = New-TilesetReviewBitmap 820 420
try {
    Draw-Label $matrix.Graphics "guard tower matrix preview: representative contexts only" 8 8 500
    $placements = @(
        @{ Role = "ground_only"; Variant = 1; X = 36; Y = 64 },
        @{ Role = "entrance_s"; Variant = 1; X = 160; Y = 64 },
        @{ Role = "entrance_n"; Variant = 2; X = 284; Y = 64 },
        @{ Role = "bridgehead"; Variant = 1; X = 408; Y = 64 },
        @{ Role = "cave_threshold"; Variant = 1; X = 532; Y = 64 },
        @{ Role = "dungeon_threshold"; Variant = 2; X = 656; Y = 64 },
        @{ Role = "ground_only"; Variant = 2; X = 96; Y = 224 },
        @{ Role = "entrance_e"; Variant = 1; X = 220; Y = 224 },
        @{ Role = "entrance_w"; Variant = 2; X = 344; Y = 224 },
        @{ Role = "bridgehead"; Variant = 2; X = 468; Y = 224 },
        @{ Role = "cave_threshold"; Variant = 2; X = 592; Y = 224 }
    )
    foreach ($item in $placements) {
        $asset = $generatedAssets | Where-Object { $_.role -eq $item.Role -and $_.variant -eq $item.Variant } | Select-Object -First 1
        $dest = [Drawing.Rectangle]::new([int]$item.X, [int]$item.Y, 92, 92)
        $bitmap = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $asset.path).Path)
        try {
            $matrix.Graphics.DrawImage($bitmap, $dest, 0, 0, $bitmap.Width, $bitmap.Height, [Drawing.GraphicsUnit]::Pixel)
        }
        finally {
            $bitmap.Dispose()
        }
        Draw-Label $matrix.Graphics $asset.role ([int]$item.X) ([int]$item.Y + 94) 112
    }
    Draw-Label $matrix.Graphics "candidate-only: orientation/entrance fit needs generated-map review before acceptance" 8 388 620
    $matrix.Bitmap.Save($matrixPath, [Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $matrix.Graphics.Dispose()
    $matrix.Bitmap.Dispose()
}

$contextReportPath = Resolve-TilesetPath "build/guard_tower_contexts_v001_v002_context_report.json"
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $contextReportPath) | Out-Null
Write-CleanJsonFile $contextReportPath ([pscustomobject]@{
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    work_order_id = $WorkOrderId
    source_sheet = ConvertTo-TilesetRelativePath $resolvedSourceImage
    summary = [pscustomobject]@{
        generated_assets = $generatedAssets.Count
        roles = $requiredRoles.Count
        incomplete_roles = $incompleteRoles.Count
    }
    role_summary = $roleSummary
    generated_assets = @($generatedAssets | ForEach-Object {
        [pscustomobject]@{
            id = $_.id
            role = $_.role
            variant = $_.variant
            ground = $_.ground
            entrance_type = $_.entrance_type
            orientation = $_.orientation
            source_path = $_.source_path
            grid_row = $_.grid_row
            grid_column = $_.grid_column
            crop = $_.crop
            source_bounds = $_.source_bounds
            alpha_stats = $_.alpha_stats
        }
    })
})

$reportPath = Join-Path $resolvedOutputDirectory "guard_tower_contexts_v001_v002_review.json"
Write-CleanJsonFile $reportPath ([pscustomobject]@{
    schema_version = 1
    generated_at_local = (Get-Date).ToString("yyyy-MM-ddTHH:mm:ssK")
    work_order_id = $WorkOrderId
    status = "candidate_only_guard_tower_contexts_v001_v002"
    roles = $requiredRoles
    variants = @(1..$targetVariants)
    generated_asset_count = $generatedAssets.Count
    generated_assets = @($generatedAssets | ForEach-Object {
        [pscustomobject]@{
            id = $_.id
            source_path = $_.source_path
            role = $_.role
            variant = $_.variant
            ground = $_.ground
            entrance_type = $_.entrance_type
            orientation = $_.orientation
            grid_row = $_.grid_row
            grid_column = $_.grid_column
            alpha_bounds = $_.alpha_stats.alpha_bounds
        }
    })
    evidence = [pscustomobject]@{
        contact_sheet = ConvertTo-TilesetRelativePath $contactPath
        matrix_review_sheet = ConvertTo-TilesetRelativePath $matrixPath
        context_report = ConvertTo-TilesetRelativePath $contextReportPath
    }
    open_acceptance_holds = @(
        "candidate_only",
        "human_art_direction_review_required",
        "structure_ground_entrance_orientation_review_required_before_acceptance"
    )
})

Write-Host "Generated guard tower context candidates:"
Write-Host "  Assets:        $($generatedAssets.Count)"
Write-Host "  Roles complete: $($roleSummary.Count - $incompleteRoles.Count) / $($roleSummary.Count)"
Write-Host "  Contact sheet: $contactPath"
Write-Host "  Matrix review: $matrixPath"
Write-Host "  Review:        $reportPath"
Write-Host "  Context report: $contextReportPath"
