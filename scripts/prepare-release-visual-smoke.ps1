#requires -Version 5.1

[CmdletBinding(PositionalBinding = $false)]
param(
    [string]$BuildDirectory = "build-release-preflight",
    [string]$DepotRoot = "dist/steam/windows-x64/GMEditor-Depot",
    [string]$OutputRoot = "",
    [string]$RunId = (Get-Date -Format "yyyyMMdd-HHmmss"),
    [ValidateSet("de-DE", "en-US")]
    [string[]]$Locales = @("de-DE", "en-US"),
    [switch]$ResetExistingRun
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'initialize-visual-fixture-assets.ps1')

function Get-FullPath {
    param([string]$Path, [string]$Base)
    if (-not [System.IO.Path]::IsPathRooted($Path)) {
        $Path = Join-Path $Base $Path
    }
    return [System.IO.Path]::GetFullPath($Path).TrimEnd('\', '/')
}

function Test-IsStrictChild {
    param([string]$Child, [string]$Parent)
    $childPath = [System.IO.Path]::GetFullPath($Child).TrimEnd('\', '/')
    $parentPath = [System.IO.Path]::GetFullPath($Parent).TrimEnd('\', '/')
    return $childPath.StartsWith(
        $parentPath + [System.IO.Path]::DirectorySeparatorChar,
        [System.StringComparison]::OrdinalIgnoreCase)
}

function Assert-File {
    param([string]$Path, [string]$Label)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Label not found: $Path"
    }
}

function Write-Utf8 {
    param([string]$Path, [string]$Content)
    $parent = Split-Path -Parent $Path
    [System.IO.Directory]::CreateDirectory($parent) | Out-Null
    [System.IO.File]::WriteAllText($Path, $Content, (New-Object System.Text.UTF8Encoding($false)))
}

function Write-FixtureSettings {
    param([string]$Root, [string]$Locale)
    $language = if ($Locale -eq "de-DE") { "de" } else { "en" }
    $content = @"
# GM Editor release visual-smoke settings
language=$language
autosave_enabled=false
autosave_interval_seconds=120
backup_snapshots_enabled=false
recovery_snapshots_enabled=false
max_recovery_snapshots=5
max_backup_snapshots=10
"@
    Write-Utf8 -Path (Join-Path $Root "GMEditor.settings") -Content $content
}

function Initialize-GermanDemoAssets {
    param([string]$Root, [string]$BlackwaterSource)
    $sourceAssets = Join-Path $BlackwaterSource "Assets"
    $targetAssets = Join-Path $Root "Resources/Campaigns/campaign_release_demo_de/Assets"
    $portraitTarget = Join-Path $targetAssets "Portraits"
    $audioTarget = Join-Path $targetAssets "Audio"
    [System.IO.Directory]::CreateDirectory($portraitTarget) | Out-Null
    [System.IO.Directory]::CreateDirectory($audioTarget) | Out-Null

    $copies = @(
        @{ Source = "Portraits/pc_mira_voss.png"; Destination = (Join-Path $portraitTarget "mira.png") },
        @{ Source = "Portraits/pc_kael_thornevein.png"; Destination = (Join-Path $portraitTarget "thorne.png") },
        @{ Source = "Portraits/pc_elyon_starling.png"; Destination = (Join-Path $portraitTarget "lio.png") },
        @{ Source = "Portraits/pc_nyra_moonbrook.png"; Destination = (Join-Path $portraitTarget "seren.png") },
        @{ Source = "Audio/Ambience/rain_gutter_loop.mp3"; Destination = (Join-Path $audioTarget "regen_am_hafen.mp3") },
        @{ Source = "Audio/Effects/spell_arcane_pulse.wav"; Destination = (Join-Path $audioTarget "torenthuellung.wav") }
    )
    foreach ($copy in $copies) {
        $source = Join-Path $sourceAssets $copy.Source
        Assert-File -Path $source -Label "German demo source asset"
        Copy-Item -LiteralPath $source -Destination $copy.Destination -Force
    }
}

function Invoke-Seeder {
    param([string]$Executable, [string[]]$Arguments, [string]$WorkingDirectory)
    $previousEnvironment = @{}
    foreach ($name in @('TTRPG_DATA_ROOT','TTRPG_DB_PATH','TTRPG_SETTINGS_PATH')) {
        $previousEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
    }
    $env:TTRPG_DATA_ROOT = $WorkingDirectory
    $env:TTRPG_DB_PATH = Join-Path $WorkingDirectory 'TTRPG.db'
    $env:TTRPG_SETTINGS_PATH = Join-Path $WorkingDirectory 'GMEditor.settings'
    Push-Location $WorkingDirectory
    try {
        & $Executable @Arguments
        if ($LASTEXITCODE -ne 0) {
            throw "Seeder returned exit code $LASTEXITCODE`: $Executable"
        }
    }
    finally {
        Pop-Location
        foreach ($name in $previousEnvironment.Keys) {
            [Environment]::SetEnvironmentVariable($name,$previousEnvironment[$name],'Process')
        }
    }
}

function Get-ScenarioSteps {
    param([string]$Id, [string]$Locale)
    if ($Locale -eq "de-DE") {
        switch ($Id) {
            "launcher-empty" { return "Leere Testumgebung starten; keine Kampagne anlegen." }
            "launcher-recent" { return "Deutsche Testumgebung starten; Das Tor unter dem Hafen ausgewählt lassen." }
            "home-empty" { return "Eine leere Kampagne anlegen und Sitzung öffnen; den leeren Zustand mit Sitzung vorbereiten aufnehmen. Die historische Szenario-ID bleibt unverändert." }
            "home-populated" { return "Das Tor unter dem Hafen öffnen; unter Sitzung einen Plan mit drei vorhandenen Szenen und losen Notizen vorbereiten. Eine Szene und ihre Verknüpfungen auswählen; bei schmalem Fenster Szenen und Notizen, Szene sowie Verknüpfungen prüfen." }
            "cast-directory" { return "Deutsche Demo öffnen; Kampagne > Figuren öffnen; Mira Vale auswählen." }
            "ruleset-library" { return "Zu Kampagnen zurückkehren; Regelwerke öffnen; Hafenfantasy - Deutscher Entwurf auswählen." }
            "ruleset-studio" { return "Hafenfantasy - Deutscher Entwurf im Studio öffnen und Kernlisten auswählen." }
            "story-board" { return "Deutsche Demo öffnen; Kampagne > Geschichte öffnen; Der Obsidianschlüssel dreht sich auswählen und bereitstellen." }
            "world-map-edit" { return "Deutsche Demo öffnen; Kampagne > Welt öffnen; Hafenviertel im Karteneditor Bearbeiten öffnen. Kontextwerkzeuge, Speichern, Rückgängig und Zoom prüfen; im schmalen Fenster den Inspektor öffnen und schließen. Auswählen ist ein gültiges Bearbeitungswerkzeug." }
            "map-generation-preview" { return "Deutsche Demo öffnen; Kampagne > Welt öffnen; eine Welt- oder Regionskarte über Beschreiben und Anforderungen prüfen erstellen. Entwurf wählen vor der Übernahme aufnehmen: erkennbare Komposition, Anforderungen und Prüfhinweise; Qualitätszahlen nur unter Erweitert. Orts-, Dungeon- und Höhlengeneratoren getrennt auf Experimentell-Kennzeichnung prüfen." }
            "world-shop" { return "Deutsche Demo öffnen; Kampagne > Welt öffnen; Läden und danach Hafenausrüster auswählen." }
            "world-dungeon" { return "Deutsche Demo öffnen; Kampagne > Welt öffnen; Gewölbe und danach einen Raum auswählen. Bestehende Raumbearbeitung bleibt unabhängig vom experimentellen Generator verfügbar." }
            "encounter-prepare" { return "Deutsche Demo öffnen; Kampagne > Begegnungen öffnen; Durchbruch im Archiv auswählen und Vorbereiten öffnen." }
            "live-setup" { return "Deutsche Demo öffnen; vorbereitete Sitzung starten; in Live Aufbau auswählen. Identität der vorbereiteten Sitzung prüfen." }
            "live-scene" { return "Deutsche Demo öffnen; vorbereitete Sitzung in Live fortsetzen; Szene und Planfortschritt anzeigen." }
            "live-exploration" { return "Deutsche Demo öffnen; vorbereitete Sitzung in Live fortsetzen; Erkundung auswählen." }
            "live-combat" { return "Deutsche Demo öffnen; vorbereitete Sitzung in Live fortsetzen; Kampf auswählen und eingebettete Zugsteuerung, Schlachtfeld sowie nächsten Schritt prüfen." }
            "live-break" { return "Deutsche Demo öffnen; vorbereitete Sitzung in Live fortsetzen; Pause auswählen." }
            "live-wrap-up" { return "Deutsche Demo öffnen; vorbereitete Sitzung in Live fortsetzen; Abschluss auswählen." }
            "compendium-items" { return "Deutsche Demo öffnen; Kampagne > Kompendium öffnen; Gegenstände und danach Obsidianschlüssel auswählen." }
            "compendium-audio" { return "Deutsche Demo öffnen; Kampagne > Kompendium öffnen; Audio auswählen." }
            "settings-health" { return "Deutsche Demo öffnen; Einstellungen öffnen; Projektzustand anzeigen." }
            "player-display" { return "Deutsche Demo öffnen; gewünschte öffentliche Szene und Karte ausdrücklich bereitstellen, GM-/Spielervorschau prüfen und erst danach freigeben. Im echten Spielerfenster unabhängig auf SL-Inhalte, Nebel und verborgene Tokens prüfen; Schwarzschaltung und erneute Prüfung nach Inhaltsänderung testen." }
            default { return "Den im visuellen Szenariokatalog genannten Zustand in der deutschen Demo herstellen." }
        }
    }
    switch ($Id) {
        "launcher-empty" { return "Start the empty fixture; do not create a campaign." }
        "launcher-recent" { return "Start the rich fixture; leave the selected recent campaign on the launcher." }
        "home-empty" { return "Create a blank campaign and open Session; capture the empty state with Create a session. The historical scenario ID stays unchanged." }
        "home-populated" { return "Open Codex Feature Tour - Current Systems; prepare a Session with three existing scenes and rough notes. Select a scene and linked material; at narrow widths check Scenes & notes, Scene and Linked material panes." }
        "cast-directory" { return "Open Codex Feature Tour; open Campaign > Cast; select Mira Vale." }
        "ruleset-library" { return "Return to Campaigns; open Rulesets; select Clockwork Bloodline." }
        "ruleset-studio" { return "From Rulesets select Clockwork Bloodline, open its draft in Studio, then select Core Lists." }
        "story-board" { return "Open The Bell Beneath Blackwater; open Campaign > Story; select and stage a scene." }
        "world-map-edit" { return "Open Codex Feature Tour; open Campaign > World and a map in the editor's Edit workspace. Check contextual tools, Save, Undo and Zoom; at narrow widths open and close the inspector. Select is a valid editing tool." }
        "map-generation-preview" { return "Open Codex Feature Tour; open Campaign > World; create a world or region map through Describe and Review requirements. Capture Choose design before acceptance: readable composition, requirements and review notes, with numeric quality under Advanced only. Separately check Experimental labels for settlement, dungeon and cave generators." }
        "world-shop" { return "Open Codex Feature Tour; open Campaign > World; select Shops and a stocked shop." }
        "world-dungeon" { return "Open Codex Feature Tour; open Campaign > World; select Dungeons and a room. Existing room editing remains available independently of the experimental generator." }
        "encounter-prepare" { return "Open Codex Feature Tour; open Campaign > Encounters; select Archive Breach and Prepare." }
        "live-setup" { return "Open Codex Feature Tour; run a prepared session; select Live > Setup and verify the prepared session identity is retained." }
        "live-scene" { return "Open Codex Feature Tour; resume the prepared session in Live; show Scene and plan progress." }
        "live-exploration" { return "Open Codex Feature Tour; resume the prepared session in Live; select Exploration." }
        "live-combat" { return "Open Codex Feature Tour; resume the prepared session in Live; select Combat and check embedded turn controls, battlefield and next action." }
        "live-break" { return "Open Codex Feature Tour; resume the prepared session in Live; select Break." }
        "live-wrap-up" { return "Open Codex Feature Tour; resume the prepared session in Live; select Wrap-up." }
        "compendium-items" { return "Open Codex Feature Tour; open Campaign > Compendium; select Items and one item." }
        "compendium-audio" { return "Open Codex Feature Tour; open Campaign > Compendium; select Audio." }
        "settings-health" { return "Open Codex Feature Tour; open Settings; show Project Health." }
        "player-display" { return "Open Codex Feature Tour; explicitly stage the intended public scene/map, inspect the GM/player review, then publish. Independently check the real player window for private content, fog and hidden tokens; test Blackout and fresh review after payload changes." }
        default { return "Follow the state named in the visual scenario catalog." }
    }
}

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
if (-not $OutputRoot) { $OutputRoot = Join-Path $env:LOCALAPPDATA 'GMEditor-EngineeringEvidence/visual-smoke' }
$buildRoot = Get-FullPath -Path $BuildDirectory -Base $projectRoot
$depot = Get-FullPath -Path $DepotRoot -Base $projectRoot
$outputBase = Get-FullPath -Path $OutputRoot -Base $projectRoot
$runRoot = Get-FullPath -Path (Join-Path $outputBase $RunId) -Base $projectRoot

if ($outputBase -eq $projectRoot -or (Test-IsStrictChild -Child $outputBase -Parent $projectRoot)) {
    throw "OutputRoot must be outside the project checkout: $outputBase"
}
if (-not (Test-IsStrictChild -Child $runRoot -Parent $outputBase)) {
    throw "Run path must be a strict child of OutputRoot: $runRoot"
}
if (Test-Path -LiteralPath $runRoot) {
    if (-not $ResetExistingRun) {
        throw "Run already exists. Choose another RunId or pass -ResetExistingRun: $runRoot"
    }
    # Preserve existing captured evidence when reusing a run name.
    $resolvedRun = (Resolve-Path -LiteralPath $runRoot).Path
    $archiveRun = [IO.Path]::GetFullPath($runRoot + '.archived-' + [Guid]::NewGuid().ToString('N'))
    if (-not (Test-IsStrictChild -Child $resolvedRun -Parent $outputBase) -or
        -not (Test-IsStrictChild -Child $archiveRun -Parent $outputBase)) { throw 'Run archive escaped OutputRoot.' }
    Move-Item -LiteralPath $resolvedRun -Destination $archiveRun
}

$editorExe = Join-Path $depot "GMEditor.exe"
$featureSeeder = Join-Path $buildRoot "seed_current_feature_tour.exe"
$blackwaterSeeder = Join-Path $buildRoot "seed_blackwater_campaign.exe"
$clockworkSeeder = Join-Path $buildRoot "seed_clockwork_bloodline.exe"
$germanDemoSeeder = Join-Path $buildRoot "seed_release_demo_de.exe"
Assert-File -Path $editorExe -Label "Packaged editor"
Assert-File -Path (Join-Path $depot "gm_editor.install.json") -Label "Install marker"
Assert-File -Path $featureSeeder -Label "Feature-tour seeder"
Assert-File -Path $blackwaterSeeder -Label "Blackwater seeder"
Assert-File -Path $germanDemoSeeder -Label "German release-demo seeder"
if ($Locales -contains "en-US") {
    Assert-File -Path $clockworkSeeder -Label "Clockwork seeder"
}

[System.IO.Directory]::CreateDirectory($runRoot) | Out-Null
$fixtures = New-Object System.Collections.Generic.List[object]
foreach ($locale in $Locales) {
    $localeSuffix = if ($locale -eq "de-DE") { "de" } else { "en" }

    $emptyRoot = Join-Path $runRoot "fixtures/empty-$localeSuffix"
    [System.IO.Directory]::CreateDirectory($emptyRoot) | Out-Null
    Write-FixtureSettings -Root $emptyRoot -Locale $locale
    $fixtures.Add([PSCustomObject]@{
        id = "empty-$localeSuffix"
        locale = $locale
        profile = "empty-library"
        dataRoot = $emptyRoot
        campaigns = @()
    })

    $richRoot = Join-Path $runRoot "fixtures/rich-$localeSuffix"
    $campaignRoot = Join-Path $richRoot "Resources/Campaigns"
    [System.IO.Directory]::CreateDirectory($campaignRoot) | Out-Null
    Write-Utf8 -Path (Join-Path $richRoot "CMakeLists.txt") -Content "# Isolated visual-smoke fixture marker.`n"
    Initialize-VisualFixtureAssets -Root $richRoot -AudioSource (Join-Path $projectRoot "Resources/Audio")
    $blackwaterSource = Join-Path $campaignRoot "campaign_bell_beneath_blackwater"
    Write-FixtureSettings -Root $richRoot -Locale $locale
    Invoke-Seeder -Executable $blackwaterSeeder -Arguments @() -WorkingDirectory $richRoot
    if ($locale -eq "en-US") {
        Invoke-Seeder -Executable $clockworkSeeder -Arguments @() -WorkingDirectory $richRoot
    }
    Invoke-Seeder -Executable $featureSeeder -Arguments @((Join-Path $richRoot "TTRPG.db")) -WorkingDirectory $richRoot
    $demoRevision = $null
    $demoFingerprint = $null
    if ($locale -eq "de-DE") {
        Initialize-GermanDemoAssets -Root $richRoot -BlackwaterSource $blackwaterSource
        Invoke-Seeder -Executable $germanDemoSeeder -Arguments @("--isolate", (Join-Path $richRoot "TTRPG.db")) -WorkingDirectory $richRoot
        $verification = & $germanDemoSeeder --verify-only (Join-Path $richRoot "TTRPG.db")
        if ($LASTEXITCODE -ne 0) {
            throw "German release-demo verification failed with exit code $LASTEXITCODE."
        }
        $verificationText = ($verification -join "`n")
        if ($verificationText -notmatch 'revision=([^ |]+) \| fingerprint=([0-9a-f]{16})') {
            throw "German release-demo verification did not return revision evidence: $verificationText"
        }
        $demoRevision = $Matches[1]
        $demoFingerprint = $Matches[2]
    }
    Assert-File -Path (Join-Path $richRoot "TTRPG.db") -Label "Rich screenshot database"
    $fixtures.Add([PSCustomObject]@{
        id = "rich-$localeSuffix"
        locale = $locale
        profile = if ($locale -eq "de-DE") { "release-rich-de-authored" } else { "release-rich" }
        dataRoot = $richRoot
        campaigns = @(if ($locale -eq "de-DE") { "Das Tor unter dem Hafen" } else { "The Bell Beneath Blackwater"; "Clockwork Bloodline"; "Codex Feature Tour - Current Systems" })
        demoRevision = $demoRevision
        demoFingerprint = $demoFingerprint
    })
}

$catalogPath = Join-Path $projectRoot "Models/UX/VisualScenarioCatalog.cpp"
Assert-File -Path $catalogPath -Label "Visual scenario catalog"
$scenarioPattern = '^\s*\{"([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)"(?:,\s*(true|false))?\},?\s*$'
$scenarios = New-Object System.Collections.Generic.List[object]
foreach ($line in [System.IO.File]::ReadAllLines($catalogPath)) {
    if ($line -match $scenarioPattern) {
        $scenarios.Add([PSCustomObject]@{
            id = $Matches[1]
            surface = $Matches[2]
            seedProfile = $Matches[3]
            state = $Matches[4]
            playerSafe = ($Matches[5] -eq "true")
        })
    }
}
if ($scenarios.Count -ne 23) {
    throw "Expected 23 scenarios in VisualScenarioCatalog.cpp, found $($scenarios.Count)."
}

$viewports = @(
    [PSCustomObject]@{ key = "1280x720"; width = 1280; height = 720; uiScale = 1.0 },
    [PSCustomObject]@{ key = "1920x1080"; width = 1920; height = 1080; uiScale = 1.0 },
    [PSCustomObject]@{ key = "ultrawide"; width = 3440; height = 1440; uiScale = 1.0 },
    [PSCustomObject]@{ key = "1920x1080-150pct"; width = 1920; height = 1080; uiScale = 1.5 }
)

$queue = New-Object System.Collections.Generic.List[object]
foreach ($scenario in $scenarios) {
    foreach ($viewport in $viewports) {
        foreach ($locale in $Locales) {
            $localeSuffix = if ($locale -eq "de-DE") { "de" } else { "en" }
            $fixture = if ($scenario.seedProfile -eq "empty-library") { "empty-$localeSuffix" } else { "rich-$localeSuffix" }
            $priority = if ($locale -eq "de-DE" -and $viewport.key -in @("1280x720", "1920x1080-150pct")) { "P0" } elseif ($viewport.key -eq "1920x1080") { "P1" } else { "P2" }
            $queue.Add([PSCustomObject]@{
                captureId = "$($scenario.id)__$($viewport.key)__$locale"
                scenarioId = $scenario.id
                surface = $scenario.surface
                expectedState = $scenario.state
                seedProfile = $scenario.seedProfile
                fixture = $fixture
                locale = $locale
                viewportKey = $viewport.key
                width = $viewport.width
                height = $viewport.height
                uiScale = $viewport.uiScale
                playerSafe = $scenario.playerSafe
                priority = $priority
                navigation = Get-ScenarioSteps -Id $scenario.id -Locale $locale
                status = "pending"
            })
        }
    }
}

$queuePath = Join-Path $runRoot "capture-queue.csv"
$queue | Export-Csv -LiteralPath $queuePath -NoTypeInformation -Encoding UTF8

$editorHash = (Get-FileHash -LiteralPath $editorExe -Algorithm SHA256).Hash.ToLowerInvariant()
$assetInventory = @(foreach ($fixture in $fixtures) {
    $root = Join-Path $fixture.dataRoot 'Resources'
    if (Test-Path -LiteralPath $root) {
        foreach ($asset in (Get-ChildItem -LiteralPath $root -Recurse -File | Where-Object Extension -notin @('.db','.ini') | Sort-Object FullName)) {
            [ordered]@{fixture=$fixture.id; path=$asset.FullName.Substring($root.Length+1); sha256=(Get-FileHash -LiteralPath $asset.FullName -Algorithm SHA256).Hash}
        }
    }
})
$assetInventoryPath = Join-Path $runRoot 'fixture-assets.json'
Write-Utf8 -Path $assetInventoryPath -Content (($assetInventory | ConvertTo-Json -Depth 5) + "`n")
$manifest = [ordered]@{
    schemaVersion = 1
    runId = $RunId
    createdAtUtc = [DateTime]::UtcNow.ToString("o")
    editorExecutable = $editorExe
    editorSha256 = $editorHash
    sourceCommit = (& git -C $projectRoot rev-parse HEAD).Trim()
    sourceStatus = @(& git -C $projectRoot status --porcelain)
    fixtureGenerator = 'disposable-visual-fixtures-v1; synthetic checker identity markers and existing repository audio'
    fixtureAssetsSha256 = (Get-FileHash -LiteralPath $assetInventoryPath -Algorithm SHA256).Hash
    scenarioCatalog = $catalogPath
    expectedScenarioCount = 23
    expectedCaptureCount = $queue.Count
    fixtures = $fixtures.ToArray()
    viewports = $viewports
    queue = $queuePath
    captureMode = "manual state selection plus exact client-area pixel capture"
    limitations = @(
        "This legacy capture command requires human state review; run-desktop-qa.ps1 separately exercises automated input and framebuffer comparisons.",
        "Every capture therefore requires explicit tester confirmation of the visible state.",
        "Ultrawide capture requires a display at least 3440x1440.",
        "The 150 percent case requires Windows display scaling at 150 percent; text zoom is not equivalent."
    )
}
$manifestPath = Join-Path $runRoot "visual-smoke-run.json"
Write-Utf8 -Path $manifestPath -Content (($manifest | ConvertTo-Json -Depth 8) + "`n")

$readme = @"
# GM Editor 0.9 Visual-Smoke Run $RunId

This directory contains isolated, disposable screenshot fixtures. No user database or repository-root `TTRPG.db` is read or written.

## Start a fixture

```powershell
.\scripts\start-release-visual-smoke.ps1 -RunRoot "$runRoot" -Fixture rich-de -ViewportKey 1280x720
```

Navigate to the exact state from `capture-queue.csv`, then capture it only after checking the visible state:

```powershell
.\scripts\capture-release-visual-smoke.ps1 -RunRoot "$runRoot" -ScenarioId home-populated -ConfirmState
```

Player Display additionally requires `-PlayerSafetyConfirmed`. Close GM Editor normally after the pass. Re-run this preparation script with a new RunId to restore pristine fixtures.

The complete matrix contains $($queue.Count) cases. P0 covers German at 1280x720 and real Windows 150 percent scaling, P1 is 1920x1080 in both languages, and P2 contains ultrawide plus the additional English scaling pass.
"@
Write-Utf8 -Path (Join-Path $runRoot "README.md") -Content $readme

Write-Host "Prepared GM Editor 0.9 visual-smoke run: $runRoot"
Write-Host "Fixtures: $($fixtures.Count) | Scenarios: $($scenarios.Count) | Captures: $($queue.Count)"
Write-Host "Manifest: $manifestPath"
Write-Host "Queue: $queuePath"
