#requires -Version 5.1

[CmdletBinding(PositionalBinding = $false)]
param([string]$RepositoryRoot = "")

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
} else {
    $RepositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
}
. (Join-Path $RepositoryRoot "scripts/release-evidence-contract.ps1")

$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) ("gm-editor-release-contract-" + [Guid]::NewGuid().ToString("N"))
[IO.Directory]::CreateDirectory($temporaryRoot) | Out-Null
$utf8 = New-Object Text.UTF8Encoding($false)

function Write-TestText([string]$Path, [string]$Text) {
    [IO.Directory]::CreateDirectory((Split-Path -Parent $Path)) | Out-Null
    [IO.File]::WriteAllText($Path, $Text, $utf8)
}
function Assert-Throws([scriptblock]$Action, [string]$ExpectedFragment) {
    try { & $Action }
    catch {
        if ($_.Exception.Message -notlike "*$ExpectedFragment*") {
            throw "Expected '$ExpectedFragment', got '$($_.Exception.Message)'."
        }
        return
    }
    throw "Expected command to fail, but it succeeded."
}
function Write-CTestFixture([string]$Path, [int]$Count) {
    $lines = New-Object 'System.Collections.Generic.List[string]'
    $lines.Add("Test project C:/fixture")
    for ($index = 1; $index -le $Count; ++$index) {
        $lines.Add(("{0,3}/{1} Test #{0}: release_contract_{0} ........ Passed 0.01 sec" -f $index, $Count))
    }
    $lines.Add("")
    $lines.Add("100% tests passed, 0 tests failed out of $Count")
    $lines.Add("")
    $lines.Add("Total Test time (real) = 1.00 sec")
    Write-TestText $Path (($lines.ToArray() -join "`r`n") + "`r`n")
}
function New-MapQaFixture($SnapshotContract) {
    $groups = foreach ($name in @("world", "region_travel", "city_settlement", "tactical_outdoor", "dungeon_cave")) {
        [PSCustomObject][ordered]@{ name = $name; count = 10000; failures = 0; unique_ratio = 1.0; p95_us = 100 }
    }
    return [PSCustomObject][ordered]@{
        schema_version = 2
        product_contract = "MapCreationRequest/MapGenerationBatchResult"
        generator_version = 5
        recipe_version = 3
        style_id = $SnapshotContract.styleId
        style_version = $SnapshotContract.styleVersion
        style_snapshot_id = $SnapshotContract.snapshotId
        tier = "release"
        requested_map_count = 50000
        requested_shard_map_count = 50000
        processed_map_count = 50000
        shard_count = 4
        quality_attempts = 48
        determinism_checks = 50000
        determinism_failures = 0
        serialization_failures = 0
        failure_count = 0
        diversity_passed = $true
        performance_passed = $true
        passed = $true
        groups = @($groups)
        failure_signatures = @()
    }
}

try {
    $ctestPath = Join-Path $temporaryRoot "ctest.txt"
    Write-CTestFixture -Path $ctestPath -Count 54
    $ctest = Assert-ReleaseCTestEvidence -LogPath $ctestPath -ExpectedTestCount 54
    if ($ctest.testCount -ne 54 -or [string]$ctest.sha256 -notmatch '^[0-9a-f]{64}$') {
        throw "Valid exact CTest evidence did not produce the expected contract."
    }
    Assert-Throws { Assert-ReleaseCTestEvidence -LogPath $ctestPath -ExpectedTestCount 53 } "exactly 53"
    Add-Content -LiteralPath $ctestPath -Value "100% tests passed, 0 tests failed out of 54" -Encoding UTF8
    Assert-Throws { Assert-ReleaseCTestEvidence -LogPath $ctestPath -ExpectedTestCount 54 } "exactly one"

    $snapshotRoot = Join-Path $temporaryRoot "snapshot"
    $manifestPath = Join-Path $snapshotRoot "pack_manifest.json"
    $assetPath = Join-Path $snapshotRoot "asset.png"
    Write-TestText $assetPath "fixture-production-asset`r`n"
    $assetHash = Get-ReleaseEvidenceFileSha256 -Path $assetPath
    $manifestFixture = [ordered]@{
        schema_version = 1; style_id = "fantasy_neutral"; style_version = 3
        atlas_version = "3.3"; snapshot_id = "fantasy_neutral_v3_3_fixture"
        base_snapshot_id = "fantasy_neutral_v3_2_fixturebase"; asset_count = 1; accepted_count = 1
        candidate_count = 0; needs_repaint_count = 0
        assets = @([ordered]@{ id = "fixture.asset"; file = "asset.png"; sha256 = $assetHash; status = "accepted" })
    }
    Write-TestText $manifestPath ($manifestFixture | ConvertTo-Json -Depth 6)
    $manifestHash = Get-ReleaseEvidenceFileSha256 -Path $manifestPath
    $snapshotPath = Join-Path $snapshotRoot "production_style_snapshot.json"
    $snapshotFixture = [ordered]@{
        schema_version = 1; state = "production"; style_id = "fantasy_neutral"; style_version = 3
        atlas_version = "3.3"; snapshot_id = "fantasy_neutral_v3_3_fixture"; base_snapshot_id = "fantasy_neutral_v3_2_fixturebase"
        asset_count = 1; manifest_sha256 = $manifestHash; source_manifest_sha256 = $manifestHash; release_eligible = $true
        distribution = [ordered]@{ rights_clearance_complete = $true }
        assets = @([ordered]@{ id = "fixture.asset"; file = "asset.png"; sha256 = $assetHash; status = "accepted" })
    }
    Write-TestText $snapshotPath ($snapshotFixture | ConvertTo-Json -Depth 6)
    $snapshot = Get-ReleaseProductionSnapshotContract -SnapshotPath $snapshotPath
    if ($snapshot.snapshotId -cne "fantasy_neutral_v3_3_fixture" -or $snapshot.assetCount -ne 1 -or
        -not $snapshot.releaseEligible -or -not $snapshot.rightsClearanceComplete) {
        throw "Valid V3.3 production snapshot did not produce the expected contract."
    }
    $snapshotFixture.atlas_version = "3.2"
    Write-TestText $snapshotPath ($snapshotFixture | ConvertTo-Json -Depth 6)
    Assert-Throws { Get-ReleaseProductionSnapshotContract -SnapshotPath $snapshotPath } "canonical FantasyNeutral V3.3"
    $snapshotFixture.atlas_version = "3.3"
    Write-TestText $snapshotPath ($snapshotFixture | ConvertTo-Json -Depth 6)
    $unmanifestedPath = Join-Path $snapshotRoot "asset-provenance.json"
    Write-TestText $unmanifestedPath "{}`r`n"
    Assert-Throws { Get-ReleaseProductionSnapshotContract -SnapshotPath $snapshotPath } "unmanifested file"
    Remove-Item -LiteralPath $unmanifestedPath -Force

    $sourceV3Root = Join-Path $RepositoryRoot "Resources/Maps/Tilesets/Generated/FantasyNeutral"
    $sourceSnapshots = @{}
    foreach ($directoryName in @("v3", "v3_1", "v3_2", "v3_3")) {
        $sourceSnapshotPath = Join-Path $sourceV3Root "$directoryName/production_style_snapshot.json"
        if (-not (Test-Path -LiteralPath $sourceSnapshotPath -PathType Leaf)) {
            throw "Source production snapshot is missing: $sourceSnapshotPath"
        }
        $sourceSnapshots[$directoryName] = Get-Content -LiteralPath $sourceSnapshotPath -Raw -Encoding UTF8 | ConvertFrom-Json
    }
    if ([string]$sourceSnapshots.v3_1.atlas_version -cne "3.1" -or
        [string]$sourceSnapshots.v3_1.base_snapshot_id -cne [string]$sourceSnapshots.v3.snapshot_id -or
        [string]$sourceSnapshots.v3_2.atlas_version -cne "3.2" -or
        [string]$sourceSnapshots.v3_2.base_snapshot_id -cne [string]$sourceSnapshots.v3_1.snapshot_id -or
        [string]$sourceSnapshots.v3_3.atlas_version -cne "3.3" -or
        [string]$sourceSnapshots.v3_3.base_snapshot_id -cne [string]$sourceSnapshots.v3_2.snapshot_id) {
        throw "Source FantasyNeutral V3 through V3.3 snapshot lineage is inconsistent."
    }
    $sourceCurrent = Get-ReleaseProductionSnapshotContract -SnapshotPath (Join-Path $sourceV3Root "v3_3/production_style_snapshot.json")
    if ($sourceCurrent.snapshotId -cne [string]$sourceSnapshots.v3_3.snapshot_id -or $sourceCurrent.assetCount -ne 4) {
        throw "Current source V3.3 snapshot did not pass the strict accepted-asset contract."
    }

    $qa = New-MapQaFixture $snapshot
    $qaContract = Assert-ReleaseMapQaEvidence -Report $qa -SnapshotContract $snapshot -ExpectedMapCount 50000 -ExpectedShardCount 4
    if ($qaContract.mapCount -ne 50000 -or $qaContract.snapshotId -cne $snapshot.snapshotId) {
        throw "Valid map-QA evidence did not produce the expected contract."
    }
    $qa.schema_version = 1
    Assert-Throws { Assert-ReleaseMapQaEvidence -Report $qa -SnapshotContract $snapshot } "schema-V2 V5"
    $qa.schema_version = 2
    $qa.style_snapshot_id = "stale_snapshot"
    Assert-Throws { Assert-ReleaseMapQaEvidence -Report $qa -SnapshotContract $snapshot } "current FantasyNeutral"
    $qa.style_snapshot_id = $snapshot.snapshotId
    $qa.processed_map_count = 49999
    Assert-Throws { Assert-ReleaseMapQaEvidence -Report $qa -SnapshotContract $snapshot } "exact release tier"
    $qa.processed_map_count = 50000
    $qa.passed = "true"
    Assert-Throws { Assert-ReleaseMapQaEvidence -Report $qa -SnapshotContract $snapshot } "not exactly true"

    $one = Join-Path $temporaryRoot "one.json"
    $two = Join-Path $temporaryRoot "two.json"
    Write-TestText $one "{}"
    Write-TestText $two "{}"
    Assert-ReleaseEvidencePathsAreDistinct -RolePaths @{ one = $one; two = $two } -OutputPath (Join-Path $temporaryRoot "out.json")
    Assert-Throws {
        Assert-ReleaseEvidencePathsAreDistinct -RolePaths @{ one = $one; duplicate = $one } -OutputPath (Join-Path $temporaryRoot "out.json")
    } "same file"
    Assert-Throws {
        Assert-ReleaseEvidencePathsAreDistinct -RolePaths @{ output_reused = (Join-Path $temporaryRoot "out.json") } -OutputPath (Join-Path $temporaryRoot "out.json")
    } "may not reuse"

    $auditText = Get-Content -LiteralPath (Join-Path $RepositoryRoot "scripts/audit-release-local-readiness.ps1") -Raw -Encoding UTF8
    if ($auditText -match '\$mapVisualStatus\s+-eq\s+["'']pass["'']' -or
        $auditText -notmatch 'mapVisualAutomatedGate\s*=\s*["'']manual_qa_only["'']') {
        throw "Local readiness must never treat a text-only map verdict as an automated PASS gate."
    }

    $acceptanceText = Get-Content -LiteralPath (Join-Path $RepositoryRoot "scripts/assert-release-acceptance.ps1") -Raw -Encoding UTF8
    foreach ($requiredPattern in @(
        'assert-release-manual-qa\.ps1[\s\S]*-Priority All[\s\S]*-RequireComplete[\s\S]*-RequirePass',
        'assert-release-visual-smoke\.ps1[\s\S]*-Priority All[\s\S]*-RequireComplete[\s\S]*-RequirePass',
        'sourceStatus\.Count\s+-ne\s+0',
        'visual\.sourceCommit',
        'manual\.executableSha256',
        'visual\.editorSha256')) {
        if ($acceptanceText -notmatch $requiredPattern) {
            throw "Release acceptance lost required cross-evidence contract: $requiredPattern"
        }
    }

    $acceptanceWorkflow = Get-Content -LiteralPath (Join-Path $RepositoryRoot ".github/workflows/release-acceptance.yml") -Raw -Encoding UTF8
    foreach ($requiredPattern in @(
        'environment:\s*release-acceptance',
        'ref:\s*\$\{\{ inputs\.candidate_sha \}\}',
        'assert-release-acceptance\.ps1')) {
        if ($acceptanceWorkflow -notmatch $requiredPattern) {
            throw "Release-acceptance workflow lost required gate wiring: $requiredPattern"
        }
    }

    $portableWorkflow = Get-Content -LiteralPath (Join-Path $RepositoryRoot ".github/workflows/portable-core.yml") -Raw -Encoding UTF8
    foreach ($requiredPattern in @(
        'runs-on:\s*windows-2025',
        'runs-on:\s*ubuntu-24\.04',
        '-Targets\s+TTRPG_Editor,',
        'TTRPG_BUILD_TESTS=ON',
        'map_png_export_planning_tests',
        'map_png_export_planning')) {
        if ($portableWorkflow -notmatch $requiredPattern) {
            throw "Portable-core workflow lost required independent coverage: $requiredPattern"
        }
    }

    Write-Host "release evidence contract tests passed"
}
finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        Remove-Item -LiteralPath $temporaryRoot -Recurse -Force
    }
}
