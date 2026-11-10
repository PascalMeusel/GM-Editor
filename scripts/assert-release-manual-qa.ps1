#requires -Version 5.1

[CmdletBinding(PositionalBinding = $false)]
param(
    [Parameter(Mandatory = $true)][string]$RunRoot,
    [ValidateSet("All", "P0", "P1")][string]$Priority = "All",
    [switch]$RequireComplete,
    [switch]$RequirePass,
    [string]$OutputReport = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Add-Problem([System.Collections.Generic.List[string]]$Problems, [string]$Message) {
    $Problems.Add($Message)
}
function Get-SafeEvidencePath([string]$Root, [string]$RelativePath) {
    if ([string]::IsNullOrWhiteSpace($RelativePath) -or [System.IO.Path]::IsPathRooted($RelativePath) -or $RelativePath.Contains(':')) {
        throw "Evidence path must be relative: $RelativePath"
    }
    $segments = @($RelativePath.Replace('\', '/').Split('/'))
    if ($segments -contains ".." -or $segments -contains "." -or $segments[0] -cne "evidence") {
        throw "Evidence path contains an unsafe segment: $RelativePath"
    }
    $rootFull = [System.IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
    $candidate = [System.IO.Path]::GetFullPath((Join-Path $rootFull $RelativePath))
    if (-not $candidate.StartsWith($rootFull + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Evidence path escapes the run directory: $RelativePath"
    }
    return $candidate
}

$run = [System.IO.Path]::GetFullPath($RunRoot).TrimEnd('\', '/')
$manifestPath = Join-Path $run "manual-qa-run.json"
$resultsPath = Join-Path $run "manual-qa-results.csv"
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { throw "Run manifest is missing: $manifestPath" }
if (-not (Test-Path -LiteralPath $resultsPath -PathType Leaf)) { throw "Results file is missing: $resultsPath" }

$manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
$rows = @(Import-Csv -LiteralPath $resultsPath)
$expected = @($manifest.expectedCases)
$problems = New-Object 'System.Collections.Generic.List[string]'

if ($rows.Count -ne [int]$manifest.expectedCaseCount -or $expected.Count -ne [int]$manifest.expectedCaseCount) {
    Add-Problem $problems "Case count differs from the immutable run manifest."
}
foreach ($duplicate in @($rows | Group-Object CaseId | Where-Object Count -ne 1)) {
    Add-Problem $problems "Case ID '$($duplicate.Name)' occurs $($duplicate.Count) times."
}

$exePath = [string]$manifest.executable
$sumPath = [string]$manifest.checksumManifest
if (-not (Test-Path -LiteralPath $exePath -PathType Leaf)) {
    Add-Problem $problems "Bound executable is missing: $exePath"
}
elseif ((Get-FileHash -LiteralPath $exePath -Algorithm SHA256).Hash.ToLowerInvariant() -ne ([string]$manifest.executableSha256).ToLowerInvariant()) {
    Add-Problem $problems "Bound executable changed after the manual-QA run was prepared."
}
if (-not (Test-Path -LiteralPath $sumPath -PathType Leaf)) {
    Add-Problem $problems "Bound checksum manifest is missing: $sumPath"
}
elseif ((Get-FileHash -LiteralPath $sumPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne ([string]$manifest.checksumManifestSha256).ToLowerInvariant()) {
    Add-Problem $problems "Bound checksum manifest changed after the manual-QA run was prepared."
}

$validated = New-Object 'System.Collections.Generic.List[object]'
foreach ($expectedCase in $expected) {
    $matches = @($rows | Where-Object { [string]$_.CaseId -ceq [string]$expectedCase.caseId })
    if ($matches.Count -ne 1) {
        Add-Problem $problems "Expected case '$($expectedCase.caseId)' does not occur exactly once."
        continue
    }
    $row = $matches[0]
    $immutableFields = [ordered]@{
        Priority = [string]$expectedCase.priority
        Suite = [string]$expectedCase.suite
        Locale = [string]$expectedCase.locale
        Title = [string]$expectedCase.title
        ExpectedResult = [string]$expectedCase.expectedResult
        EvidenceRequirement = [string]$expectedCase.evidenceRequirement
    }
    foreach ($pair in $immutableFields.GetEnumerator()) {
        $propertyName = [string]$pair.Key
        $actualProperty = $row.PSObject.Properties[$propertyName]
        $actualValue = if ($null -ne $actualProperty) { [string]$actualProperty.Value } else { "" }
        if ($actualValue -cne [string]$pair.Value) {
            Add-Problem $problems "Immutable field '$propertyName' changed for '$($row.CaseId)'."
        }
    }

    $status = ([string]$row.Status).Trim().ToLowerInvariant()
    if ($status -notin @("pending", "pass", "fail", "blocked")) {
        Add-Problem $problems "Invalid status '$($row.Status)' for '$($row.CaseId)'."
        $status = "invalid"
    }
    if ($status -eq "pass") {
        if ([string]::IsNullOrWhiteSpace([string]$row.Tester)) {
            Add-Problem $problems "Pass case '$($row.CaseId)' does not name a tester."
        }
        $parsedTime = [DateTimeOffset]::MinValue
        if ([string]::IsNullOrWhiteSpace([string]$row.TestedAtUtc) -or
            -not [DateTimeOffset]::TryParse([string]$row.TestedAtUtc, [ref]$parsedTime) -or
            -not ([string]$row.TestedAtUtc).Trim().EndsWith("Z", [System.StringComparison]::OrdinalIgnoreCase)) {
            Add-Problem $problems "Pass case '$($row.CaseId)' needs an ISO-8601 UTC timestamp ending in Z."
        }

        $paths = @(([string]$row.EvidencePaths).Split(';') | ForEach-Object { $_.Trim() } | Where-Object { $_ })
        $hashes = @(([string]$row.EvidenceSha256).Split(';') | ForEach-Object { $_.Trim().ToLowerInvariant() } | Where-Object { $_ })
        if ($paths.Count -eq 0 -or $paths.Count -ne $hashes.Count) {
            Add-Problem $problems "Pass case '$($row.CaseId)' needs one SHA-256 value for every evidence path."
        }
        else {
            for ($i = 0; $i -lt $paths.Count; $i++) {
                try {
                    if ($hashes[$i] -notmatch '^[0-9a-f]{64}$') {
                        throw "invalid SHA-256 '$($hashes[$i])'"
                    }
                    $evidence = Get-SafeEvidencePath -Root $run -RelativePath $paths[$i]
                    if (-not (Test-Path -LiteralPath $evidence -PathType Leaf)) {
                        throw "file does not exist"
                    }
                    if (((Get-Item -LiteralPath $evidence).Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                        throw "reparse-point evidence is not allowed"
                    }
                    $actual = (Get-FileHash -LiteralPath $evidence -Algorithm SHA256).Hash.ToLowerInvariant()
                    if ($actual -ne $hashes[$i]) {
                        throw "hash mismatch"
                    }
                }
                catch {
                    Add-Problem $problems "Evidence '$($paths[$i])' for '$($row.CaseId)' is invalid: $($_.Exception.Message)"
                }
            }
        }
    }
    elseif ($status -in @("fail", "blocked") -and [string]::IsNullOrWhiteSpace([string]$row.Notes)) {
        Add-Problem $problems "Case '$($row.CaseId)' with status '$status' must explain the finding in Notes."
    }

    $validated.Add([PSCustomObject]@{
        CaseId = [string]$row.CaseId
        Priority = [string]$row.Priority
        Status = $status
    })
}

$targets = if ($Priority -eq "All") { @($validated.ToArray()) } else { @($validated.ToArray() | Where-Object Priority -eq $Priority) }
$passCount = @($targets | Where-Object Status -eq "pass").Count
$pendingCount = @($targets | Where-Object Status -eq "pending").Count
$failCount = @($targets | Where-Object Status -eq "fail").Count
$blockedCount = @($targets | Where-Object Status -eq "blocked").Count
$invalidCount = @($targets | Where-Object Status -eq "invalid").Count

if ($RequireComplete -and $pendingCount -gt 0) {
    Add-Problem $problems "$pendingCount required case(s) remain pending for priority $Priority."
}
if ($RequirePass -and $passCount -ne $targets.Count) {
    Add-Problem $problems "Only $passCount of $($targets.Count) required case(s) pass for priority $Priority."
}

$report = [PSCustomObject][ordered]@{
    schemaVersion = 1
    generatedAtUtc = [DateTime]::UtcNow.ToString("o")
    runId = [string]$manifest.runId
    executableSha256 = [string]$manifest.executableSha256
    runManifestSha256 = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
    resultsSha256 = (Get-FileHash -LiteralPath $resultsPath -Algorithm SHA256).Hash.ToLowerInvariant()
    priority = $Priority
    targetCount = $targets.Count
    passCount = $passCount
    pendingCount = $pendingCount
    failCount = $failCount
    blockedCount = $blockedCount
    invalidCount = $invalidCount
    evidenceIntegrityPass = ($problems.Count -eq 0)
    releaseGatePass = ($targets.Count -gt 0 -and $passCount -eq $targets.Count -and $problems.Count -eq 0)
    problems = $problems.ToArray()
}

if (-not [string]::IsNullOrWhiteSpace($OutputReport)) {
    $reportPath = [System.IO.Path]::GetFullPath($OutputReport)
    $reportSidecar = "$reportPath.sha256"
    if ((Test-Path -LiteralPath $reportPath) -or (Test-Path -LiteralPath $reportSidecar)) { throw "Output report or sidecar already exists: $reportPath" }
    [System.IO.Directory]::CreateDirectory((Split-Path -Parent $reportPath)) | Out-Null
    [System.IO.File]::WriteAllText($reportPath, ($report | ConvertTo-Json -Depth 5), (New-Object System.Text.UTF8Encoding($false)))
    $reportHash = (Get-FileHash -LiteralPath $reportPath -Algorithm SHA256).Hash.ToLowerInvariant()
    [System.IO.File]::WriteAllText($reportSidecar, "$reportHash  $([System.IO.Path]::GetFileName($reportPath))`r`n", (New-Object System.Text.UTF8Encoding($false)))
}

Write-Host "Manual release QA: $($targets.Count) target(s), $passCount pass, $pendingCount pending, $failCount fail, $blockedCount blocked, $invalidCount invalid."
if ($problems.Count -gt 0) {
    foreach ($problem in $problems) { Write-Error $problem }
    throw "Manual release-QA validation failed with $($problems.Count) problem(s)."
}
Write-Host "Manual release-QA evidence is internally consistent."
