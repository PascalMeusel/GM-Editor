param(
    [Parameter(Mandatory=$true)][string]$InputRoot,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [string]$BaselineReportPath = "",
    [ValidateRange(0, [int]::MaxValue)][int]$ExpectedShardCount = 0,
    [ValidateRange(0, [int]::MaxValue)][int]$ExpectedRequestedMapCount = 0
)
$ErrorActionPreference = "Stop"
$inputPath = [IO.Path]::GetFullPath($InputRoot)
$outputPath = [IO.Path]::GetFullPath($OutputDirectory)
if (-not (Test-Path -LiteralPath $inputPath -PathType Container)) { throw "QA input root not found: $inputPath" }

function Test-IsChildPath([string]$Candidate, [string]$Parent) {
    $candidatePath = [IO.Path]::GetFullPath($Candidate).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
    $parentPath = [IO.Path]::GetFullPath($Parent).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
    return $candidatePath.StartsWith($parentPath + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)
}

$allReports = @(Get-ChildItem -LiteralPath $inputPath -Recurse -File -Filter generation_qa_report.json |
    Where-Object { -not (Test-IsChildPath $_.FullName $outputPath) })
if ($allReports.Count -eq 0) { throw "No shard reports found under: $inputPath" }

$preloadedReports = @()
foreach ($reportFile in $allReports) {
    try { $content = Get-Content -Raw -LiteralPath $reportFile.FullName | ConvertFrom-Json -ErrorAction Stop }
    catch { throw "Invalid shard report '$($reportFile.FullName)': $($_.Exception.Message)" }
    foreach ($property in @("schema_version", "product_contract", "generator_version", "recipe_version", "style_id", "style_version", "style_snapshot_id", "tier", "requested_map_count", "requested_shard_map_count", "processed_map_count", "shard_index", "shard_count", "quality_attempts", "determinism_checks", "determinism_failures", "serialization_failures", "failure_count", "diversity_passed", "complete", "elapsed_ms", "passed")) {
        if ($null -eq $content.PSObject.Properties[$property]) { throw "Shard report '$($reportFile.FullName)' is missing required property '$property'." }
    }
    $preloadedReports += [pscustomobject]@{ File=$reportFile; Report=$content }
}

if ($ExpectedShardCount -eq 0) {
    $declaredShardCounts = @($preloadedReports | ForEach-Object { [int]$_.Report.shard_count } | Select-Object -Unique)
    if ($declaredShardCounts.Count -ne 1 -or $declaredShardCounts[0] -lt 1) {
        throw "Could not infer a single valid shard count from shard reports. Supply ExpectedShardCount explicitly."
    }
    $ExpectedShardCount = $declaredShardCounts[0]
}

$expectedReportPaths = @()
for ($shard = 0; $shard -lt $ExpectedShardCount; ++$shard) {
    $expectedReportPaths += [IO.Path]::GetFullPath((Join-Path $inputPath (("shard_{0:d3}" -f $shard) + "/generation_qa_report.json")))
}
$actualReportPaths = @($preloadedReports | ForEach-Object { [IO.Path]::GetFullPath($_.File.FullName) })
if ($actualReportPaths.Count -ne $ExpectedShardCount -or @($actualReportPaths | Sort-Object -Unique).Count -ne $actualReportPaths.Count) {
    throw "Expected exactly $ExpectedShardCount unique shard reports, found $($actualReportPaths.Count)."
}
foreach ($expectedPath in $expectedReportPaths) {
    if ($actualReportPaths -notcontains $expectedPath) { throw "Missing expected shard report: $expectedPath" }
}

$loadedReports = @($preloadedReports | Sort-Object { [int]$_.Report.shard_index })
$shardIndexes = @($loadedReports | ForEach-Object { [int]$_.Report.shard_index })
if (@($shardIndexes | Sort-Object -Unique).Count -ne $ExpectedShardCount) { throw "Shard reports contain duplicate shard_index values." }
for ($expectedIndex = 0; $expectedIndex -lt $ExpectedShardCount; ++$expectedIndex) {
    $entry = @($loadedReports | Where-Object { [int]$_.Report.shard_index -eq $expectedIndex })
    if ($entry.Count -ne 1) { throw "Missing shard_index $expectedIndex." }
    if ([int]$entry[0].Report.shard_count -ne $ExpectedShardCount) { throw "Shard $expectedIndex declares shard_count $($entry[0].Report.shard_count), expected $ExpectedShardCount." }
    $expectedPath = $expectedReportPaths[$expectedIndex]
    if ([IO.Path]::GetFullPath($entry[0].File.FullName) -ne $expectedPath) { throw "Shard $expectedIndex report is not in its expected directory: $expectedPath" }
}

$tiers = @($loadedReports | ForEach-Object { [string]$_.Report.tier } | Select-Object -Unique)
if ($tiers.Count -ne 1) { throw "Shard reports disagree on tier: $($tiers -join ', ')." }
$sharedContractFields = @("schema_version", "product_contract", "generator_version", "recipe_version", "style_id", "style_version", "style_snapshot_id", "quality_attempts")
foreach ($property in $sharedContractFields) {
    $values = @($loadedReports | ForEach-Object { [string]$_.Report.$property } | Select-Object -Unique)
    if ($values.Count -ne 1) { throw "Shard reports disagree on $property." }
}
if ([int]$loadedReports[0].Report.schema_version -ne 2 -or [string]$loadedReports[0].Report.product_contract -ne "MapCreationRequest/MapGenerationBatchResult") {
    throw "Shard reports do not satisfy the schema v2 product-contract QA gate."
}
if ([int]$loadedReports[0].Report.generator_version -ne 5 -or [int]$loadedReports[0].Report.recipe_version -ne 3) {
    throw "Shard reports do not use the required V5 generator and recipe versions."
}
if ([string]::IsNullOrWhiteSpace([string]$loadedReports[0].Report.style_id) -or
    [int]$loadedReports[0].Report.style_version -ne 3 -or
    [string]::IsNullOrWhiteSpace([string]$loadedReports[0].Report.style_snapshot_id)) {
    throw "Shard reports are not bound to a complete V3 production style snapshot."
}
$qualityAttempts = [int]$loadedReports[0].Report.quality_attempts
if ($qualityAttempts -notin @(12, 24, 48)) {
    throw "Shard reports do not use a fixed V5 Draft/Balanced/Polished attempt budget."
}
$requestedCounts = @($loadedReports | ForEach-Object { [int64]$_.Report.requested_map_count } | Select-Object -Unique)
if ($requestedCounts.Count -ne 1 -or $requestedCounts[0] -lt 1) { throw "Shard reports disagree on requested_map_count." }
$requestedMapCount = [int64]$requestedCounts[0]
if ($ExpectedRequestedMapCount -gt 0 -and $requestedMapCount -ne $ExpectedRequestedMapCount) {
    throw "Shard reports request $requestedMapCount maps, expected $ExpectedRequestedMapCount."
}
foreach ($entry in $loadedReports) {
    if ($entry.Report.complete -ne $true -or $entry.Report.passed -ne $true) { throw "Shard $($entry.Report.shard_index) reported QA failure; refusing to merge incomplete success." }
    $processed = [int64]$entry.Report.processed_map_count
    if ($processed -lt 0 -or $processed -gt $requestedMapCount) { throw "Shard $($entry.Report.shard_index) has invalid processed_map_count $processed." }
    if ($processed -ne [int64]$entry.Report.requested_shard_map_count) {
        throw "Shard $($entry.Report.shard_index) processed $processed maps but requested $($entry.Report.requested_shard_map_count) for its shard."
    }
    if ([int64]$entry.Report.failure_count -ne 0 -or [int64]$entry.Report.determinism_failures -ne 0 -or [int64]$entry.Report.serialization_failures -ne 0 -or $entry.Report.diversity_passed -ne $true) {
        throw "Shard $($entry.Report.shard_index) declares passed=true but contains failed QA summary metrics."
    }
}
$requestedShardMapCount = [int64](@($loadedReports | ForEach-Object { [int64]$_.Report.requested_shard_map_count } | Measure-Object -Sum).Sum)
if ($requestedShardMapCount -ne $requestedMapCount) {
    throw "Requested shard-map sum $requestedShardMapCount does not match global requested_map_count $requestedMapCount."
}

New-Item -ItemType Directory -Force -Path $outputPath | Out-Null
$allCases = [Collections.Generic.List[object]]::new()
$caseOutput = Join-Path $outputPath "generation_qa_cases.jsonl"
$writer = [IO.StreamWriter]::new($caseOutput, $false, [Text.UTF8Encoding]::new($false))
try {
    foreach ($entry in $loadedReports) {
        $caseFile = Join-Path $entry.File.DirectoryName "generation_qa_cases.jsonl"
        if (-not (Test-Path -LiteralPath $caseFile -PathType Leaf)) { throw "Shard $($entry.Report.shard_index) is missing cases report: $caseFile" }
        $caseCount = 0
        foreach ($line in [IO.File]::ReadLines($caseFile)) {
            if ([string]::IsNullOrWhiteSpace($line)) { continue }
            try { $case = $line | ConvertFrom-Json -ErrorAction Stop }
            catch { throw "Invalid case JSON in '$caseFile': $($_.Exception.Message)" }
            foreach ($property in @("schema_version", "generator_version", "recipe_version", "style_id", "style_version", "style_snapshot_id", "entry_point", "attempts_evaluated", "validation_eligible", "batch_failure", "passed", "id")) {
                if ($null -eq $case.PSObject.Properties[$property]) { throw "Case in '$caseFile' is missing required property '$property'." }
            }
            if ([int]$case.schema_version -ne 2) { throw "Case '$($case.id)' does not use schema version 2." }
            if ([int]$case.generator_version -ne [int]$entry.Report.generator_version -or [int]$case.recipe_version -ne [int]$entry.Report.recipe_version -or
                [string]$case.style_id -ne [string]$entry.Report.style_id -or [string]$case.style_version -ne [string]$entry.Report.style_version -or
                [string]$case.style_snapshot_id -ne [string]$entry.Report.style_snapshot_id) {
                throw "Case '$($case.id)' does not match its shard's generator, recipe, or production style contract."
            }
            if (-not ([string]$case.entry_point).StartsWith("MapCreationRequest/", [StringComparison]::Ordinal) -or
                $case.validation_eligible -ne $true -or [string]$case.batch_failure -ne "none" -or
                [int]$case.attempts_evaluated -ne $qualityAttempts) {
                throw "Case '$($case.id)' violates the V5 product candidate contract."
            }
            $writer.WriteLine($line)
            $allCases.Add($case)
            ++$caseCount
        }
        if ($caseCount -ne [int64]$entry.Report.processed_map_count) {
            throw "Shard $($entry.Report.shard_index) declares $($entry.Report.processed_map_count) processed maps but contains $caseCount cases."
        }
    }
} finally { $writer.Dispose() }
$processedMapCount = [int64]$allCases.Count
if ($processedMapCount -ne $requestedMapCount) {
    throw "Merged case count $processedMapCount does not match requested_map_count $requestedMapCount."
}
$caseIds = @($allCases | ForEach-Object { [string]$_.id })
if (@($caseIds | Sort-Object -Unique).Count -ne $caseIds.Count) { throw "Shard case reports contain duplicate case IDs." }
$failedCases = @($allCases | Where-Object { $_.passed -ne $true })
if ($failedCases.Count -gt 0) { throw "Shard case reports contain $($failedCases.Count) failed cases despite successful shard summaries." }
$groups = @()
foreach ($group in @($allCases | Group-Object group | Sort-Object Name)) {
    $unique = @($group.Group | Select-Object -ExpandProperty output_hash -Unique).Count
    $durations = @($group.Group | Select-Object -ExpandProperty elapsed_us | Sort-Object)
    $p95 = if ($durations.Count -eq 0) { 0 } else { $durations[[Math]::Min($durations.Count - 1, [int][Math]::Floor($durations.Count * 0.95))] }
    $groups += [pscustomobject]@{ name=$group.Name; count=$group.Count; failures=@($group.Group | Where-Object {-not $_.passed}).Count; unique_ratio=[Math]::Round($unique/[double][Math]::Max(1,$group.Count),5); p95_us=$p95 }
}
$failureSignatures = @{}
foreach ($case in @($allCases | Where-Object {-not $_.passed})) {
    $signature = (@($case.invariants | Where-Object {-not $_.passed} | Select-Object -ExpandProperty code | Sort-Object) -join '+')
    if (-not $failureSignatures.ContainsKey($signature)) { $failureSignatures[$signature] = [pscustomobject]@{ signature=$signature; scenario_id=$case.id; group=$case.group; seed=$case.seed } }
}
$performanceComparisons = @()
$performancePassed = $true
if (-not [string]::IsNullOrWhiteSpace($BaselineReportPath)) {
    $baselinePath = [IO.Path]::GetFullPath($BaselineReportPath)
    if (-not (Test-Path -LiteralPath $baselinePath -PathType Leaf)) { throw "Performance baseline report not found: $baselinePath" }
    $baseline = Get-Content -Raw -LiteralPath $baselinePath | ConvertFrom-Json
    foreach ($group in $groups) {
        $baselineGroup = @($baseline.groups | Where-Object name -eq $group.name | Select-Object -First 1)
        if ($baselineGroup.Count -eq 0 -or [double]$baselineGroup[0].p95_us -le 0) { continue }
        $ratio = [double]$group.p95_us / [double]$baselineGroup[0].p95_us
        $passed = $ratio -le 1.20
        if (-not $passed) { $performancePassed = $false }
        $performanceComparisons += [pscustomobject]@{ name=$group.name; baseline_p95_us=[int64]$baselineGroup[0].p95_us; current_p95_us=[int64]$group.p95_us; ratio=[Math]::Round($ratio,4); passed=$passed }
    }
}
$merged = [ordered]@{
    schema_version=2; product_contract="MapCreationRequest/MapGenerationBatchResult"; generator_version=[int]$loadedReports[0].Report.generator_version; recipe_version=[int]$loadedReports[0].Report.recipe_version
    style_id=[string]$loadedReports[0].Report.style_id; style_version=[int]$loadedReports[0].Report.style_version; style_snapshot_id=[string]$loadedReports[0].Report.style_snapshot_id
    tier=[string]$loadedReports[0].Report.tier; requested_map_count=$requestedMapCount; requested_shard_map_count=$requestedShardMapCount
    processed_map_count=$processedMapCount; shard_count=$ExpectedShardCount; quality_attempts=[int]$loadedReports[0].Report.quality_attempts
    determinism_checks=(@($loadedReports | ForEach-Object { $_.Report.determinism_checks } | Measure-Object -Sum).Sum)
    determinism_failures=(@($loadedReports | ForEach-Object { $_.Report.determinism_failures } | Measure-Object -Sum).Sum)
    serialization_failures=(@($loadedReports | ForEach-Object { $_.Report.serialization_failures } | Measure-Object -Sum).Sum)
    failure_count=@($allCases | Where-Object {-not $_.passed}).Count
    diversity_passed=(@($groups | Where-Object { $_.unique_ratio -lt $(if ($_.name -in @('tactical_outdoor','dungeon_cave')) {0.95} else {0.99}) }).Count -eq 0)
    elapsed_ms=(@($loadedReports | ForEach-Object { $_.Report.elapsed_ms } | Measure-Object -Maximum).Maximum); groups=$groups; failure_signatures=@($failureSignatures.Values)
    benchmark_context=[ordered]@{ machine=$env:COMPUTERNAME; os=[Environment]::OSVersion.VersionString; process_architecture=[Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture.ToString() }
    performance_baseline=if ([string]::IsNullOrWhiteSpace($BaselineReportPath)) { $null } else { $baselinePath }
    performance_comparisons=$performanceComparisons; performance_passed=$performancePassed
}
$merged | Add-Member -NotePropertyName passed -NotePropertyValue ($merged.failure_count -eq 0 -and $merged.determinism_failures -eq 0 -and $merged.serialization_failures -eq 0 -and $merged.diversity_passed -and $merged.performance_passed)
$merged | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $outputPath "generation_qa_report.json") -Encoding utf8
Write-Host "Merged $ExpectedShardCount shards and $processedMapCount cases: $outputPath"
