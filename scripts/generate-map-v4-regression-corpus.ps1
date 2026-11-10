param(
    [string]$BuildDirectory = 'build-release',
    [string]$OutputDirectory = 'build/map-feedback/generator-v4-assets-500',
    [int]$MapsPerGroup = 100,
    [int]$ThrottleLimit = 4,
    [long]$SeedBase = 700000,
    [switch]$ReuseGeneratedMaps
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
function Project-Path([string]$Path) {
    if ([IO.Path]::IsPathRooted($Path)) { return [IO.Path]::GetFullPath($Path) }
    return [IO.Path]::GetFullPath((Join-Path $repositoryRoot $Path))
}
$buildPath = Project-Path $BuildDirectory
$generator = @((Join-Path $buildPath 'map_generation_preview_export.exe'), (Join-Path $buildPath 'Release/map_generation_preview_export.exe')) |
    Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
if ([string]::IsNullOrWhiteSpace($generator)) { throw 'Build map_generation_preview_export first.' }
$outputPath = Project-Path $OutputDirectory
New-Item -ItemType Directory -Force -Path $outputPath | Out-Null
$styleSnapshotPath = Project-Path 'Resources/Maps/Tilesets/Generated/FantasyNeutral/v3_3/production_style_snapshot.json'
if (-not (Test-Path -LiteralPath $styleSnapshotPath -PathType Leaf)) {
    throw "Current FantasyNeutral v3_3 production snapshot is missing: $styleSnapshotPath"
}
$styleSnapshot = Get-Content -LiteralPath $styleSnapshotPath -Raw | ConvertFrom-Json
if ([string]$styleSnapshot.state -ne 'production' -or
    [string]$styleSnapshot.atlas_version -ne '3.3' -or
    [string]::IsNullOrWhiteSpace([string]$styleSnapshot.snapshot_id)) {
    throw "FantasyNeutral v3_3 snapshot is not a valid current production snapshot: $styleSnapshotPath"
}
if ($SeedBase -lt 0 -or $SeedBase -gt [uint32]::MaxValue) { throw 'SeedBase must fit in an unsigned 32-bit seed.' }

$groups = @('world','region','city','tactical','dungeon')
$tasks = New-Object System.Collections.Generic.List[object]
for ($groupIndex = 0; $groupIndex -lt $groups.Count; $groupIndex++) {
    $group = $groups[$groupIndex]
    $groupPath = Join-Path $outputPath $group
    New-Item -ItemType Directory -Force -Path $groupPath | Out-Null
    for ($index = 0; $index -lt $MapsPerGroup; $index++) {
        $seedValue = [uint64]$SeedBase + [uint64]$groupIndex * 1000003 + [uint64]$index * 7919
        if ($seedValue -gt [uint32]::MaxValue) { throw "Seed overflow for group $group index $index." }
        $seed = [uint32]$seedValue
        $id = '{0}_{1:d3}' -f $group, ($index + 1)
        $json = Join-Path $groupPath "$id.json"
        $png = Join-Path $groupPath "$id.png"
        $landscapes = @('river_valley','forest_frontier','highlands','mountain_pass','lake_country')
        $arguments = switch ($group) {
            'world' { @('--preset','balanced','--scope','world','--width','64','--height','40','--seed',"$seed",'--water',"$(16 + ($index % 5) * 7)",'--mountains',"$(10 + ($index % 6) * 8)",'--forests',"$(16 + ($index % 7) * 6)",'--requested-settlements',"$(3 + $index % 7)",'--requested-landmarks',"$(2 + $index % 5)",'--landscape',$landscapes[$index % 5]) }
            'region' { @('--preset',$(if ($index % 4 -eq 0) {'travel'} else {'balanced'}),'--scope','local','--width','50','--height','34','--seed',"$seed",'--water',"$(12 + ($index % 5) * 8)",'--mountains',"$(8 + ($index % 6) * 7)",'--forests',"$(18 + ($index % 7) * 6)",'--requested-settlements',"$(2 + $index % 5)",'--requested-landmarks',"$(2 + $index % 5)",'--landscape',$landscapes[$index % 5]) }
            'city' { @('--preset','city','--width','48','--height','34','--seed',"$seed",'--water',"$(($index % 3) * 14)",'--settlements',"$(38 + $index % 18)",'--routes',"$(45 + $index % 24)") }
            'tactical' {
                $templates = @('open_ground','forest_road','river_crossing','ruin','village_street','city_square','cave','stone_dungeon')
                @('--preset','tactical','--tactical-template',$templates[$index % $templates.Count],'--width','42','--height','30','--seed',"$seed",'--tactics',"$(28 + $index % 40)")
            }
            'dungeon' { @('--preset',$(if ($index % 2 -eq 0) {'dungeon'} else {'cave'}),'--scope','dungeon','--width','42','--height','30','--seed',"$seed",'--tactics',"$(30 + $index % 36)") }
        }
        $tasks.Add([pscustomobject]@{ Group=$group; GroupIndex=$groupIndex; LocalIndex=$index; SeedBase=$SeedBase; Id=$id; Seed=$seed; Json=$json; Png=$png; Arguments=@($arguments) })
    }
}

if (-not $ReuseGeneratedMaps) {
    $taskCount = $tasks.Count
    $completed = 0
    $tasks | ForEach-Object -Parallel {
        $task = $_
        $arguments = @()
        for ($argumentIndex = 0; $argumentIndex -lt $task.Arguments.Count; $argumentIndex++) {
            if ($task.Arguments[$argumentIndex] -eq '--seed') { ++$argumentIndex; continue }
            $arguments += $task.Arguments[$argumentIndex]
        }
        $invokeArguments = @($arguments) + @('--qa-seed-base',"$($task.SeedBase)",'--qa-group-index',"$($task.GroupIndex)",'--qa-local-index',"$($task.LocalIndex)",'--out',$task.Json,'--png',$task.Png,'--pixels-per-cell','8','--style-id','fantasy_neutral')
        & $using:generator @invokeArguments *> $null
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $task.Png)) { throw "Generation failed for $($task.Id)." }
    } -ThrottleLimit $ThrottleLimit
} else {
    foreach ($task in $tasks) {
        if (-not (Test-Path -LiteralPath $task.Json) -or -not (Test-Path -LiteralPath $task.Png)) { throw "Missing reusable artifact $($task.Id)." }
    }
}

function Write-ContactSheet([string]$Group, [object[]]$GroupTasks) {
    $cardWidth = 170; $cardHeight = 120; $columns = 10; $rows = [int][Math]::Ceiling($GroupTasks.Count / $columns)
    $sheet = [Drawing.Bitmap]::new($cardWidth * $columns, $cardHeight * $rows, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($sheet)
    $font = [Drawing.Font]::new('Segoe UI', 7)
    try {
        $graphics.Clear([Drawing.Color]::FromArgb(255,24,24,21)); $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        for ($index = 0; $index -lt $GroupTasks.Count; $index++) {
            $task = $GroupTasks[$index]; $x = ($index % $columns) * $cardWidth; $y = [int][Math]::Floor($index / $columns) * $cardHeight
            $image = [Drawing.Image]::FromFile($task.Png)
            try {
                $scale = [Math]::Min(($cardWidth - 6) / $image.Width, ($cardHeight - 20) / $image.Height)
                $width = [int]($image.Width * $scale); $height = [int]($image.Height * $scale)
                $graphics.DrawImage($image, $x + [int](($cardWidth - $width) / 2), $y + 2, $width, $height)
            } finally { $image.Dispose() }
            $graphics.DrawString("$($task.Id) · $($task.Seed)", $font, [Drawing.Brushes]::White, $x + 4, $y + $cardHeight - 16)
        }
        $sheet.Save((Join-Path $outputPath "contact_sheet_${Group}_$($GroupTasks.Count).png"), [Drawing.Imaging.ImageFormat]::Png)
    } finally { $font.Dispose(); $graphics.Dispose(); $sheet.Dispose() }
}

foreach ($group in $groups) { Write-ContactSheet $group @($tasks | Where-Object Group -eq $group) }
$manifest = [ordered]@{
    schema_version = 2; renderer = 'MapPngExport CPU rasterizer'; generator_version = 4
    style_snapshot_id = [string]$styleSnapshot.snapshot_id; map_count = $tasks.Count
    maps_per_group = $MapsPerGroup; groups = $groups
    seed_basis = [ordered]@{ formula = 'base + group_index * 1000003 + local_index * 7919'; base = $SeedBase; matches_release_qa = $true }
    render_contract = 'Shared generator output, immutable production snapshot, scale profile, and deterministic role lookup; CPU PNG and Dear ImGui/OpenGL are separate rasterizers and are not pixel-identical.'
    maps = @($tasks | ForEach-Object { [ordered]@{ id=$_.Id; group=$_.Group; seed=$_.Seed; png=(Resolve-Path $_.Png).Path.Substring($outputPath.Length + 1).Replace('\','/'); json=(Resolve-Path $_.Json).Path.Substring($outputPath.Length + 1).Replace('\','/') } })
}
$manifest | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $outputPath 'corpus_manifest.json') -Encoding utf8
$links = ($groups | ForEach-Object { "<article><h2>$_</h2><a href='contact_sheet_${_}_${MapsPerGroup}.png'><img src='contact_sheet_${_}_${MapsPerGroup}.png'></a></article>" }) -join "`n"
"<!doctype html><html><head><meta charset='utf-8'><title>Karten-Generator V4 - 500 Produktionsrenderings</title><style>body{background:#171714;color:#eee8dc;font:14px Segoe UI;margin:24px}article{margin:0 0 28px}img{max-width:100%;background:#090908;border:1px solid #4a463d}a{color:#e0aa4c}</style></head><body><h1>Karten-Generator V4 - $($tasks.Count) Produktionsrenderings</h1><p>Gemeinsame Generator-, Snapshot-, Maßstabs- und Rollenverträge mit dem Editor, Snapshot $($styleSnapshot.snapshot_id). Die PNGs stammen aus dem separaten CPU-Exporter und sind nicht pixelidentisch mit der Dear-ImGui/OpenGL-Ansicht.</p>$links</body></html>" |
    Set-Content (Join-Path $outputPath 'index.html') -Encoding utf8
Write-Host "Generated $($tasks.Count)-map V4 corpus: $(Join-Path $outputPath 'index.html')"
