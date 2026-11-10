param(
    [string]$ManifestPath = "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3/pack_manifest.json",
    [string]$QaProfilePath = "Resources/Maps/Tilesets/Source/FantasyNeutral/v3/qa_profile.json",
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

function Resolve-ProjectPath([string]$Path) {
    if ([IO.Path]::IsPathRooted($Path)) { return [IO.Path]::GetFullPath($Path) }
    return [IO.Path]::GetFullPath((Join-Path $repositoryRoot $Path))
}

function Get-RangeRole([int]$Variant, $Ranges) {
    $offset = $Variant
    foreach ($range in $Ranges) {
        if ($offset -le [int]$range.count) { return [string]$range.name }
        $offset -= [int]$range.count
    }
    return "unclassified"
}

function Get-SemanticMetadata([string]$Family, [int]$Variant) {
    $scales = @("world", "region", "site", "tactical")
    $rotations = @(0, 1, 2, 3)
    $footprint = [ordered]@{ width = 1; height = 1 }
    $anchors = @()
    $neighbors = @("terrain_water")
    $role = "unclassified"
    $edgeContract = "none"

    switch ($Family) {
        "terrain_water" {
            $role = Get-RangeRole $Variant @(
                @{name="grass_temperate";count=8}, @{name="forest_floor";count=4},
                @{name="earth";count=4}, @{name="mud";count=3}, @{name="gravel";count=3},
                @{name="field_soil";count=4}, @{name="coastal_sand";count=4},
                @{name="water_shallow";count=6}, @{name="water_deep";count=6},
                @{name="cave_floor";count=6}, @{name="dressed_stone_floor";count=6})
            $rotations = @(0)
            $anchors = @("tile_edges")
            $neighbors = @("terrain_water", "transitions_details")
            $edgeContract = "seamless_material"
        }
        "transitions_details" {
            $role = if ($Variant -le 16) { "organic_blend_mask" } else { "ground_detail_decal" }
            $anchors = if ($Variant -le 16) { @("tile_edges") } else { @("center") }
            $neighbors = @("terrain_water", "roads_rivers", "vegetation", "landscape")
            $edgeContract = if ($Variant -le 16) { "organic_transition" } else { "transparent_decal" }
        }
        "roads_rivers" {
            $role = Get-RangeRole $Variant @(
                @{name="path";count=6}, @{name="road_dirt";count=6}, @{name="road_gravel";count=6},
                @{name="road_stone";count=6}, @{name="creek";count=4}, @{name="river";count=4},
                @{name="river_deep";count=4}, @{name="canal";count=6})
            $anchors = @("entry_a", "entry_b")
            $neighbors = @("terrain_water", "crossings", "settlements")
            $edgeContract = "continuous_curve_profile"
        }
        "crossings" {
            $role = Get-RangeRole $Variant @(
                @{name="bridge_wood_light";count=5}, @{name="bridge_wood_heavy";count=5},
                @{name="bridge_stone_rough";count=5}, @{name="bridge_stone_dressed";count=5},
                @{name="ford";count=4}, @{name="pier";count=4})
            $footprint = [ordered]@{ width = 2; height = 1 }
            $anchors = @("approach_a", "span", "approach_b")
            $neighbors = @("roads_rivers", "terrain_water")
            $edgeContract = "crossing_span"
        }
        "vegetation" {
            $role = Get-RangeRole $Variant @(
                @{name="deciduous_group";count=6}, @{name="conifer_group";count=6},
                @{name="mixed_tree_group";count=6}, @{name="forest_edge";count=6},
                @{name="clearing";count=6}, @{name="bushes";count=6},
                @{name="undergrowth_stumps";count=6}, @{name="fallen_tree";count=6})
            $footprint = [ordered]@{ width = 2; height = 2 }
            $anchors = @("ground")
            $neighbors = @("terrain_water", "vegetation", "roads_rivers")
            $edgeContract = "transparent_stamp"
        }
        "landscape" {
            $role = Get-RangeRole $Variant @(
                @{name="peak";count=5}, @{name="connected_ridge";count=5}, @{name="rock_group";count=5},
                @{name="scree";count=5}, @{name="cliff";count=5}, @{name="slope";count=5},
                @{name="mountain_pass";count=5}, @{name="foothill";count=5})
            $footprint = [ordered]@{ width = 3; height = 3 }
            $anchors = @("ground", "component_edges")
            $neighbors = @("terrain_water", "landscape", "vegetation", "roads_rivers")
            $edgeContract = "component_stamp"
        }
        "settlements" {
            $role = Get-RangeRole $Variant @(
                @{name="rural";count=16}, @{name="town_service";count=16},
                @{name="fortification";count=15}, @{name="waterfront";count=13})
            $footprint = [ordered]@{ width = 2; height = 2 }
            $anchors = @("ground", "entrance")
            $neighbors = @("terrain_water", "roads_rivers", "crossings", "settlements")
            $edgeContract = "structure_footprint"
        }
        "tactical_outdoor" {
            $role = Get-RangeRole $Variant @(
                @{name="fence";count=4}, @{name="low_wall";count=4}, @{name="barricade";count=4},
                @{name="wagon_cart";count=4}, @{name="tent_camp";count=4}, @{name="campfire";count=4},
                @{name="crate_barrel";count=4}, @{name="ruin";count=4}, @{name="rubble";count=4},
                @{name="well_log_cover";count=4})
            $scales = @("site", "tactical")
            $anchors = @("ground")
            $neighbors = @("terrain_water", "roads_rivers", "vegetation", "settlements")
            $edgeContract = "tactical_footprint"
        }
        "cave_dungeon" {
            $role = Get-RangeRole $Variant @(
                @{name="cave_natural";count=16}, @{name="dungeon_architecture";count=16},
                @{name="dungeon_furnishing";count=16}, @{name="ancient_detail";count=8})
            $scales = @("site", "tactical", "dungeon")
            $anchors = @("floor_or_wall")
            $neighbors = @("cave_dungeon", "terrain_water", "transitions_details")
            $edgeContract = "dungeon_context"
        }
        "map_symbols" {
            $names = @("capital", "city", "town", "village", "fortress", "ruin", "cave", "harbor", "temple", "tower", "camp", "crossing", "quest", "danger", "discovery", "route", "party", "spawn_party", "spawn_opposition", "cover", "elevation", "door", "hazard", "interaction")
            $role = $names[[Math]::Min($names.Count - 1, [Math]::Max(0, $Variant - 1))]
            $rotations = @(0)
            $anchors = @("point")
            $neighbors = @("terrain_water", "settlements", "cave_dungeon")
            $edgeContract = "map_symbol"
        }
    }

    return [ordered]@{
        semantic_role = $role
        allowed_map_types = $scales
        footprint = $footprint
        anchors = $anchors
        allowed_rotations_quarter_turns = $rotations
        compatible_neighbor_families = $neighbors
        edge_contract = $edgeContract
        zoom_range = [ordered]@{ minimum = 0.35; maximum = 4.0 }
        lighting_profile = "neutral_overhead_soft"
        palette_profile = "muted_warm_natural"
        source_prompt_reference = "Resources/Maps/Tilesets/Source/FantasyNeutral/v3/production_prompts.md"
        qa_history = @()
        supersedes = ""
        license_review_status = "pending"
    }
}

$manifestFile = Resolve-ProjectPath $ManifestPath
$profileFile = Resolve-ProjectPath $QaProfilePath
if (-not (Test-Path -LiteralPath $manifestFile -PathType Leaf)) { throw "Missing manifest: $manifestFile" }
if (-not (Test-Path -LiteralPath $profileFile -PathType Leaf)) { throw "Missing QA profile: $profileFile" }
$manifest = Get-Content -Raw -LiteralPath $manifestFile | ConvertFrom-Json
$profile = Get-Content -Raw -LiteralPath $profileFile | ConvertFrom-Json

foreach ($asset in @($manifest.assets)) {
    $metadata = Get-SemanticMetadata ([string]$asset.family) ([int]$asset.variant)
    foreach ($property in $metadata.GetEnumerator()) {
        $asset | Add-Member -NotePropertyName $property.Key -NotePropertyValue $property.Value -Force
    }
}

$manifest.schema_version = 3
$manifest | Add-Member -NotePropertyName art_direction_id -NotePropertyValue ([string]$profile.art_direction_id) -Force
$manifest | Add-Member -NotePropertyName qa_profile -NotePropertyValue "Resources/Maps/Tilesets/Source/FantasyNeutral/v3/qa_profile.json" -Force
$manifest | Add-Member -NotePropertyName semantic_catalog_version -NotePropertyValue 1 -Force
$manifest | Add-Member -NotePropertyName production_snapshot_policy -NotePropertyValue "complete_accepted_snapshot_only" -Force
$manifest.accepted_count = @($manifest.assets | Where-Object status -eq "accepted").Count
$manifest.candidate_count = @($manifest.assets | Where-Object status -eq "candidate").Count
$manifest.needs_repaint_count = @($manifest.assets | Where-Object status -eq "needs_repaint").Count

$destination = if ([string]::IsNullOrWhiteSpace($OutputPath)) { $manifestFile } else { Resolve-ProjectPath $OutputPath }
$parent = Split-Path -Parent $destination
if (-not [string]::IsNullOrWhiteSpace($parent)) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
$manifest | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $destination -Encoding utf8
Write-Host "Enriched $(@($manifest.assets).Count) FantasyNeutral V3 assets: $destination"
