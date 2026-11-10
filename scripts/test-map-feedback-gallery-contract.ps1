param(
    [string]$BuildDirectory = "build-release",
    [string]$OutputRoot = "build/map-feedback/gallery-contract-tests"
)

$ErrorActionPreference = "Stop"
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
function Project-Path([string]$Path) {
    if ([IO.Path]::IsPathRooted($Path)) { return [IO.Path]::GetFullPath($Path) }
    return [IO.Path]::GetFullPath((Join-Path $repositoryRoot $Path))
}
function Require([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

$buildPath = Project-Path $BuildDirectory
$exporter = @(
    (Join-Path $buildPath "map_generation_preview_export.exe"),
    (Join-Path $buildPath "Release/map_generation_preview_export.exe")
) | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
if ([string]::IsNullOrWhiteSpace($exporter)) {
    throw "Build map_generation_preview_export first."
}

$runDirectory = Join-Path (Project-Path $OutputRoot) ([guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $runDirectory | Out-Null

$common = @(
    "--preset", "balanced", "--scope", "world", "--width", "72", "--height", "46",
    "--water", "20", "--mountains", "13", "--forests", "50", "--settlements", "5",
    "--routes", "14", "--landscape", "forest_frontier", "--locale", "de", "--no-fog"
)

Push-Location $repositoryRoot
try {
    # Use a deliberately contradictory Must brief for a stable fail-closed
    # assertion. A formerly rejected seed can legitimately become valid after
    # generator repairs and is therefore not a durable negative fixture.
    $invalidJson = Join-Path $runDirectory "rejected.json"
    $invalidPng = Join-Path $runDirectory "rejected.png"
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    # Exercise the fail-closed path deliberately with one attempt. The normal
    # editor/preview contract below uses the fixed Balanced budget of 24.
    $invalidLog = (& $exporter @common --quality-attempts 1 --seed 41004 --world-note "The world must contain a river. No rivers." --out $invalidJson --png $invalidPng --pixels-per-cell 8 2>&1 | Out-String)
    $invalidExit = $LASTEXITCODE
    $ErrorActionPreference = $previousPreference
    Require ($invalidExit -ne 0) "Contradictory Must brief unexpectedly returned success."
    Require (-not (Test-Path -LiteralPath $invalidJson)) "Rejected seed wrote JSON evidence despite the fail-closed gate."
    Require (-not (Test-Path -LiteralPath $invalidPng)) "Rejected seed wrote a PNG despite the fail-closed gate."
    # PowerShell versions differ in whether redirected native stderr is
    # returned as text or only written to the error stream. Exit status plus
    # the absence of both artifacts is the portable fail-closed assertion.

    $defaultJson = Join-Path $runDirectory "default.json"
    $defaultPng = Join-Path $runDirectory "default.png"
    & $exporter @common --quality-attempts 24 --seed 41006 --out $defaultJson --png $defaultPng --pixels-per-cell 8 | Out-Null
    Require ($LASTEXITCODE -eq 0) "Valid seed failed without a route overlay."
    $default = Get-Content -Raw -LiteralPath $defaultJson | ConvertFrom-Json
    Require ([int]$default.quality.score -ge 85) "Valid gallery fixture did not meet the score floor."
    Require ([bool]$default.quality.validation_eligible) "Valid gallery fixture is not validation-eligible."
    Require ([bool]$default.quality.candidate_floor_passed) "Valid gallery fixture did not meet every relevant category floor."
    Require ([int]$default.quality.critical_issue_count -eq 0) "Valid gallery fixture contains critical issues."
    Require (-not [bool]$default.render.show_routes) "Normal design preview enabled MapRoute overlays by default."
    Require ([int]$default.render.visible_route_overlay_count -eq 0) "Normal design preview exposed a MapRoute overlay."
    Require ([int]$default.render.available_route_overlay_count -gt 0) "Route visibility test fixture does not contain a route model."
    Require ([bool]$default.render.compiled_plan_consumed_by_png) "PNG evidence does not confirm consumption of the explicitly compiled render plan."
    Require ([string]$default.render.audience -eq "gm" -and [string]$default.render.locale -eq "de") "Render-plan audience or locale differs from the requested preview contract."
    Require ([string]$default.render.render_plan_style_snapshot_id -eq [string]$default.style_snapshot_id) "Render plan and generated map use different production snapshots."
    Require ([string]$default.render.render_plan_content_hash -match '^[0-9a-f]{16}$') "Render-plan content hash is missing or malformed."
    Require (-not [bool]$default.render.pixel_identical_to_imgui_editor) "Evidence must not claim unproven CPU/ImGui pixel identity."

    $routeJson = Join-Path $runDirectory "explicit-route.json"
    $routePng = Join-Path $runDirectory "explicit-route.png"
    & $exporter @common --quality-attempts 24 --seed 41006 --show-routes --out $routeJson --png $routePng --pixels-per-cell 8 | Out-Null
    Require ($LASTEXITCODE -eq 0) "Valid seed failed with an explicitly requested route overlay."
    $explicit = Get-Content -Raw -LiteralPath $routeJson | ConvertFrom-Json
    Require ([bool]$explicit.render.show_routes) "Explicit --show-routes request was not retained in evidence."
    Require ([int]$explicit.render.visible_route_overlay_count -gt 0) "Explicit route request produced no visible route overlay."
    Require ([bool]$explicit.render.compiled_plan_consumed_by_png) "Explicit-route PNG did not retain the compiled-plan contract."
    Require ([string]$default.render.render_plan_content_hash -ne [string]$explicit.render.render_plan_content_hash) "Changing the route layer did not change the immutable render-plan hash."
    Require ([string]$default.hashes.graph_hash -eq [string]$explicit.hashes.graph_hash) "Presentation-only route selection changed the feature graph."
    Require ([string]$default.hashes.raster_hash -eq [string]$explicit.hashes.raster_hash) "Presentation-only route selection changed raster semantics."
    Require ((Get-FileHash -Algorithm SHA256 -LiteralPath $defaultPng).Hash -ne
             (Get-FileHash -Algorithm SHA256 -LiteralPath $routePng).Hash) "Explicit route overlay did not change the PNG presentation."

    $evidence = [ordered]@{
        schema_version = 1
        passed = $true
        rejected_seed = 41004
        rejected_exit_code = $invalidExit
        accepted_seed = 41006
        accepted_score = [int]$default.quality.score
        default_routes_visible = [bool]$default.render.show_routes
        explicit_routes_visible = [bool]$explicit.render.show_routes
        available_route_overlay_count = [int]$default.render.available_route_overlay_count
        explicit_visible_route_overlay_count = [int]$explicit.render.visible_route_overlay_count
        graph_hash = [string]$default.hashes.graph_hash
        raster_hash = [string]$default.hashes.raster_hash
        default_render_plan_hash = [string]$default.render.render_plan_content_hash
        explicit_route_render_plan_hash = [string]$explicit.render.render_plan_content_hash
        render_plan_locale = [string]$default.render.locale
        render_plan_audience = [string]$default.render.audience
        pixel_identity_claimed = [bool]$default.render.pixel_identical_to_imgui_editor
    }
    $evidence | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $runDirectory "contract-test.json") -Encoding utf8
    Write-Host "Map feedback gallery contract passed: $(Join-Path $runDirectory 'contract-test.json')"
}
finally {
    Pop-Location
}
