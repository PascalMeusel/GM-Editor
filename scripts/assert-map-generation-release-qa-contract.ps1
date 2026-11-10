param(
    [Parameter(Mandatory=$true)][string]$ReportDirectory,
    [ValidateRange(1, [int]::MaxValue)][int]$ExpectedMapCount = 25,
    [ValidateSet(12, 24, 48)][int]$ExpectedAttempts = 12
)

$ErrorActionPreference = "Stop"
$directory = [IO.Path]::GetFullPath($ReportDirectory)
$reportPath = Join-Path $directory "generation_qa_report.json"
$casesPath = Join-Path $directory "generation_qa_cases.jsonl"
if (-not (Test-Path -LiteralPath $reportPath -PathType Leaf)) { throw "Missing QA report: $reportPath" }
if (-not (Test-Path -LiteralPath $casesPath -PathType Leaf)) { throw "Missing QA cases: $casesPath" }

$report = Get-Content -Raw -LiteralPath $reportPath | ConvertFrom-Json -ErrorAction Stop
$expectedContract = "MapCreationRequest/MapGenerationBatchResult"
if ([int]$report.schema_version -ne 2) { throw "Expected report schema 2." }
if ([string]$report.product_contract -ne $expectedContract) { throw "Unexpected product contract." }
if ([int]$report.generator_version -ne 5 -or [int]$report.recipe_version -ne 3) { throw "Report is not bound to generator V5 / recipe V3." }
if ([string]::IsNullOrWhiteSpace([string]$report.style_id) -or [int]$report.style_version -lt 1 -or
    [string]::IsNullOrWhiteSpace([string]$report.style_snapshot_id)) { throw "Report has no production style binding." }
if ([int]$report.quality_attempts -ne $ExpectedAttempts) { throw "Unexpected V5 attempt budget." }
if ([int]$report.requested_map_count -ne $ExpectedMapCount -or
    [int]$report.requested_shard_map_count -ne $ExpectedMapCount -or
    [int]$report.processed_map_count -ne $ExpectedMapCount) { throw "Report map counts are incomplete." }
if ($report.complete -ne $true -or $report.passed -ne $true) { throw "QA report did not complete successfully." }

$cases = [Collections.Generic.List[object]]::new()
foreach ($line in [IO.File]::ReadLines($casesPath)) {
    if ([string]::IsNullOrWhiteSpace($line)) { continue }
    $cases.Add(($line | ConvertFrom-Json -ErrorAction Stop))
}
if ($cases.Count -ne $ExpectedMapCount) { throw "Expected $ExpectedMapCount cases, found $($cases.Count)." }
if (@($cases | ForEach-Object { [string]$_.id } | Sort-Object -Unique).Count -ne $cases.Count) { throw "Case IDs are not unique." }

$requiredInvariantCodes = @(
    "batch.succeeded",
    "validation.eligible",
    "contract.product_entry_point",
    "contract.generator_version",
    "contract.recipe_version",
    "contract.style_id",
    "contract.style_version",
    "contract.production_snapshot",
    "contract.creation_intent",
    "contract.fixed_attempt_budget",
    "facts.must_fulfilled",
    "facts.excluded_fulfilled",
    "persistence.roundtrip"
)
foreach ($case in $cases) {
    if ([int]$case.schema_version -ne 2 -or [int]$case.generator_version -ne 5 -or [int]$case.recipe_version -ne 3) {
        throw "Case '$($case.id)' is not bound to V5 / recipe V3."
    }
    if ([string]$case.style_id -ne [string]$report.style_id -or [int]$case.style_version -ne [int]$report.style_version -or
        [string]$case.style_snapshot_id -ne [string]$report.style_snapshot_id) {
        throw "Case '$($case.id)' does not match the report style snapshot."
    }
    if (-not ([string]$case.entry_point).StartsWith("MapCreationRequest/", [StringComparison]::Ordinal) -or
        [int]$case.attempts_evaluated -ne $ExpectedAttempts -or $case.validation_eligible -ne $true -or
        [string]$case.batch_failure -ne "none" -or $case.passed -ne $true) {
        throw "Case '$($case.id)' violates the V5 candidate contract."
    }
    $caseInvariantCodes = @($case.invariants | ForEach-Object { [string]$_.code })
    foreach ($code in $requiredInvariantCodes) {
        if ($caseInvariantCodes -notcontains $code) { throw "Case '$($case.id)' is missing invariant '$code'." }
    }
    if (@($case.invariants | Where-Object { $_.passed -ne $true }).Count -ne 0) {
        throw "Case '$($case.id)' contains a failed invariant."
    }
}

$groups = @($cases | ForEach-Object { [string]$_.group } | Sort-Object -Unique)
foreach ($expectedGroup in @("world", "region_travel", "city_settlement", "tactical_outdoor", "dungeon_cave")) {
    if ($groups -notcontains $expectedGroup) { throw "Missing QA group '$expectedGroup'." }
}

Write-Host "V5 release-QA contract verified for $($cases.Count) cases."
