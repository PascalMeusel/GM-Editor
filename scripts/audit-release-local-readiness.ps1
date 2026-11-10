#requires -Version 5.1

[CmdletBinding(PositionalBinding = $false)]
param(
    [string]$DepotRoot = "dist/steam/windows-x64/GMEditor-Depot",
    [string]$ZipPath = "dist/steam/windows-x64/GMEditor-0.9.0-windows-x64.zip",
    [Parameter(Mandatory = $true)][string]$InstalledSmokeReport,
    [Parameter(Mandatory = $true)][string]$BinaryAuditReport,
    [Parameter(Mandatory = $true)][string]$CTestLog,
    [Parameter(Mandatory = $true)][string]$MapQaReport,
    [ValidateRange(0, 100000)][int]$ExpectedCTestCount = 0,
    [ValidateRange(1, 256)][int]$ExpectedMapQaShardCount = 4,
    [string]$ProductionSnapshotPath = "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3_3/production_style_snapshot.json",
    [string]$ScreenshotManifest = "",
    [string]$MapVisualReview = "",
    [string]$SourceFreezeReport = "",
    [string]$ManualQaReport = "",
    [Parameter(Mandatory = $true)][string]$OutputPath,
    [switch]$RequireTechnicalChain,
    [switch]$RequireLocalReleaseReady
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if ($ExpectedCTestCount -eq 0) {
    throw "ExpectedCTestCount must be supplied explicitly from this candidate's full CTest inventory; no historical default is assumed."
}

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
. (Join-Path $PSScriptRoot "release-evidence-contract.ps1")
function Resolve-ProjectPath([string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) { return [System.IO.Path]::GetFullPath($Path) }
    return [System.IO.Path]::GetFullPath((Join-Path $projectRoot $Path))
}
function Read-JsonFile([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Required JSON evidence is missing: $Path" }
    return Get-Content -LiteralPath $Path -Raw -Encoding UTF8 | ConvertFrom-Json
}
function Add-Finding([System.Collections.Generic.List[object]]$Findings, [string]$Gate, [string]$Status, [string]$Detail) {
    $Findings.Add([PSCustomObject][ordered]@{ gate = $Gate; status = $Status; detail = $Detail })
}

$depot = Resolve-ProjectPath $DepotRoot
$zip = Resolve-ProjectPath $ZipPath
$smokePath = Resolve-ProjectPath $InstalledSmokeReport
$binaryPath = Resolve-ProjectPath $BinaryAuditReport
$ctestPath = Resolve-ProjectPath $CTestLog
$mapQaPath = Resolve-ProjectPath $MapQaReport
$productionSnapshotPath = Resolve-ProjectPath $ProductionSnapshotPath
$output = Resolve-ProjectPath $OutputPath
if ((Test-Path -LiteralPath $output) -or (Test-Path -LiteralPath "$output.sha256")) {
    throw "Output report or hash sidecar already exists and will not be overwritten: $output"
}

$evidenceRolePaths = @{
    installed_smoke = $smokePath
    binary_audit = $binaryPath
    ctest = $ctestPath
    map_qa = $mapQaPath
    production_snapshot_source = $productionSnapshotPath
}
if (-not [string]::IsNullOrWhiteSpace($ScreenshotManifest)) { $evidenceRolePaths.screenshot_manifest = Resolve-ProjectPath $ScreenshotManifest }
if (-not [string]::IsNullOrWhiteSpace($MapVisualReview)) { $evidenceRolePaths.supplemental_map_visual_review = Resolve-ProjectPath $MapVisualReview }
if (-not [string]::IsNullOrWhiteSpace($SourceFreezeReport)) { $evidenceRolePaths.source_freeze = Resolve-ProjectPath $SourceFreezeReport }
if (-not [string]::IsNullOrWhiteSpace($ManualQaReport)) { $evidenceRolePaths.manual_qa = Resolve-ProjectPath $ManualQaReport }
Assert-ReleaseEvidencePathsAreDistinct -RolePaths $evidenceRolePaths -OutputPath $output

$findings = New-Object 'System.Collections.Generic.List[object]'
$technicalProblems = New-Object 'System.Collections.Generic.List[string]'
try {
    & (Join-Path $PSScriptRoot "assert-steam-depot.ps1") -DepotRoot $depot | Out-Host
    Add-Finding $findings "depot" "pass" "Exact runtime allowlist and SHA256SUMS validation passed."
}
catch {
    $technicalProblems.Add("Depot: $($_.Exception.Message)")
    Add-Finding $findings "depot" "fail" $_.Exception.Message
}
try {
    & (Join-Path $PSScriptRoot "assert-steam-zip-matches-depot.ps1") -DepotRoot $depot -ZipPath $zip | Out-Host
    Add-Finding $findings "zip_matches_depot" "pass" "ZIP paths, sizes and SHA-256 values match the depot."
}
catch {
    $technicalProblems.Add("ZIP: $($_.Exception.Message)")
    Add-Finding $findings "zip_matches_depot" "fail" $_.Exception.Message
}

$exePath = Join-Path $depot "GMEditor.exe"
$sumPath = Join-Path $depot "SHA256SUMS.txt"
if (-not (Test-Path -LiteralPath $exePath -PathType Leaf) -or -not (Test-Path -LiteralPath $sumPath -PathType Leaf)) {
    throw "Depot executable or checksum manifest is missing."
}
$exeSha256 = (Get-FileHash -LiteralPath $exePath -Algorithm SHA256).Hash.ToLowerInvariant()
$sumSha256 = (Get-FileHash -LiteralPath $sumPath -Algorithm SHA256).Hash.ToLowerInvariant()

$productionSnapshotContract = $null
$snapshotDistributionStatus = "blocked"
try {
    $productionSnapshotContract = Get-ReleaseProductionSnapshotContract -SnapshotPath $productionSnapshotPath
    $depotSnapshotPath = Join-Path $depot "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3_3/production_style_snapshot.json"
    $depotSnapshotContract = Get-ReleaseProductionSnapshotContract -SnapshotPath $depotSnapshotPath
    if ([string]$depotSnapshotContract.snapshotId -cne [string]$productionSnapshotContract.snapshotId -or
        [string]$depotSnapshotContract.sha256 -cne [string]$productionSnapshotContract.sha256 -or
        [string]$depotSnapshotContract.manifestSha256 -cne [string]$productionSnapshotContract.manifestSha256) {
        throw "Depot and source use different current production snapshots."
    }
    $snapshotDistributionStatus = if ($productionSnapshotContract.releaseEligible -and $productionSnapshotContract.rightsClearanceComplete) { "pass" } else { "blocked" }
    Add-Finding $findings "production_snapshot" "pass" "Source, depot and current FantasyNeutral V3.3 snapshot are byte-bound as '$($productionSnapshotContract.snapshotId)'."
    Add-Finding $findings "production_snapshot_distribution" $snapshotDistributionStatus $(if ($snapshotDistributionStatus -eq "pass") {
        "Snapshot explicitly records release eligibility and completed rights clearance."
    } else {
        "Snapshot is technically active but does not explicitly grant release eligibility and completed rights clearance."
    })
}
catch {
    $technicalProblems.Add("Production snapshot: $($_.Exception.Message)")
    Add-Finding $findings "production_snapshot" "fail" $_.Exception.Message
}

try {
    $ctestEvidence = Assert-ReleaseCTestEvidence -LogPath $ctestPath -ExpectedTestCount $ExpectedCTestCount
    Add-Finding $findings "ctest" "pass" "Exactly $($ctestEvidence.testCount) passed rows and one completed summary; log SHA-256 $($ctestEvidence.sha256). Log contents alone do not bind test execution to this depot executable or source revision."
}
catch {
    $technicalProblems.Add("CTest: $($_.Exception.Message)")
    Add-Finding $findings "ctest" "fail" $_.Exception.Message
}

try {
    $smoke = Read-JsonFile $smokePath
    $smokePass = [string]$smoke.status -eq "passed" -and
        ([string]$smoke.source_executable_sha256).ToLowerInvariant() -eq $exeSha256 -and
        ([string]$smoke.source_checksum_manifest_sha256).ToLowerInvariant() -eq $sumSha256 -and
        [string]$smoke.install_tree_unchanged -eq "passed" -and [string]$smoke.per_user_storage -eq "passed"
    if (-not $smokePass) { throw "Installed-smoke result or artifact hashes do not match the current depot." }
    Add-Finding $findings "installed_depot_smoke" "pass" "Smoke report is passed and bound to the current EXE and checksum manifest."
}
catch {
    $technicalProblems.Add("Installed smoke: $($_.Exception.Message)")
    Add-Finding $findings "installed_depot_smoke" "fail" $_.Exception.Message
}

try {
    $binary = Read-JsonFile $binaryPath
    $binarySnapshots = @($binary.production_assets.snapshots)
    $binaryPass = [string]$binary.gates.technical_binary_and_inventory_audit -eq "pass" -and
        ([string]$binary.binary.sha256).ToLowerInvariant() -eq $exeSha256 -and
        ([string]$binary.depot.checksum_manifest_sha256).ToLowerInvariant() -eq $sumSha256 -and
        [bool]$binary.binary.metadata_pass -and [string]$binary.depot.checksum_validation -eq "pass" -and
        @($binary.imports.unknown).Count -eq 0 -and [bool]$binary.production_assets.integrity_pass -and
        [bool]$binary.production_assets.snapshot_lineage_pass -and $binarySnapshots.Count -eq 4 -and
        [string]$binarySnapshots[3].snapshot_id -ceq [string]$productionSnapshotContract.snapshotId -and
        [string]$binarySnapshots[3].atlas_version -ceq "3.3"
    if (-not $binaryPass) { throw "Binary/inventory audit is not a technical pass for the current depot hashes." }
    Add-Finding $findings "binary_inventory" "pass" "Binary metadata, imports, notices and the complete V3 through V3.3 production snapshot chain match the current depot."
}
catch {
    $technicalProblems.Add("Binary audit: $($_.Exception.Message)")
    Add-Finding $findings "binary_inventory" "fail" $_.Exception.Message
}

try {
    $mapQa = Read-JsonFile $mapQaPath
    if ($null -eq $productionSnapshotContract) { throw "The current production snapshot contract did not validate." }
    $mapQaEvidence = Assert-ReleaseMapQaEvidence -Report $mapQa -SnapshotContract $productionSnapshotContract `
        -ExpectedMapCount 50000 -ExpectedShardCount $ExpectedMapQaShardCount
    Add-Finding $findings "map_semantic_qa" "pass" "Exact schema-V2/V5 50,000-case release gate passed in $($mapQaEvidence.shardCount) shards against snapshot '$($mapQaEvidence.snapshotId)'; this is not visual acceptance."
}
catch {
    $technicalProblems.Add("Map QA: $($_.Exception.Message)")
    Add-Finding $findings "map_semantic_qa" "fail" $_.Exception.Message
}

$screenshotStatus = "missing"
if (-not [string]::IsNullOrWhiteSpace($ScreenshotManifest)) {
    try {
        $screenPath = Resolve-ProjectPath $ScreenshotManifest
        $screen = Read-JsonFile $screenPath
        $sidecar = "$screenPath.sha256"
        if (-not (Test-Path -LiteralPath $sidecar -PathType Leaf)) { throw "Screenshot manifest hash sidecar is missing." }
        $sidecarLine = (Get-Content -LiteralPath $sidecar -Encoding UTF8 | Select-Object -First 1)
        if ($sidecarLine -notmatch '^([0-9a-fA-F]{64})\s{2}FINAL_SCREENSHOT_MANIFEST\.json$') { throw "Screenshot sidecar format is invalid." }
        if ((Get-FileHash -LiteralPath $screenPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $Matches[1].ToLowerInvariant()) { throw "Screenshot manifest hash does not match its sidecar." }
        if (([string]$screen.editorSha256).ToLowerInvariant() -ne $exeSha256 -or [int]$screen.screenshotCount -lt 5 -or @($screen.screenshots).Count -ne [int]$screen.screenshotCount) {
            throw "Screenshot selection is not bound to this executable or contains fewer than five images."
        }
        foreach ($shot in @($screen.screenshots)) {
            $shotPath = Join-Path (Split-Path -Parent $screenPath) ([string]$shot.file)
            if (-not (Test-Path -LiteralPath $shotPath -PathType Leaf) -or
                (Get-FileHash -LiteralPath $shotPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne ([string]$shot.sha256).ToLowerInvariant()) {
                throw "Selected screenshot is missing or changed: $($shot.file)"
            }
        }
        $screenshotStatus = "pass"
        Add-Finding $findings "selected_screenshot_manifest" "pass" "Selected screenshot files and manifest are intact and bound to the current EXE; this is not full UI acceptance."
    }
    catch {
        $screenshotStatus = "fail"
        Add-Finding $findings "selected_screenshot_manifest" "fail" $_.Exception.Message
    }
}
else { Add-Finding $findings "selected_screenshot_manifest" "missing" "No selected screenshot manifest was supplied." }

$mapVisualStatus = "not_supplied"
if (-not [string]::IsNullOrWhiteSpace($MapVisualReview)) {
    $reviewPath = Resolve-ProjectPath $MapVisualReview
    if (-not (Test-Path -LiteralPath $reviewPath -PathType Leaf)) {
        $mapVisualStatus = "invalid"
        Add-Finding $findings "map_visual_supplemental" "invalid" "Supplemental map visual review is missing."
    }
    else {
        $reviewText = Get-Content -LiteralPath $reviewPath -Raw -Encoding UTF8
        $verdictMatch = [regex]::Match($reviewText, '(?im)^Verdict:\s*(pass|fail|pending)\s*$')
        $declaredVerdict = if ($verdictMatch.Success) { $verdictMatch.Groups[1].Value.ToLowerInvariant() } else { "unstructured" }
        $mapVisualStatus = "manual_$declaredVerdict`_unverified"
        Add-Finding $findings "map_visual_supplemental" "manual_only" "Supplemental text declares '$declaredVerdict'. Text-based self-report is never an automated PASS gate; validated manual-QA evidence remains authoritative."
    }
}
else { Add-Finding $findings "map_visual_supplemental" "not_supplied" "No supplemental text review was supplied; this does not replace validated manual QA." }

$sourceStatus = "missing"
$sourceEvidenceBound = $false
if (-not [string]::IsNullOrWhiteSpace($SourceFreezeReport)) {
    try {
        $source = Read-JsonFile (Resolve-ProjectPath $SourceFreezeReport)
        $sourceStatus = ([string]$source.status).ToLowerInvariant()
        if ($sourceStatus -notin @("ready", "blocked")) { throw "Unknown source-freeze status '$sourceStatus'." }
        if ($sourceStatus -eq "ready" -and (-not [bool]$source.captureStable -or [int]$source.dirtyPathCount -ne 0 -or -not [bool]$source.expectedTagAtHead)) {
            throw "Source-freeze report claims ready without a stable clean tagged state."
        }
        if ($sourceStatus -eq "ready") {
            $sourceEvidenceBound = $null -ne $source.artifact -and
                ([string]$source.artifact.executableSha256).ToLowerInvariant() -eq $exeSha256 -and
                ([string]$source.artifact.checksumManifestSha256).ToLowerInvariant() -eq $sumSha256 -and
                ([string]$source.artifact.ctestLogSha256).ToLowerInvariant() -eq (Get-FileHash -LiteralPath $ctestPath -Algorithm SHA256).Hash.ToLowerInvariant() -and
                ([string]$source.artifact.mapQaReportSha256).ToLowerInvariant() -eq (Get-FileHash -LiteralPath $mapQaPath -Algorithm SHA256).Hash.ToLowerInvariant()
            if (-not $sourceEvidenceBound) {
                throw "Ready source report is not bound to the current EXE, checksum manifest, CTest log and map-QA report."
            }
        }
        Add-Finding $findings "source_freeze" $sourceStatus "Source audit is inventory evidence; only ready represents a clean tagged source state."
    }
    catch {
        $sourceStatus = "invalid"
        Add-Finding $findings "source_freeze" "invalid" $_.Exception.Message
    }
}
else { Add-Finding $findings "source_freeze" "missing" "No source-freeze report was supplied." }

$manualStatus = "missing"
if (-not [string]::IsNullOrWhiteSpace($ManualQaReport)) {
    try {
        $manualPath = Resolve-ProjectPath $ManualQaReport
        $manual = Read-JsonFile $manualPath
        $manualSidecar = "$manualPath.sha256"
        if (-not (Test-Path -LiteralPath $manualSidecar -PathType Leaf)) { throw "Manual-QA report hash sidecar is missing." }
        $manualSidecarLine = Get-Content -LiteralPath $manualSidecar -Encoding UTF8 | Select-Object -First 1
        if ($manualSidecarLine -notmatch '^([0-9a-fA-F]{64})\s{2}(.+)$' -or
            $Matches[2] -cne [System.IO.Path]::GetFileName($manualPath) -or
            (Get-FileHash -LiteralPath $manualPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $Matches[1].ToLowerInvariant()) {
            throw "Manual-QA report hash sidecar is invalid."
        }
        $manualRunRoot = Split-Path -Parent $manualPath
        $manualManifestPath = Join-Path $manualRunRoot "manual-qa-run.json"
        $manualResultsPath = Join-Path $manualRunRoot "manual-qa-results.csv"
        if (-not (Test-Path -LiteralPath $manualManifestPath -PathType Leaf) -or
            -not (Test-Path -LiteralPath $manualResultsPath -PathType Leaf) -or
            (Get-FileHash -LiteralPath $manualManifestPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne ([string]$manual.runManifestSha256).ToLowerInvariant() -or
            (Get-FileHash -LiteralPath $manualResultsPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne ([string]$manual.resultsSha256).ToLowerInvariant()) {
            throw "Manual-QA run manifest or results changed after the final report was generated."
        }
        if (([string]$manual.executableSha256).ToLowerInvariant() -ne $exeSha256) { throw "Manual-QA report belongs to a different executable." }
        $manualStatus = if ([bool]$manual.releaseGatePass -and [int]$manual.passCount -eq [int]$manual.targetCount) { "pass" } else { "blocked" }
        Add-Finding $findings "manual_qa" $manualStatus "Manual evidence remains independent from automated technical gates."
    }
    catch {
        $manualStatus = "invalid"
        Add-Finding $findings "manual_qa" "invalid" $_.Exception.Message
    }
}
else { Add-Finding $findings "manual_qa" "missing" "No final manual-QA report was supplied." }

$technicalPass = $technicalProblems.Count -eq 0
$localReady = $technicalPass -and $snapshotDistributionStatus -eq "pass" -and
    $sourceStatus -eq "ready" -and $sourceEvidenceBound -and $manualStatus -eq "pass" -and $screenshotStatus -eq "pass"
$evidenceFiles = New-Object 'System.Collections.Generic.List[object]'
foreach ($entry in @($evidenceRolePaths.GetEnumerator() | Sort-Object Key)) {
    $evidencePath = [IO.Path]::GetFullPath([string]$entry.Value)
    $exists = Test-Path -LiteralPath $evidencePath -PathType Leaf
    $evidenceFiles.Add([PSCustomObject][ordered]@{
        role = [string]$entry.Key
        path = $evidencePath
        exists = $exists
        sha256 = if ($exists) { (Get-FileHash -LiteralPath $evidencePath -Algorithm SHA256).Hash.ToLowerInvariant() } else { "" }
    })
}
$report = [PSCustomObject][ordered]@{
    schemaVersion = 2
    generatedAtUtc = [DateTime]::UtcNow.ToString("o")
    product = "GM Editor"
    executableSha256 = $exeSha256
    checksumManifestSha256 = $sumSha256
    technicalArtifactChain = if ($technicalPass) { "pass" } else { "fail" }
    expectedCTestCount = $ExpectedCTestCount
    ctestProofScope = "log_contents_only; execution/source/depot binding requires separately captured evidence"
    expectedMapQaShardCount = $ExpectedMapQaShardCount
    productionSnapshot = if ($null -eq $productionSnapshotContract) { $null } else { $productionSnapshotContract }
    productionSnapshotDistribution = $snapshotDistributionStatus
    selectedScreenshotEvidence = $screenshotStatus
    mapVisualSupplementalEvidence = $mapVisualStatus
    mapVisualAutomatedGate = "manual_qa_only"
    sourceFreeze = $sourceStatus
    sourceEvidenceBound = $sourceEvidenceBound
    manualQa = $manualStatus
    localReleaseReady = $localReady
    publicReleaseGo = $false
    publicReleaseReason = "Publisher, rights and Steamworks gates are external and cannot be granted by this local audit."
    technicalProblems = $technicalProblems.ToArray()
    evidenceFiles = $evidenceFiles.ToArray()
    findings = $findings.ToArray()
}
[System.IO.Directory]::CreateDirectory((Split-Path -Parent $output)) | Out-Null
[System.IO.File]::WriteAllText($output, ($report | ConvertTo-Json -Depth 7), (New-Object System.Text.UTF8Encoding($false)))
$outputHash = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash.ToLowerInvariant()
[System.IO.File]::WriteAllText("$output.sha256", "$outputHash  $([System.IO.Path]::GetFileName($output))`r`n", (New-Object System.Text.UTF8Encoding($false)))

Write-Host "Technical artifact chain: $($report.technicalArtifactChain.ToUpperInvariant())"
Write-Host "Screenshot selection:     $($report.selectedScreenshotEvidence.ToUpperInvariant())"
Write-Host "Snapshot distribution:    $($report.productionSnapshotDistribution.ToUpperInvariant())"
Write-Host "Map visual supplement:    $($report.mapVisualSupplementalEvidence.ToUpperInvariant())"
Write-Host "Source freeze:            $($report.sourceFreeze.ToUpperInvariant())"
Write-Host "Manual QA:                $($report.manualQa.ToUpperInvariant())"
Write-Host "Local release ready:      $($report.localReleaseReady)"
Write-Host "Public release GO:        False (external authority required)"
Write-Host "Report: $output"

if ($RequireTechnicalChain -and -not $technicalPass) { throw "Technical release artifact chain failed." }
if ($RequireLocalReleaseReady -and -not $localReady) { throw "Local release readiness gate is not complete." }
