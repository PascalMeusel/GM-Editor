param(
    [string]$BuildDirectory = "build-release",
    [string]$OutputDirectory = "build/map-feedback/editor-generator-20",
    [long]$QaSeedBase = -1,
    [switch]$ReuseGeneratedMaps
)
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
function Project-Path([string]$Path) { if ([IO.Path]::IsPathRooted($Path)) { return [IO.Path]::GetFullPath($Path) }; return [IO.Path]::GetFullPath((Join-Path $repositoryRoot $Path)) }
function Assert-GalleryCandidateMetadata($Recipe, $Metadata) {
    if ($null -eq $Metadata.quality -or $null -eq $Metadata.render -or $null -eq $Metadata.production_assets) {
        throw "Gallery recipe $($Recipe.Id) produced incomplete evidence metadata."
    }
    if ([int]$Metadata.quality.score -lt 85 -or
        -not [bool]$Metadata.quality.validation_eligible -or
        -not [bool]$Metadata.quality.candidate_floor_passed -or
        [int]$Metadata.quality.critical_issue_count -ne 0) {
        throw "Gallery recipe $($Recipe.Id) is not an exportable candidate: $($Metadata.quality | ConvertTo-Json -Compress)."
    }
    if ([int]$Metadata.production_assets.unresolved_used_role_count -ne 0) {
        throw "Gallery recipe $($Recipe.Id) referenced unresolved production roles."
    }
    if (-not [bool]$Metadata.render.compiled_plan_consumed_by_png -or
        [string]$Metadata.render.audience -ne 'gm' -or
        [string]$Metadata.render.locale -ne 'de' -or
        [string]$Metadata.render.render_plan_style_snapshot_id -ne [string]$Metadata.style_snapshot_id -or
        [string]$Metadata.render.render_plan_content_hash -notmatch '^[0-9a-f]{16}$' -or
        [bool]$Metadata.render.pixel_identical_to_imgui_editor) {
        throw "Gallery recipe $($Recipe.Id) does not carry a consistent immutable RenderPlan contract."
    }
    $routeOverlayExpected = [bool]$Recipe.AllowRouteOverlay
    if ([bool]$Metadata.render.show_routes -ne $routeOverlayExpected) {
        throw "Gallery recipe $($Recipe.Id) route-overlay evidence does not match its explicit presentation contract."
    }
    if (-not $routeOverlayExpected -and [int]$Metadata.render.visible_route_overlay_count -ne 0) {
        throw "Gallery recipe $($Recipe.Id) leaked a MapRoute overlay into a normal design preview."
    }
    if ($routeOverlayExpected -and [int]$Metadata.render.visible_route_overlay_count -lt 1) {
        throw "Gallery recipe $($Recipe.Id) requested an active route, but no route overlay was rendered."
    }
}
function Get-QaSeedCoordinate([string]$RecipeId) {
    $number = [int]$RecipeId.Substring(0, 2)
    if ($number -le 5) { return [pscustomobject]@{ GroupIndex=0; LocalIndex=($number - 1) } }
    if ($number -le 10) { return [pscustomobject]@{ GroupIndex=1; LocalIndex=($number - 6) } }
    if ($number -le 14) { return [pscustomobject]@{ GroupIndex=2; LocalIndex=($number - 11) } }
    if ($number -le 18) { return [pscustomobject]@{ GroupIndex=3; LocalIndex=($number - 15) } }
    return [pscustomobject]@{ GroupIndex=4; LocalIndex=($number - 19) }
}
$buildPath = Project-Path $BuildDirectory
$generator = @((Join-Path $buildPath "map_generation_preview_export.exe"),(Join-Path $buildPath "Release/map_generation_preview_export.exe")) | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
if ([string]::IsNullOrWhiteSpace($generator)) { throw "Build map_generation_preview_export first." }
$outputPath = Project-Path $OutputDirectory
New-Item -ItemType Directory -Force -Path $outputPath | Out-Null
if ($QaSeedBase -lt -1 -or $QaSeedBase -gt [uint32]::MaxValue) { throw 'QaSeedBase must be -1 or fit in an unsigned 32-bit seed.' }

$recipes = @(
    [pscustomobject]@{ Id="01_world_balanced"; Label="Welt - ausgewogen"; Args=@("--preset","balanced","--scope","world","--width","72","--height","46","--seed","41001","--water","22","--mountains","18","--forests","24","--settlements","8","--routes","22","--landscape","river_valley","--road-network","connected") },
    [pscustomobject]@{ Id="02_world_lakes"; Label="Welt - Seenland"; Args=@("--preset","balanced","--scope","world","--width","72","--height","46","--seed","41002","--water","38","--mountains","10","--forests","28","--settlements","7","--routes","18","--landscape","lake_country") },
    # The specialised mountain/forest presets intentionally produce regional
    # maps. World gallery fixtures stay on the world-capable Balanced preset
    # and express their identity through the recipe densities/profile.
    [pscustomobject]@{ Id="03_world_ranges"; Label="Welt - Gebirgsketten"; Args=@("--preset","balanced","--scope","world","--width","72","--height","46","--seed","41003","--water","14","--mountains","58","--forests","14","--settlements","6","--routes","20","--mountain-pattern","ranges") },
    [pscustomobject]@{ Id="04_world_frontier"; Label="Welt - Waldgrenze"; Args=@("--preset","balanced","--scope","world","--width","72","--height","46","--seed","41006","--water","20","--mountains","13","--forests","50","--settlements","5","--routes","14","--landscape","forest_frontier") },
    # Keep the CLI fixture ASCII-stable on Windows shells. The C++ test feeds
    # the exact localized German UI sentence directly as UTF-8 as well.
    [pscustomobject]@{ Id="05_world_notes"; Label="Welt - aus Notizen"; NoteDriven=$true; Args=@("--preset","balanced","--scope","world","--width","72","--height","46","--seed","41005","--water","18","--mountains","12","--forests","20","--settlements","3","--routes","28","--requested-settlements","4","--requested-landmarks","0","--edge-connections","4","--road-network","connected","--world-note","Three nations named Alderreach, Veyra, and Kharad share a great river. There is a large capital named Crosswater.") },
    [pscustomobject]@{ Id="06_region_river"; Label="Region - Flusstal"; Args=@("--preset","balanced","--scope","local","--width","50","--height","34","--seed","42001","--water","28","--mountains","8","--forests","24","--settlements","8","--routes","26","--landscape","river_valley") },
    [pscustomobject]@{ Id="07_region_forest"; Label="Region - dichter Wald"; Args=@("--preset","forest","--scope","local","--width","50","--height","34","--seed","42002","--water","18","--mountains","5","--forests","58","--settlements","5","--routes","18") },
    [pscustomobject]@{ Id="08_region_pass"; Label="Region - Gebirgspass"; Args=@("--preset","mountain","--scope","local","--width","50","--height","34","--seed","42003","--water","8","--mountains","62","--forests","12","--settlements","4","--routes","24","--landscape","mountain_pass","--mountain-pattern","ranges") },
    [pscustomobject]@{ Id="09_region_lakes"; Label="Region - Seen und Wege"; Args=@("--preset","balanced","--scope","local","--width","50","--height","34","--seed","42004","--water","42","--mountains","7","--forests","34","--settlements","6","--routes","20","--landscape","lake_country") },
    [pscustomobject]@{ Id="10_travel_plains"; Label="Reise - offenes Land"; AllowRouteOverlay=$true; Args=@("--preset","travel","--width","50","--height","34","--seed","42005","--water","10","--mountains","4","--forests","12","--routes","30","--tactics","12","--show-routes") },
    # Settlement V4 deliberately selects its six high-level plans from
    # `(seed / 17) % 6`. Keep these fixtures in different archetype buckets so
    # the visual gate cannot accidentally compare four coast towns while their
    # cards claim river, inland, green, and grid layouts.
    [pscustomobject]@{ Id="11_city_river"; Label="Stadt - am Fluss"; Args=@("--preset","city","--scope","local","--width","48","--height","34","--seed","42985","--water","30","--settlements","48","--routes","55","--road-network","grid") },
    [pscustomobject]@{ Id="12_city_inland"; Label="Stadt - Binnenstadt"; Args=@("--preset","city","--scope","local","--width","48","--height","34","--seed","42950","--water","4","--settlements","54","--routes","62","--road-network","grid") },
    [pscustomobject]@{ Id="13_city_green"; Label="Stadt - gruene Viertel"; Args=@("--preset","city","--scope","local","--width","48","--height","34","--seed","42967","--water","12","--forests","10","--settlements","44","--routes","52","--road-network","grid") },
    [pscustomobject]@{ Id="14_city_crossroads"; Label="Stadt - Strassenkreuz"; Args=@("--preset","city","--scope","local","--width","48","--height","34","--seed","43035","--water","0","--settlements","42","--routes","68","--edge-connections","4","--road-network","grid") },
    [pscustomobject]@{ Id="15_tactical_forest"; Label="Kampf - Waldweg"; Args=@("--preset","tactical","--tactical-template","forest_road","--width","42","--height","30","--seed","44001","--tactics","48") },
    [pscustomobject]@{ Id="16_tactical_crossing"; Label="Kampf - Flussquerung"; Args=@("--preset","tactical","--tactical-template","river_crossing","--width","42","--height","30","--seed","44002","--tactics","52") },
    [pscustomobject]@{ Id="17_tactical_ruins"; Label="Kampf - Ruinengelaende"; Args=@("--preset","tactical","--tactical-template","ruin","--width","42","--height","30","--seed","44003","--tactics","65") },
    # OpenGround does not yet promise readable elevation. Keep the gallery
    # title honest until a dedicated highland tactical template and invariant
    # exist instead of presenting flat terrain as a fulfilled height brief.
    [pscustomobject]@{ Id="18_tactical_highland"; Label="Kampf - offenes Gelaende"; Args=@("--preset","tactical","--tactical-template","open_ground","--width","42","--height","30","--seed","44004","--tactics","55") },
    [pscustomobject]@{ Id="19_dungeon"; Label="Dungeon - Steinraeume"; Args=@("--preset","dungeon","--scope","dungeon","--width","42","--height","30","--seed","45001","--tactics","45") },
    [pscustomobject]@{ Id="20_cave"; Label="Dungeon - Hoehlennetz"; Args=@("--preset","cave","--scope","dungeon","--width","42","--height","30","--seed","45002","--tactics","50") }
)

$resolvedSeeds = @{}
$effectiveArguments = @{}
$metadataById = @{}
$index = 0
foreach ($recipe in $recipes) {
    ++$index
    $jsonPath = Join-Path $outputPath "$($recipe.Id).json"
    $pngPath = Join-Path $outputPath "$($recipe.Id).png"
    $arguments = @($recipe.Args)
    $seedArgumentIndex = [Array]::IndexOf($arguments, "--seed")
    if ($seedArgumentIndex -lt 0) { throw "Recipe $($recipe.Id) has no seed." }
    $requestedSeed = [uint32]$arguments[$seedArgumentIndex + 1]
    if ($QaSeedBase -ge 0) {
        $coordinate = Get-QaSeedCoordinate $recipe.Id
        $seedValue = [uint64]$QaSeedBase + [uint64]$coordinate.GroupIndex * 1000003 + [uint64]$coordinate.LocalIndex * 7919
        if ($seedValue -gt [uint32]::MaxValue) { throw "QA seed overflow for $($recipe.Id)." }
        $requestedSeed = [uint32]$seedValue
        $withoutSeed = @()
        for ($argumentIndex = 0; $argumentIndex -lt $arguments.Count; $argumentIndex++) {
            if ($arguments[$argumentIndex] -eq '--seed') { ++$argumentIndex; continue }
            $withoutSeed += $arguments[$argumentIndex]
        }
        $arguments = @($withoutSeed) + @('--qa-seed-base',"$QaSeedBase",'--qa-group-index',"$($coordinate.GroupIndex)",'--qa-local-index',"$($coordinate.LocalIndex)")
    }
    $resolvedSeeds[$recipe.Id] = $requestedSeed
    $effectiveArguments[$recipe.Id] = @($arguments)
    if (-not $ReuseGeneratedMaps) {
        # Match the editor's V5 "Balanced" contract explicitly. Keeping this
        # visible in the evidence script prevents future CLI defaults from
        # silently changing which maps receive visual review.
        $invokeArguments = @("--generator-version","5","--quality-attempts","24") + @($arguments) + @("--out",$jsonPath,"--png",$pngPath,"--pixels-per-cell","18","--style-id","fantasy_neutral","--locale","de")
        & $generator @invokeArguments
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $pngPath)) { throw "Map feedback generation failed for $($recipe.Id)." }
    } elseif (-not (Test-Path -LiteralPath $jsonPath) -or -not (Test-Path -LiteralPath $pngPath)) {
        throw "Cannot reuse missing generated map artifacts for $($recipe.Id)."
    }
    $metadata = Get-Content -Raw -LiteralPath $jsonPath | ConvertFrom-Json
    if ($null -ne $metadata.requested_seed -and [uint32]$metadata.requested_seed -ne $requestedSeed) {
        throw "Generated metadata seed mismatch for $($recipe.Id): expected $requestedSeed, got $($metadata.requested_seed)."
    }
    if ($recipe.Id -match '_world_' -and [string]$metadata.atlas_level -ne 'world') {
        throw "World gallery recipe $($recipe.Id) produced atlas level '$($metadata.atlas_level)'."
    }
    if ($recipe.Id -match '_world_' -and
        (-not [bool]$metadata.qa_checks.world_scale_no_tactical_vegetation_roles -or
         -not [bool]$metadata.qa_checks.world_scale_no_vegetation_stamp_roles -or
         -not [bool]$metadata.qa_checks.world_scale_no_decorative_landmark_assets -or
         -not [bool]$metadata.qa_checks.world_scale_no_close_range_terrain_roles -or
         -not [bool]$metadata.qa_checks.world_scale_no_curve_profile_stamps)) {
        throw "World gallery recipe $($recipe.Id) referenced a tactical, vegetation, close-range terrain, decorative landmark, or curve-stamp role."
    }
    if ($recipe.NoteDriven) {
        $expectedNations = @('Alderreach','Veyra','Kharad')
        if (-not [bool]$metadata.world_facts.applied -or [int]$metadata.world_facts.stated_nation_count -ne 3 -or
            -not [bool]$metadata.world_facts.requires_shared_great_river) {
            throw "Note-driven world facts were not applied for $($recipe.Id)."
        }
        if ((@($metadata.world_facts.nation_names) -join '|') -ne ($expectedNations -join '|') -or
            [string]$metadata.world_facts.capital_name -ne 'Crosswater') {
            throw "Note-driven world facts lost a named nation or capital for $($recipe.Id)."
        }
        if (@($metadata.world_facts.city_names).Count -ne 0) {
            throw "The sparse note fixture unexpectedly invented parsed city facts for $($recipe.Id)."
        }
        if (-not [bool]$metadata.qa_checks.passed -or
            -not [bool]$metadata.qa_checks.named_nations_present -or
            -not [bool]$metadata.qa_checks.named_capital_present -or
            -not [bool]$metadata.qa_checks.no_generic_feature_names -or
            -not [bool]$metadata.qa_checks.road_network_present -or
            -not [bool]$metadata.qa_checks.river_network_present -or
            -not [bool]$metadata.qa_checks.shared_great_river_valid -or
            -not [bool]$metadata.qa_checks.settlements_on_land -or
            -not [bool]$metadata.qa_checks.road_water_crossings_valid -or
            -not [bool]$metadata.qa_checks.world_scale_no_tactical_vegetation_roles -or
            -not [bool]$metadata.qa_checks.world_scale_no_vegetation_stamp_roles -or
            -not [bool]$metadata.qa_checks.world_scale_no_decorative_landmark_assets -or
            -not [bool]$metadata.qa_checks.world_scale_no_close_range_terrain_roles -or
            -not [bool]$metadata.qa_checks.world_scale_no_curve_profile_stamps) {
            throw "Note-driven world QA failed for $($recipe.Id): $($metadata.qa_checks | ConvertTo-Json -Compress)."
        }
    }
    Assert-GalleryCandidateMetadata $recipe $metadata
    $metadataById[$recipe.Id] = $metadata
}

$cardWidth=480; $cardHeight=340; $columns=4; $rows=5
$sheet=[Drawing.Bitmap]::new($cardWidth*$columns,$cardHeight*$rows,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics=[Drawing.Graphics]::FromImage($sheet)
$titleFont=[Drawing.Font]::new("Segoe UI",14,[Drawing.FontStyle]::Bold)
$metaFont=[Drawing.Font]::new("Segoe UI",9)
try {
    $graphics.Clear([Drawing.Color]::FromArgb(255,24,24,21)); $graphics.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    for($i=0;$i-lt$recipes.Count;$i++){
        $recipe=$recipes[$i];$col=$i%$columns;$row=[int][Math]::Floor($i/$columns);$x=$col*$cardWidth;$y=$row*$cardHeight
        $graphics.FillRectangle([Drawing.Brushes]::Black,$x+8,$y+42,$cardWidth-16,$cardHeight-54)
        $img=[Drawing.Image]::FromFile((Join-Path $outputPath "$($recipe.Id).png"))
        try {$scale=[Math]::Min(($cardWidth-16)/$img.Width,($cardHeight-54)/$img.Height);$w=[int]($img.Width*$scale);$h=[int]($img.Height*$scale);$graphics.DrawImage($img,$x+8+[int](($cardWidth-16-$w)/2),$y+42+[int](($cardHeight-54-$h)/2),$w,$h)} finally {$img.Dispose()}
        $graphics.DrawString(("{0:00}  {1}" -f ($i+1),$recipe.Label),$titleFont,[Drawing.Brushes]::White,$x+10,$y+8)
        $seed=$resolvedSeeds[$recipe.Id];$graphics.DrawString("Seed $seed",$metaFont,[Drawing.Brushes]::LightGray,$x+10,$y+28)
    }
    $sheet.Save((Join-Path $outputPath "contact_sheet_20.png"),[Drawing.Imaging.ImageFormat]::Png)
} finally {$titleFont.Dispose();$metaFont.Dispose();$graphics.Dispose();$sheet.Dispose()}

$items=@($recipes|%{
    $metadata = $metadataById[$_.Id]
    [ordered]@{
        id=$_.Id; label=$_.Label; png="$($_.Id).png"; json="$($_.Id).json"; seed=$resolvedSeeds[$_.Id]
        generator_version=$metadata.generator_version; style_version=$metadata.style_version; style_snapshot_id=$metadata.style_snapshot_id
        graph_hash=$metadata.hashes.graph_hash; raster_hash=$metadata.hashes.raster_hash; quality_score=$metadata.quality.score
        score_purpose='structural_diagnostics'; visual_acceptance='not_reviewed'; cartography_revision=$metadata.cartography_revision
        used_role_count=$metadata.production_assets.used_role_count; unresolved_used_role_count=$metadata.production_assets.unresolved_used_role_count
        validation_eligible=$metadata.quality.validation_eligible; candidate_floor_passed=$metadata.quality.candidate_floor_passed
        route_overlay_visible=$metadata.render.show_routes; visible_route_overlay_count=$metadata.render.visible_route_overlay_count
        world_facts=$metadata.world_facts; qa_checks=$metadata.qa_checks
        arguments=$effectiveArguments[$_.Id]
    }
})
$items|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $outputPath "gallery_manifest.json") -Encoding utf8
$cards=($items|%{"<article><a href='$($_.png)'><img src='$($_.png)' loading='lazy'></a><h2>$($_.label)</h2><p>Seed $($_.seed) · Generator V$($_.generator_version) · Stil V$($_.style_version) · Strukturdiagnostik $($_.quality_score) · Visuell noch nicht abgenommen · Rollen $($_.used_role_count), ungelöst $($_.unresolved_used_role_count) · <a href='$($_.json)'>JSON-Nachweis</a></p></article>"})-join"`n"
$html="<!doctype html><html lang='de'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width'><title>20 Editor-Generatorkarten mit FantasyNeutral V3</title><style>body{margin:0;background:#171714;color:#eee8dc;font:14px Segoe UI,sans-serif}header{position:sticky;z-index:2;top:0;background:#1d1c18f4;padding:18px 24px;border-bottom:1px solid #504b40}h1{margin:0;font-size:22px}main{padding:20px;display:grid;grid-template-columns:repeat(auto-fit,minmax(420px,1fr));gap:16px}article{background:#25241f;border:1px solid #4a463d;border-radius:10px;padding:12px}img{display:block;width:100%;height:360px;object-fit:contain;background:#0e0e0c}h2{font-size:16px;margin:10px 0 4px}p{color:#aaa79f;margin:0}a{color:#e0aa4c}</style></head><body><header><h1>20 Karten mit FantasyNeutral-V3-Produktionsassets</h1><div>Generator und PNG-Export verwenden denselben MapModel-Vertrag, denselben unveränderlichen Produktionssnapshot, dieselben Maßstabsprofile und dieselbe deterministische Rollenauflösung wie der Editor. Der CPU-PNG-Exporter und Dear ImGui/OpenGL sind getrennte Rasterizer; die Bilder sind daher nicht pixelidentisch. Jede Karte verlinkt ihren Hash-, Qualitäts-, Renderparameter- und Asset-Rollennachweis.</div></header><main>$cards</main></body></html>"
$html|Set-Content -LiteralPath (Join-Path $outputPath "index.html") -Encoding utf8
Write-Host "Generated 20-map editor feedback gallery: $(Join-Path $outputPath 'index.html')"
