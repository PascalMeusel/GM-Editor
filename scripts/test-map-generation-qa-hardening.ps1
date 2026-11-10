param(
    [string]$RepositoryRoot = ""
)

$ErrorActionPreference = "Stop"
if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) {
    $RepositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
} else {
    $RepositoryRoot = [IO.Path]::GetFullPath($RepositoryRoot)
}

$mergeScript = Join-Path $RepositoryRoot "scripts/merge-map-generation-qa.ps1"
$runScript = Join-Path $RepositoryRoot "scripts/run-map-generation-qa.ps1"
if (-not (Test-Path -LiteralPath $mergeScript -PathType Leaf)) { throw "Merge script not found: $mergeScript" }
if (-not (Test-Path -LiteralPath $runScript -PathType Leaf)) { throw "Run script not found: $runScript" }

$temporaryRoot = Join-Path ([IO.Path]::GetTempPath()) ("gm-editor-map-qa-hardening-" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $temporaryRoot -ErrorAction Stop | Out-Null

function Assert-Throws([scriptblock]$Action, [string]$ExpectedFragment) {
    try {
        & $Action
    } catch {
        if (-not [string]::IsNullOrWhiteSpace($ExpectedFragment) -and $_.Exception.Message -notlike "*$ExpectedFragment*") {
            throw "Expected failure containing '$ExpectedFragment', got '$($_.Exception.Message)'"
        }
        return
    }
    throw "Expected command to fail, but it succeeded."
}

function Write-TextUtf8([string]$Path, [string]$Text) {
    [IO.File]::WriteAllText($Path, $Text, [Text.UTF8Encoding]::new($false))
}

function Write-QAShard(
    [string]$Root,
    [int]$ShardIndex,
    [int]$ShardCount,
    [int]$RequestedCount,
    [int]$ProcessedCount,
    [bool]$Passed = $true,
    [string]$DirectoryName = ""
) {
    if ([string]::IsNullOrWhiteSpace($DirectoryName)) { $DirectoryName = "shard_{0:d3}" -f $ShardIndex }
    $directory = Join-Path $Root $DirectoryName
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    $report = [ordered]@{
        schema_version = 2; product_contract = "MapCreationRequest/MapGenerationBatchResult"; generator_version = 5; recipe_version = 3
        style_id = "fantasy_neutral"; style_version = 3; style_snapshot_id = "fantasy_neutral_v3_test"
        tier = "pr"; requested_map_count = $RequestedCount; requested_shard_map_count = $ProcessedCount; processed_map_count = $ProcessedCount
        shard_index = $ShardIndex; shard_count = $ShardCount; quality_attempts = 12
        determinism_checks = 0; determinism_failures = 0; serialization_failures = 0
        failure_count = if ($Passed) { 0 } else { 1 }; diversity_passed = $Passed; complete = $Passed; elapsed_ms = 1; passed = $Passed
    }
    Write-TextUtf8 (Join-Path $directory "generation_qa_report.json") ($report | ConvertTo-Json -Depth 6)
    $cases = [Collections.Generic.List[string]]::new()
    for ($index = 0; $index -lt $ProcessedCount; ++$index) {
        $case = [ordered]@{
            schema_version = 2; id = "case-$ShardIndex-$index"; group = "world"; band = "valid"
            seed = $ShardIndex * 1000 + $index; output_hash = "hash-$ShardIndex-$index"; elapsed_us = 100
            generator_version = 5; recipe_version = 3; style_id = "fantasy_neutral"; style_version = 3; style_snapshot_id = "fantasy_neutral_v3_test"
            entry_point = "MapCreationRequest/MapGenerationBatchResult"; attempts_evaluated = 12; validation_eligible = $Passed; batch_failure = "none"; passed = $Passed; invariants = @()
        }
        $cases.Add(($case | ConvertTo-Json -Compress -Depth 6))
    }
    Write-TextUtf8 (Join-Path $directory "generation_qa_cases.jsonl") (($cases -join [Environment]::NewLine) + $(if ($cases.Count -gt 0) { [Environment]::NewLine } else { "" }))
}

function Invoke-Merge([string]$InputRoot, [int]$ExpectedShardCount, [int]$ExpectedRequestedMapCount) {
    $output = Join-Path $InputRoot "merged"
    & $mergeScript -InputRoot $InputRoot -OutputDirectory $output -ExpectedShardCount $ExpectedShardCount -ExpectedRequestedMapCount $ExpectedRequestedMapCount
    return Get-Content -Raw -LiteralPath (Join-Path $output "generation_qa_report.json") | ConvertFrom-Json
}

try {
    $validRoot = Join-Path $temporaryRoot "valid"
    Write-QAShard -Root $validRoot -ShardIndex 0 -ShardCount 2 -RequestedCount 10 -ProcessedCount 5
    Write-QAShard -Root $validRoot -ShardIndex 1 -ShardCount 2 -RequestedCount 10 -ProcessedCount 5
    $merged = Invoke-Merge -InputRoot $validRoot -ExpectedShardCount 2 -ExpectedRequestedMapCount 10
    if (-not [bool]$merged.passed -or [int]$merged.processed_map_count -ne 10 -or [int]$merged.shard_count -ne 2) {
        throw "Valid shard reports were not merged as a complete successful run."
    }

    $missingRoot = Join-Path $temporaryRoot "missing"
    Write-QAShard -Root $missingRoot -ShardIndex 0 -ShardCount 2 -RequestedCount 10 -ProcessedCount 5
    Assert-Throws { Invoke-Merge -InputRoot $missingRoot -ExpectedShardCount 2 -ExpectedRequestedMapCount 10 } "Expected exactly 2 unique shard reports"

    $duplicateRoot = Join-Path $temporaryRoot "duplicate"
    Write-QAShard -Root $duplicateRoot -ShardIndex 0 -ShardCount 2 -RequestedCount 10 -ProcessedCount 5
    Write-QAShard -Root $duplicateRoot -ShardIndex 1 -ShardCount 2 -RequestedCount 10 -ProcessedCount 5
    Write-QAShard -Root $duplicateRoot -ShardIndex 0 -ShardCount 2 -RequestedCount 10 -ProcessedCount 5 -DirectoryName "extra_copy"
    Assert-Throws { Invoke-Merge -InputRoot $duplicateRoot -ExpectedShardCount 2 -ExpectedRequestedMapCount 10 } "Expected exactly 2 unique shard reports"

    $failedRoot = Join-Path $temporaryRoot "failed"
    Write-QAShard -Root $failedRoot -ShardIndex 0 -ShardCount 2 -RequestedCount 10 -ProcessedCount 5
    Write-QAShard -Root $failedRoot -ShardIndex 1 -ShardCount 2 -RequestedCount 10 -ProcessedCount 5 -Passed $false
    Assert-Throws { Invoke-Merge -InputRoot $failedRoot -ExpectedShardCount 2 -ExpectedRequestedMapCount 10 } "reported QA failure"

    $partialRoot = Join-Path $temporaryRoot "partial"
    Write-QAShard -Root $partialRoot -ShardIndex 0 -ShardCount 2 -RequestedCount 10 -ProcessedCount 4
    Write-QAShard -Root $partialRoot -ShardIndex 1 -ShardCount 2 -RequestedCount 10 -ProcessedCount 4
    Assert-Throws { Invoke-Merge -InputRoot $partialRoot -ExpectedShardCount 2 -ExpectedRequestedMapCount 10 } "Requested shard-map sum"

    $mockSuccess = Join-Path $temporaryRoot "mock-success.ps1"
    $mockSuccessSource = @'
$ErrorActionPreference = "Stop"
$values = @{}
for ($index = 0; $index -lt $args.Count; ++$index) {
    if ([string]$args[$index] -notlike "--*") { continue }
    $key = [string]$args[$index]
    if ($index + 1 -lt $args.Count -and [string]$args[$index + 1] -notlike "--*") {
        $values[$key] = [string]$args[++$index]
    }
}
$output = [string]$values["--out"]
$mapCount = if ($values.ContainsKey("--maps")) { [int]$values["--maps"] } else { 500 }
$shardIndex = [int]$values["--shard-index"]
$shardCount = [int]$values["--shard-count"]
$processed = 0
$caseLines = [Collections.Generic.List[string]]::new()
for ($globalIndex = 0; $globalIndex -lt $mapCount; ++$globalIndex) {
    if ($globalIndex % $shardCount -ne $shardIndex) { continue }
    ++$processed
    $case = [ordered]@{
        schema_version=2; id="mock-$globalIndex"; group="world"; band="broad"; seed=$globalIndex
        output_hash="hash-$globalIndex"; elapsed_us=1; generator_version=5; recipe_version=3
        style_id="fantasy_neutral"; style_version=3; style_snapshot_id="mock-production-snapshot"
        entry_point="MapCreationRequest/MapGenerationBatchResult"; attempts_evaluated=12
        validation_eligible=$true; batch_failure="none"; passed=$true; invariants=@()
    }
    $caseLines.Add(($case | ConvertTo-Json -Compress -Depth 5))
}
New-Item -ItemType Directory -Path $output -Force | Out-Null
[IO.File]::WriteAllText((Join-Path $output "generation_qa_cases.jsonl"), ($caseLines -join [Environment]::NewLine) + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
$report = [ordered]@{
    schema_version=2; product_contract="MapCreationRequest/MapGenerationBatchResult"; generator_version=5; recipe_version=3
    style_id="fantasy_neutral"; style_version=3; style_snapshot_id="mock-production-snapshot"; tier="pr"
    requested_map_count=$mapCount; requested_shard_map_count=$processed; processed_map_count=$processed
    shard_index=$shardIndex; shard_count=$shardCount; quality_attempts=12; determinism_checks=0
    determinism_failures=0; serialization_failures=0; failure_count=0; diversity_passed=$true
    complete=$true; elapsed_ms=1; passed=$true
}
[IO.File]::WriteAllText((Join-Path $output "generation_qa_report.json"), ($report | ConvertTo-Json -Depth 5), [Text.UTF8Encoding]::new($false))
exit 0
'@
    Write-TextUtf8 $mockSuccess $mockSuccessSource
    & $runScript -ExecutablePath $mockSuccess -OutputRoot $temporaryRoot -RunId "process-success" -MapCount 10 -ShardCount 2
    $runnerMerged = Get-Content -Raw -LiteralPath (Join-Path $temporaryRoot "process-success/merged/generation_qa_report.json") | ConvertFrom-Json
    if ($runnerMerged.passed -ne $true -or [int]$runnerMerged.processed_map_count -ne 10) {
        throw "Runner did not preserve a complete successful two-shard result."
    }

    $mockFailure = Join-Path $temporaryRoot "mock-failure.cmd"
    Write-TextUtf8 $mockFailure "@echo off`r`nexit /b 17`r`n"
    Assert-Throws {
        & $runScript -ExecutablePath $mockFailure -OutputRoot $temporaryRoot -RunId "process-exit" -MapCount 5 -ShardCount 2
    } "shard 0 exited 17"

    $staleRoot = Join-Path $temporaryRoot "stale"
    New-Item -ItemType Directory -Path $staleRoot | Out-Null
    Assert-Throws {
        & $runScript -ExecutablePath $mockFailure -OutputRoot $temporaryRoot -RunId "stale" -MapCount 5
    } "Refusing to reuse an existing QA run directory"

    Write-Host "map-generation QA script hardening tests passed"
} finally {
    if (Test-Path -LiteralPath $temporaryRoot) { Remove-Item -LiteralPath $temporaryRoot -Recurse -Force }
}
