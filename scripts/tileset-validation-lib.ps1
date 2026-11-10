$script:TilesetRepoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path

Add-Type -AssemblyName System.Drawing

function Resolve-TilesetPath([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) {
        return $Path
    }
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }
    return (Join-Path $script:TilesetRepoRoot $Path)
}

function ConvertTo-TilesetRelativePath([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) {
        return $Path
    }

    $fullPath = [System.IO.Path]::GetFullPath((Resolve-TilesetPath $Path))
    $rootPath = [System.IO.Path]::GetFullPath($script:TilesetRepoRoot)
    if (-not $rootPath.EndsWith([System.IO.Path]::DirectorySeparatorChar)) {
        $rootPath += [System.IO.Path]::DirectorySeparatorChar
    }

    if ($fullPath.StartsWith($rootPath, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $fullPath.Substring($rootPath.Length).Replace("\", "/")
    }
    return $fullPath.Replace("\", "/")
}

function Read-TilesetJson([string]$Path, $DefaultValue = $null) {
    $resolved = Resolve-TilesetPath $Path
    if (-not (Test-Path -LiteralPath $resolved)) {
        return $DefaultValue
    }

    $text = Get-Content -Raw -LiteralPath $resolved
    if ([string]::IsNullOrWhiteSpace($text)) {
        return $DefaultValue
    }
    return ($text | ConvertFrom-Json)
}

function Get-TilesetProperty($Object, [string]$Name, $DefaultValue = $null) {
    if ($null -eq $Object) {
        return $DefaultValue
    }
    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        return $DefaultValue
    }
    return $property.Value
}

function Format-TilesetDurationMs($Value) {
    $milliseconds = 0L
    if ($null -ne $Value) {
        try {
            $milliseconds = [int64]$Value
        }
        catch {
            $milliseconds = 0L
        }
    }

    if ($milliseconds -ge 1000) {
        return [string]::Format([System.Globalization.CultureInfo]::InvariantCulture, "{0:N1} s", ($milliseconds / 1000.0))
    }
    return "$milliseconds ms"
}

function Format-TilesetSourceQaLine($SourceQa) {
    $sourceQaCoverage = Get-TilesetProperty $SourceQa "contextual_asset_coverage" ([pscustomobject]@{})
    $sourceQaTiming = Get-TilesetProperty $SourceQa "timing" ([pscustomobject]@{})
    $caseLoopElapsedMs = [int64](Get-TilesetProperty $sourceQaTiming "case_loop_elapsed_ms" (Get-TilesetProperty $sourceQaTiming "total_elapsed_ms" 0))
    $preflightElapsedMs = [int64](Get-TilesetProperty $sourceQaTiming "preflight_elapsed_ms" 0)
    $artReadinessWarmupElapsedMs = [int64](Get-TilesetProperty $sourceQaTiming "art_readiness_warmup_elapsed_ms" 0)
    $caseOffset = [int](Get-TilesetProperty $SourceQa "case_offset" 0)
    $caseLimit = Get-TilesetProperty $SourceQa "case_limit" $null
    $availableCaseCount = [int](Get-TilesetProperty $SourceQa "available_case_count" (Get-TilesetProperty $SourceQa "case_count" 0))

    $line = '- Source QA: {0} cases, filter `{1}`, context fixtures `{2}`, art readiness `{3}`, contextual accepted `{4}/{5}`, candidates `{6}`' -f `
        [int](Get-TilesetProperty $SourceQa "case_count" 0),
        (Get-TilesetProperty $SourceQa "case_filter" ""),
        [bool](Get-TilesetProperty $SourceQa "context_fixtures" $false),
        [bool](Get-TilesetProperty $SourceQa "art_readiness" $false),
        [int](Get-TilesetProperty $sourceQaCoverage "accepted_asset_count" 0),
        [int](Get-TilesetProperty $sourceQaCoverage "target_asset_count" 0),
        [int](Get-TilesetProperty $sourceQaCoverage "candidate_asset_count" 0)

    if ($caseOffset -gt 0 -or $null -ne $caseLimit) {
        $caseLimitText = if ($null -ne $caseLimit) { [string]([int]$caseLimit) } else { "all" }
        $line += ', slice offset `{0}`, limit `{1}` from `{2}` available' -f $caseOffset, $caseLimitText, $availableCaseCount
    }

    if ($caseLoopElapsedMs -gt 0 -or $preflightElapsedMs -gt 0 -or $artReadinessWarmupElapsedMs -gt 0) {
        $line += ', timing case loop `{0}`, preflight `{1}`' -f (Format-TilesetDurationMs $caseLoopElapsedMs), (Format-TilesetDurationMs $preflightElapsedMs)
        if ($artReadinessWarmupElapsedMs -gt 0) {
            $line += ' (style warm-up `{0}`)' -f (Format-TilesetDurationMs $artReadinessWarmupElapsedMs)
        }
    }

    return $line
}

function ConvertTo-TilesetArray($Value) {
    if ($null -eq $Value) {
        return @()
    }
    if ($Value -is [System.Collections.IEnumerable] -and $Value -isnot [string] -and $Value -isnot [pscustomobject]) {
        $items = @()
        foreach ($item in $Value) {
            $items += $item
        }
        return $items
    }
    return @($Value)
}

function Get-TilesetContextualDiagnosticKind($Diagnostic) {
    if ($Diagnostic.label -eq "single_road_family_collapse") {
        return "generator_exercise_gap"
    }
    if ($Diagnostic.fallback_reason -eq "repeated_bridge_component_context") {
        return "generator_exercise_gap"
    }
    if ($Diagnostic.fallback_reason -eq "candidate_not_runtime_accepted") {
        return "candidate_not_runtime_accepted"
    }
    if ($Diagnostic.fallback_reason -in @(
            "missing_bridge_crossing_context",
            "missing_bridge_exit_approach_asset",
            "missing_bridge_approach_asset",
            "missing_bridge_bank_context",
            "missing_structure_entrance_context_asset",
            "missing_contextual_asset_slot",
            "wrong_structure_ground_context",
            "missing_waterfront_structure_ground_asset",
            "missing_bridgehead_structure_entrance_asset")) {
        return "context_or_art_gap"
    }
    if ($Diagnostic.fallback_reason -eq "single_road_family_exercise_gap") {
        return "generator_exercise_gap"
    }
    return "unclassified_runtime_warning"
}

function Get-TilesetManifestAssets($Manifest) {
    return ConvertTo-TilesetArray (Get-TilesetProperty $Manifest "assets" @())
}

function Get-TilesetV2ManifestBundle([string]$SourceRoot) {
    $root = Resolve-TilesetPath $SourceRoot
    return [pscustomobject]@{
        SourceRoot = $root
        Tiles = Read-TilesetJson (Join-Path $root "tiles.json") ([pscustomobject]@{ assets = @() })
        Transitions = Read-TilesetJson (Join-Path $root "transition_pairs.json") ([pscustomobject]@{ assets = @(); ordered_pairs = @() })
        Ways = Read-TilesetJson (Join-Path $root "ways.json") ([pscustomobject]@{ assets = @() })
        Taxonomy = Read-TilesetJson (Join-Path $root "taxonomy.json") ([pscustomobject]@{ production_work_orders = @() })
        Validation = Read-TilesetJson (Join-Path $root "validation_results.json") ([pscustomobject]@{ results = @() })
    }
}

function Get-TilesetV2AssetRecords($Bundle) {
    $records = @()
    foreach ($manifestName in @("tiles.json", "transition_pairs.json", "ways.json", "taxonomy.json")) {
        $manifest = switch ($manifestName) {
            "tiles.json" { $Bundle.Tiles }
            "transition_pairs.json" { $Bundle.Transitions }
            "ways.json" { $Bundle.Ways }
            "taxonomy.json" { $Bundle.Taxonomy }
        }
        foreach ($asset in (Get-TilesetManifestAssets $manifest)) {
            $records += [pscustomobject]@{
                Manifest = $manifestName
                Asset = $asset
                Id = Get-TilesetProperty $asset "id" ""
                SourcePath = Get-TilesetProperty $asset "source_path" ""
            }
        }
    }
    return $records
}

function Get-TilesetV2ProductionWorkOrderRecords($Bundle) {
    $records = @()
    foreach ($manifestName in @("tiles.json", "transition_pairs.json", "ways.json", "taxonomy.json")) {
        $manifest = switch ($manifestName) {
            "tiles.json" { $Bundle.Tiles }
            "transition_pairs.json" { $Bundle.Transitions }
            "ways.json" { $Bundle.Ways }
            "taxonomy.json" { $Bundle.Taxonomy }
        }
        foreach ($workOrder in (ConvertTo-TilesetArray (Get-TilesetProperty $manifest "production_work_orders" @()))) {
            $records += [pscustomobject]@{
                Manifest = $manifestName
                WorkOrder = $workOrder
                Id = Get-TilesetProperty $workOrder "id" ""
            }
        }
    }
    return $records
}

function ConvertTo-TilesetBridgeCrossingKey([string]$Value) {
    if ([string]::IsNullOrWhiteSpace($Value)) {
        return ""
    }
    if ($Value.StartsWith("way_", [System.StringComparison]::Ordinal)) {
        return $Value.Substring(4)
    }
    return $Value
}

function Get-TilesetProductionWorkOrderCoverageOwner($WorkOrder) {
    $owner = Get-TilesetProperty $WorkOrder "owner" ""
    $assetClass = Get-TilesetProperty $WorkOrder "asset_class" ""
    $context = Get-TilesetProperty $WorkOrder "required_context" $null

    switch ($assetClass) {
        "integrated_way_ground" {
            $ground = Get-TilesetProperty $context "ground" ""
            if (-not [string]::IsNullOrWhiteSpace($ground)) {
                return ('{0}:{1}' -f $owner, $ground)
            }
        }
        "bridge_sequence" {
            $crossings = @(ConvertTo-TilesetArray (Get-TilesetProperty $context "crossing" @()) | ForEach-Object {
                ConvertTo-TilesetBridgeCrossingKey ([string]$_)
            } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
            if ($crossings.Count -gt 0) {
                return ('{0}:{1}' -f $owner, ($crossings -join "_or_"))
            }
        }
    }

    return $owner
}

function Get-TilesetAssetSourcePath($Asset) {
    $sourcePath = Get-TilesetProperty $Asset "source_path" ""
    if ([string]::IsNullOrWhiteSpace($sourcePath)) {
        return ""
    }
    return Resolve-TilesetPath $sourcePath
}

function ConvertFrom-TilesetAssetFileName([string]$Path) {
    $name = [System.IO.Path]::GetFileNameWithoutExtension($Path)
    $parts = $name -split "__"
    $errors = @()
    $style = ""
    $scale = ""
    $assetClass = ""
    $owner = ""
    $neighbor = ""
    $mask = $null
    $variant = $null

    if ($parts.Count -lt 5) {
        $errors += "Filename must contain at least style, scale, asset class, owner, and variant parts."
    }
    else {
        $style = $parts[0]
        $scale = $parts[1]
        $assetClass = $parts[2]
        $owner = $parts[3]

        if ($style -cnotmatch "^[a-z0-9]+(?:_[a-z0-9]+)*$") {
            $errors += "Style must be lowercase snake case."
        }
        if ($scale -notin @("world", "local", "city", "dungeon", "battle")) {
            $errors += "Scale must be one of world, local, city, dungeon, or battle."
        }
        if ($assetClass -notin @("base", "same_auto", "transition_edge", "transition_corner", "way_segment", "way_transition", "bridge_span", "multitile", "structure", "symbol", "battle_prop", "battle_full_cell", "token_overlay", "transition_review")) {
            $errors += "Unknown asset class '$assetClass'."
        }

        switch ($assetClass) {
            "transition_edge" {
                if ($parts.Count -ne 8) {
                    $errors += "Transition edge filenames must contain owner, to, neighbor, mask, and variant."
                }
                else {
                    if ($parts[4] -ne "to") { $errors += "Transition edge filenames must use __to__ between owner and neighbor." }
                    $neighbor = $parts[5]
                    if ($parts[6] -notmatch "^mask([0-9]{2})$") { $errors += "Terrain transition mask must use mask00 through mask15." }
                    else { $mask = [int]$Matches[1] }
                    if ($parts[7] -notmatch "^v([0-9]{3})$") { $errors += "Variant must use v001 style three-digit numbering." }
                    else { $variant = [int]$Matches[1] }
                }
            }
            "way_transition" {
                if ($parts.Count -ne 8) {
                    $errors += "Way transition filenames must contain owner, to, neighbor, mask, and variant."
                }
                else {
                    if ($parts[4] -ne "to") { $errors += "Way transition filenames must use __to__ between owner and neighbor." }
                    $neighbor = $parts[5]
                    if ($parts[6] -notmatch "^mask([0-9]{3})$") { $errors += "Way masks must use mask000 through mask255." }
                    else { $mask = [int]$Matches[1] }
                    if ($parts[7] -notmatch "^v([0-9]{3})$") { $errors += "Variant must use v001 style three-digit numbering." }
                    else { $variant = [int]$Matches[1] }
                }
            }
            { $_ -in @("same_auto", "way_segment") } {
                if ($parts.Count -ne 6) {
                    $errors += "$assetClass filenames must contain owner, mask, and variant."
                }
                else {
                    $maskPattern = if ($assetClass -eq "way_segment") { "^mask([0-9]{3})$" } else { "^mask([0-9]{2})$" }
                    if ($parts[4] -notmatch $maskPattern) { $errors += "Mask format is invalid for $assetClass." }
                    else { $mask = [int]$Matches[1] }
                    if ($parts[5] -notmatch "^v([0-9]{3})$") { $errors += "Variant must use v001 style three-digit numbering." }
                    else { $variant = [int]$Matches[1] }
                }
            }
            default {
                if ($parts.Count -ne 6) {
                    $errors += "$assetClass filenames must contain owner, detail, and variant."
                }
                else {
                    if ($parts[5] -notmatch "^v([0-9]{3})$") { $errors += "Variant must use v001 style three-digit numbering." }
                    else { $variant = [int]$Matches[1] }
                }
            }
        }
    }

    return [pscustomobject]@{
        IsValid = ($errors.Count -eq 0)
        Errors = $errors
        Id = $name.Replace("__", ".")
        Name = $name
        Style = $style
        Scale = $scale
        AssetClass = $assetClass
        Owner = $owner
        Neighbor = $neighbor
        Mask = $mask
        Variant = $variant
    }
}

function Get-TilesetImageInfo([string]$Path) {
    $resolved = Resolve-TilesetPath $Path
    $bitmap = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $resolved).Path)
    try {
        return [pscustomobject]@{
            Width = $bitmap.Width
            Height = $bitmap.Height
            PixelFormat = $bitmap.PixelFormat.ToString()
            AlphaPixelFormat = [Drawing.Image]::IsAlphaPixelFormat($bitmap.PixelFormat)
        }
    }
    finally {
        $bitmap.Dispose()
    }
}

function Test-TilesetImageHasTransparency([string]$Path, [int]$SampleStep = 1) {
    $resolved = Resolve-TilesetPath $Path
    $bitmap = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $resolved).Path)
    try {
        $step = [Math]::Max(1, $SampleStep)
        for ($y = 0; $y -lt $bitmap.Height; $y += $step) {
            for ($x = 0; $x -lt $bitmap.Width; $x += $step) {
                if ($bitmap.GetPixel($x, $y).A -lt 255) {
                    return $true
                }
            }
        }
        return $false
    }
    finally {
        $bitmap.Dispose()
    }
}

function Get-TilesetStableIndex([int]$X, [int]$Y, [int]$Salt, [int]$Count) {
    if ($Count -le 1) {
        return 0
    }
    $value = ($X * 73856093) -bxor ($Y * 19349663) -bxor ($Salt * 83492791)
    return (($value -band 0x7fffffff) % $Count)
}

function Get-TilesetEdgeAverage([string]$Path, [string]$Edge, [int]$Band = 2) {
    $resolved = Resolve-TilesetPath $Path
    $bitmap = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $resolved).Path)
    try {
        $bandSize = [Math]::Max(1, $Band)
        $sumR = 0.0
        $sumG = 0.0
        $sumB = 0.0
        $sumA = 0.0
        $count = 0
        for ($y = 0; $y -lt $bitmap.Height; ++$y) {
            for ($x = 0; $x -lt $bitmap.Width; ++$x) {
                $include = $false
                switch ($Edge) {
                    "north" { $include = $y -lt $bandSize }
                    "east" { $include = $x -ge ($bitmap.Width - $bandSize) }
                    "south" { $include = $y -ge ($bitmap.Height - $bandSize) }
                    "west" { $include = $x -lt $bandSize }
                }
                if (-not $include) {
                    continue
                }
                $pixel = $bitmap.GetPixel($x, $y)
                if ($pixel.A -lt 16) {
                    continue
                }
                $sumR += $pixel.R
                $sumG += $pixel.G
                $sumB += $pixel.B
                $sumA += $pixel.A
                ++$count
            }
        }
        if ($count -eq 0) {
            return [pscustomobject]@{ R = 0.0; G = 0.0; B = 0.0; A = 0.0; Count = 0 }
        }
        return [pscustomobject]@{
            R = $sumR / $count
            G = $sumG / $count
            B = $sumB / $count
            A = $sumA / $count
            Count = $count
        }
    }
    finally {
        $bitmap.Dispose()
    }
}

function Get-TilesetColorDistance($Left, $Right) {
    $dr = [double]$Left.R - [double]$Right.R
    $dg = [double]$Left.G - [double]$Right.G
    $db = [double]$Left.B - [double]$Right.B
    return [Math]::Sqrt(($dr * $dr) + ($dg * $dg) + ($db * $db))
}

function Select-TilesetAsset($Assets, [string]$Owner, [string[]]$AssetClasses, $Mask = $null, [int]$X = 0, [int]$Y = 0, [int]$Salt = 0) {
    $pool = @($Assets | Where-Object {
        (Get-TilesetProperty $_ "owner" "") -eq $Owner -and
        (Get-TilesetProperty $_ "asset_class" "") -in $AssetClasses -and
        (Get-TilesetProperty $_ "status" "") -in @("candidate", "accepted")
    })
    if ($null -ne $Mask) {
        $pool = @($pool | Where-Object { [int](Get-TilesetProperty $_ "mask" -1) -eq [int]$Mask })
    }
    if ($pool.Count -eq 0) {
        return $null
    }
    $sorted = @($pool | Sort-Object { Get-TilesetProperty $_ "id" "" })
    return $sorted[(Get-TilesetStableIndex $X $Y $Salt $sorted.Count)]
}

function Select-TilesetTransitionAsset($Assets, [string]$Owner, [string]$Neighbor, [int]$Mask, [int]$X = 0, [int]$Y = 0, [int]$Salt = 0) {
    $pool = @($Assets | Where-Object {
        (Get-TilesetProperty $_ "asset_class" "") -eq "transition_edge" -and
        (Get-TilesetProperty $_ "owner" "") -eq $Owner -and
        (Get-TilesetProperty $_ "neighbor" "") -eq $Neighbor -and
        [int](Get-TilesetProperty $_ "mask" -1) -eq $Mask -and
        (Get-TilesetProperty $_ "status" "") -in @("candidate", "accepted")
    })
    if ($pool.Count -eq 0) {
        return $null
    }
    $sorted = @($pool | Sort-Object { Get-TilesetProperty $_ "id" "" })
    return $sorted[(Get-TilesetStableIndex $X $Y $Salt $sorted.Count)]
}

function Draw-TilesetAsset($Graphics, $Asset, [Drawing.Rectangle]$Destination) {
    if ($null -eq $Asset) {
        return
    }
    $sourcePath = Get-TilesetAssetSourcePath $Asset
    if (-not (Test-Path -LiteralPath $sourcePath)) {
        return
    }
    $bitmap = [Drawing.Bitmap]::new((Resolve-Path -LiteralPath $sourcePath).Path)
    try {
        $Graphics.DrawImage($bitmap, $Destination, 0, 0, $bitmap.Width, $bitmap.Height, [Drawing.GraphicsUnit]::Pixel)
    }
    finally {
        $bitmap.Dispose()
    }
}

function New-TilesetReviewBitmap([int]$Width, [int]$Height, [string]$Background = "#101418") {
    $bitmap = [Drawing.Bitmap]::new($Width, $Height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
    $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.Clear([Drawing.ColorTranslator]::FromHtml($Background))
    return [pscustomobject]@{
        Bitmap = $bitmap
        Graphics = $graphics
    }
}
