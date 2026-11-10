#requires -Version 5.1

[CmdletBinding(PositionalBinding = $false)]
param(
    [string]$DepotRoot = "dist/steam/windows-x64/GMEditor-Depot",
    [string]$OutputRoot = "build/release-evidence/manual-qa",
    [string]$RunId = "",
    [string]$PreparedBy = $env:USERNAME,
    [string]$MachineLabel = $env:COMPUTERNAME
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
function Resolve-ProjectPath([string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $projectRoot $Path))
}

function Read-KeyValueFile([string]$Path) {
    $values = @{}
    foreach ($line in @(Get-Content -LiteralPath $Path)) {
        if ($line -notmatch '^([^=]+)=(.*)$') {
            throw "Invalid key/value line in $Path`: $line"
        }
        if ($values.ContainsKey($Matches[1])) {
            throw "Duplicate key '$($Matches[1])' in $Path"
        }
        $values[$Matches[1]] = $Matches[2]
    }
    return $values
}

$depot = Resolve-ProjectPath $DepotRoot
$outputBase = Resolve-ProjectPath $OutputRoot
if (-not (Test-Path -LiteralPath $depot -PathType Container)) {
    throw "Depot does not exist: $depot"
}

& (Join-Path $PSScriptRoot "assert-steam-depot.ps1") -DepotRoot $depot

$executable = Join-Path $depot "GMEditor.exe"
$checksumManifest = Join-Path $depot "SHA256SUMS.txt"
$versionFile = Join-Path $depot "VERSION.txt"
$version = Read-KeyValueFile $versionFile
$exeSha256 = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash.ToLowerInvariant()
$manifestSha256 = (Get-FileHash -LiteralPath $checksumManifest -Algorithm SHA256).Hash.ToLowerInvariant()

if ([string]$version["binary_sha256"] -ne $exeSha256) {
    throw "VERSION.txt is not bound to the current depot executable."
}
if ([string]::IsNullOrWhiteSpace($PreparedBy) -or [string]::IsNullOrWhiteSpace($MachineLabel)) {
    throw "PreparedBy and MachineLabel must be non-empty."
}
if ([string]::IsNullOrWhiteSpace($RunId)) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMdd-HHmmss")
    $RunId = "manual-$($version['version'])-$($exeSha256.Substring(0, 12))-$stamp"
}
if ($RunId -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]{2,95}$') {
    throw "RunId may contain only letters, digits, dots, underscores and hyphens."
}

$runRoot = Join-Path $outputBase $RunId
if (Test-Path -LiteralPath $runRoot) {
    throw "Manual-QA run already exists and will not be overwritten: $runRoot"
}
[System.IO.Directory]::CreateDirectory($runRoot) | Out-Null
[System.IO.Directory]::CreateDirectory((Join-Path $runRoot "evidence")) | Out-Null

$cases = New-Object 'System.Collections.Generic.List[object]'
function Add-ManualCase {
    param(
        [string]$Id,
        [ValidateSet("P0", "P1")][string]$Priority,
        [string]$Suite,
        [string]$Locale,
        [string]$Title,
        [string]$ExpectedResult,
        [string]$EvidenceRequirement
    )
    $cases.Add([PSCustomObject][ordered]@{
        caseId = $Id
        priority = $Priority
        suite = $Suite
        locale = $Locale
        title = $Title
        expectedResult = $ExpectedResult
        evidenceRequirement = $EvidenceRequirement
    })
}

$coreCases = @(
    @("01-navigation", "Orientierung", "Kampagnen, Regelwerke und Einstellungen sowie Sitzung, Kampagne und Live sind erkennbar. Geschichte, Figuren, Welt, Begegnungen und Kompendium liegen unter Kampagne.", "Orientation", "Campaigns, Rulesets and Settings, then Session, Campaign and Live are identifiable. Story, Cast, World, Encounters and Compendium are under Campaign."),
    @("02-campaign-home", "Sitzung aus Notizen vorbereiten", "Eine Kampagne laesst sich erstellen oder laden. Eine vorbereitete Sitzung nimmt lose Notizen auf; bewusst ausgewaehlter Text wird zur Szene, waehrend die Originalnotizen erhalten bleiben.", "Prepare a session from notes", "A campaign can be created or loaded. A prepared session holds rough notes; deliberately selected text becomes a scene while the original notes remain."),
    @("03-cast-character", "Figur erstellen", "Eine Figur laesst sich in Figuren erstellen, auswaehlen, pruefen, speichern und nach Neustart wiederfinden.", "Create a character", "A character can be created, selected, inspected, saved, and found again after restart from Cast."),
    @("04-story-live", "Drei-Szenen-Plan vorbereiten", "Neue und bestehende Szenen lassen sich zu einem Plan verknuepfen, umordnen und mit Material verbinden. Ein Bibliotheksbesuch erhaelt Sitzung und Auswahl. Entfernen eines Planeintrags loescht die Szene nicht.", "Prepare a three-scene plan", "New and existing scenes can be linked into a plan, reordered and linked to material. Library visits preserve session and selection. Removing a plan entry does not delete its scene."),
    @("05-world-atlas", "Karte erstellen und bearbeiten", "Beschreiben, Anforderungen pruefen und Entwurf waehlen fuehren zu Bearbeiten, Spielervorschau und Ausgeben. Bearbeiten, Rueckgaengig, Speichern, Neuladen und Export funktionieren. Nation, Hauptstadt, gemeinsamer Fluss sowie Region/Taktik-Atlasdetails bleiben erreichbar. Orts-, Dungeon- und Hoehlengeneratoren sind experimentell markiert.", "Create and edit a map", "Describe, Review requirements and Choose design lead to Edit, Player preview and Export. Editing, Undo, Save, reopen and export work. Nation, capital, shared river and Region/Tactical atlas details remain reachable. Settlement, dungeon and cave generators are labeled experimental."),
    @("06-encounter", "Begegnung vorbereiten", "Karte, Beteiligte und Bereitschaft sind verstaendlich; die vorbereitete Begegnung gelangt ohne zweiten Primaereditor in Live.", "Prepare an encounter", "Map, roster, and readiness are clear; the prepared encounter transfers to Live without a second primary editor."),
    @("07-live-session", "Dieselbe Sitzung durchfuehren", "Die vorbereitete Sitzung behaelt beim Start ihre ID. Szenenfortschritt und alle sechs Live-Phasen samt Audio/Uhr sind bedienbar. Neustart stellt gespeicherten Cursor und Phase mit gestopptem Audio und schwarzer Spieleranzeige wieder her; erneute Freigabe braucht neue Pruefung.", "Run the same prepared session", "Starting retains the prepared session ID. Scene progress and all six Live phases, audio and clock work. Restart restores the committed cursor and phase with audio stopped and player output black; publication needs fresh review."),
    @("08-player-display", "Spieleranzeige", "Die Anzeige laesst sich bewusst aktivieren und zeigt ausschliesslich explizit freigegebene Inhalte.", "Player Display", "The display can be armed deliberately and shows only explicitly shared information."),
    @("09-command-search", "Suche und Befehle", "Strg+K findet eine Entitaet, navigiert zu ihr und fuehrt eine verstaendlich benannte Aktion aus.", "Search and commands", "Ctrl+K finds an entity, navigates to it, and executes a clearly named action."),
    @("10-ruleset-pin", "Regelwerk binden", "Ein veroeffentlichtes Regelwerk laesst sich waehlen; die exakte Versionsbindung bleibt nach Neustart erhalten.", "Pin a ruleset", "A published ruleset can be selected and its exact version pin survives restart."),
    @("11-language-restart", "Sprache und Neustart", "Die gewaehlte Sprache bleibt nach Neustart erhalten; normale Systemtexte der Kernoberflaeche stimmen mit ihr ueberein.", "Language and restart", "The selected language survives restart and normal core-interface system copy matches it."),
    @("12-package-import", "Paketimport", "Vorschau und Abbruch veraendern nichts; bestaetigtes Ersetzen beziehungsweise Kopieren ist klar und die importierte Kampagne laesst sich oeffnen.", "Package import", "Preview and cancel do not mutate data; confirmed replace or copy is clear and the imported campaign can be opened.")
)
foreach ($locale in @("de-DE", "en-US")) {
    foreach ($case in $coreCases) {
        $title = if ($locale -eq "de-DE") { $case[1] } else { $case[3] }
        $expectedResult = if ($locale -eq "de-DE") { $case[2] } else { $case[4] }
        $evidenceRequirement = if ($locale -eq "de-DE") {
            "Mindestens ein Screenshot oder kurzes Video vom Abschlusszustand; private Kampagnendaten unkenntlich machen."
        }
        else {
            "At least one screenshot or short video of the completed state; redact private campaign data."
        }
        Add-ManualCase -Id "core-$($case[0])__$($locale.Replace('-', '_'))" -Priority "P0" -Suite "core-workflow" -Locale $locale `
            -Title $title -ExpectedResult $expectedResult -EvidenceRequirement $evidenceRequirement
    }
}

Add-ManualCase "ops-01-clean-steam-gui-audio" "P0" "installation" "neutral" "Saubere Steam-Installation" `
    "Der exakte Depot-Build startet ueber Steam mit GUI, OpenGL und Audio; das konfigurierte VC++-Redistributable reicht aus." `
    "Video oder Screenshot plus Steam-Build-ID, Windows-Build und GPU-/Audiogeraet in den Notizen."
Add-ManualCase "ops-02-save-reload" "P0" "data-integrity" "neutral" "Speichern und Neustart" `
    "Eine bearbeitete Kampagne bleibt nach Speichern, Beenden und Neustart vollstaendig und referenziell konsistent. Sitzungsentwuerfe werden bei Strg+S und Speichern vor Schliessen einbezogen; Speicherfehler erhalten den Entwurf und blockieren den Wechsel, Abbrechen behaelt ihn und bewusstes Verwerfen laedt beim erneuten Oeffnen nur gespeicherte Inhalte." `
    "Vorher-/Nachher-Screenshots und exportierter Diagnose-/Hashbeleg ohne private Inhalte."
Add-ManualCase "ops-03-backup-restore" "P0" "data-integrity" "neutral" "Backup und Wiederherstellung" `
    "Backup, Pre-Restore-Sicherheitskopie und Wiederherstellung erhalten Kampagne und Verknuepfungen; Abbruch mutiert nichts." `
    "Screenshots der Vorschau und des wiederhergestellten Abschlusszustands."
Add-ManualCase "ops-04-import-export-cancel-replace" "P0" "data-integrity" "neutral" "Export-/Import-Abbruchpfade" `
    "Export, Vorschau, Abbruch, Ersetzen und Kopie funktionieren ohne stille Datenueberschreibung oder rohe IDs." `
    "Screenshots/Video aller Entscheidungen und Abschlusszustaende."
Add-ManualCase "ops-05-update-uninstall-reinstall" "P0" "installation" "neutral" "Update, Deinstallation, Neuinstallation" `
    "Update sowie Deinstallation/Neuinstallation lassen Benutzerdaten unveraendert und starten danach dieselbe Kampagne." `
    "Steam-Build-IDs, Ablaufvideo und Vorher-/Nachher-Nachweis des Benutzerdatenstands."
Add-ManualCase "safety-01-second-window-sentinels" "P0" "player-safety" "neutral" "Spielerfenster mit Geheimnis-Sentinels" `
    "GM-Notizen, Geheimnisse, verdeckte Tokens, unveroeffentlichte Handouts und zukuenftige Verstaerkungen erscheinen nie im Spielerfenster." `
    "Aufnahme beider Fenster mit gut sichtbaren, kuenstlichen Sentinel-Texten."
Add-ManualCase "safety-02-player-png-fog" "P0" "player-safety" "neutral" "Spielersicherer PNG-Export" `
    "Raster- und kontinuierlicher Spielerexport erzwingen Fog und enthalten keine privaten Marker, Pfade oder Notizen." `
    "Beide exportierten PNGs sowie Screenshot der GM-Ausgangslage."
Add-ManualCase "perf-01-tactical-80x80" "P0" "performance" "neutral" "80x80-Taktikkarte" `
    "Hover, Zoom und Kamerabewegung bleiben auf der protokollierten Mindesthardware responsiv; keine dauerhafte UI-Blockade." `
    "Messprotokoll mit Hardware, Aufloesung/DPI, FPS/Frametime und kurzer Capture-Sequenz."
Add-ManualCase "maps-01-editor-export-display-parity" "P0" "map-visual" "neutral" "Kartenparitaet" `
    "Editor, GM-Export und Player Display zeigen dieselben freigegebenen Features massstabsgerecht; Player-Ausgabe bleibt sicher." `
    "Drei Vergleichsbilder desselben Seeds/Rezepts mit sichtbarem Seed und Stil-Snapshot."
Add-ManualCase "access-01-1280-150dpi-keyboard" "P0" "accessibility" "de-DE" "1280x720 bei 150 Prozent" `
    "Alle Primaeraktionen der Kernablaeufe sind sichtbar und per Tastatur erreichbar; Fokus und Text bleiben lesbar." `
    "Echte 150-Prozent-DPI-Screenshots und kurzer Tastaturablauf; simulierte Skalierung reicht nicht."

$results = foreach ($case in $cases) {
    [PSCustomObject][ordered]@{
        CaseId = $case.caseId
        Priority = $case.priority
        Suite = $case.suite
        Locale = $case.locale
        Title = $case.title
        ExpectedResult = $case.expectedResult
        EvidenceRequirement = $case.evidenceRequirement
        Status = "pending"
        Tester = ""
        TestedAtUtc = ""
        Notes = ""
        EvidencePaths = ""
        EvidenceSha256 = ""
    }
}

$manifest = [PSCustomObject][ordered]@{
    schemaVersion = 1
    runId = $RunId
    createdAtUtc = [DateTime]::UtcNow.ToString("o")
    preparedBy = $PreparedBy
    preparationMachine = $MachineLabel
    product = "GM Editor"
    version = [string]$version["version"]
    depotRoot = $depot
    executable = $executable
    executableSha256 = $exeSha256
    checksumManifest = $checksumManifest
    checksumManifestSha256 = $manifestSha256
    expectedCaseCount = $cases.Count
    expectedCases = $cases.ToArray()
    allowedStatuses = @("pending", "pass", "fail", "blocked")
    releaseRule = "Every P0 case must be pass; pending, fail and blocked are release blockers."
}

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$manifestPath = Join-Path $runRoot "manual-qa-run.json"
$resultsPath = Join-Path $runRoot "manual-qa-results.csv"
$readmePath = Join-Path $runRoot "README_DE_EN.md"
[System.IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 8), $utf8NoBom)
$results | Export-Csv -LiteralPath $resultsPath -NoTypeInformation -Encoding UTF8

$readme = @"
# Manueller GM-Editor-Release-Test / Manual GM Editor release test

Dieser Lauf ist an GMEditor.exe mit SHA-256 $exeSha256 gebunden. Bearbeite ausschliesslich manual-qa-results.csv und lege Belege unter evidence/ ab. Fuer pass sind Tester, UTC-Zeit und mindestens ein hashgebundener Beleg erforderlich. pending, fail und blocked bleiben Release-Blocker.

This run is bound to GMEditor.exe SHA-256 $exeSha256. Edit only manual-qa-results.csv and place evidence below evidence/. A pass requires a tester, UTC timestamp, and at least one hash-bound evidence file. pending, fail, and blocked remain release blockers.

Beispiel / Example:

EvidencePaths=evidence/core-01-de.png;evidence/core-01-de.mp4

EvidenceSha256=<64 hex chars>;<64 hex chars>

Validierung / Validation:

powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/assert-release-manual-qa.ps1 -RunRoot "$runRoot" -RequirePass
"@
[System.IO.File]::WriteAllText($readmePath, $readme, $utf8NoBom)

Write-Host "Manual release-QA run prepared."
Write-Host "Run:   $runRoot"
Write-Host "Cases: $($cases.Count) (24 bilingual core workflows + 10 operational P0 gates)"
Write-Host "EXE:   $exeSha256"
Write-Host "Edit:  $resultsPath"
