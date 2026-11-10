[CmdletBinding(PositionalBinding = $false)]
param(
    [Parameter(Mandatory = $true)]
    [string]$QaReportPath,

    [int]$ExpectedAvailableCaseCount = -1,
    [int]$ExpectedCaseOffset = -1,
    [int]$ExpectedCaseLimit = -1,
    [string[]]$ExpectedCases = @(),
    [switch]$AssertProgressFile
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")

if (-not (Test-Path -LiteralPath $QaReportPath)) {
    throw "Batch review JSON not found: $QaReportPath"
}

$report = Get-Content -LiteralPath $QaReportPath -Raw | ConvertFrom-Json
$results = @(ConvertTo-TilesetArray (Get-TilesetProperty $report "results" @()))

if ($ExpectedAvailableCaseCount -ge 0) {
    $actual = [int](Get-TilesetProperty $report "available_case_count" -1)
    if ($actual -ne $ExpectedAvailableCaseCount) {
        throw "Expected available_case_count $ExpectedAvailableCaseCount, found $actual"
    }
}

if ($ExpectedCaseOffset -ge 0) {
    $actual = [int](Get-TilesetProperty $report "case_offset" -1)
    if ($actual -ne $ExpectedCaseOffset) {
        throw "Expected case_offset $ExpectedCaseOffset, found $actual"
    }
}

if ($ExpectedCaseLimit -ge 0) {
    $actual = [int](Get-TilesetProperty $report "case_limit" -1)
    if ($actual -ne $ExpectedCaseLimit) {
        throw "Expected case_limit $ExpectedCaseLimit, found $actual"
    }
}

if ($ExpectedCases.Count -gt 0) {
    $expected = @()
    foreach ($caseGroup in $ExpectedCases) {
        foreach ($caseName in ([string]$caseGroup -split "[,;]")) {
            $trimmed = $caseName.Trim()
            if (-not [string]::IsNullOrWhiteSpace($trimmed)) {
                $expected += $trimmed
            }
        }
    }

    $actualCases = @($results | ForEach-Object { [string](Get-TilesetProperty $_ "case" "") })
    if ($actualCases.Count -ne $expected.Count) {
        throw "Expected $($expected.Count) case(s), found $($actualCases.Count): $($actualCases -join ', ')"
    }

    for ($i = 0; $i -lt $expected.Count; ++$i) {
        if ($actualCases[$i] -ne $expected[$i]) {
            throw "Expected case[$i] '$($expected[$i])', found '$($actualCases[$i])'"
        }
    }
}

if ($AssertProgressFile) {
    $progressPath = Join-Path (Split-Path -Parent $QaReportPath) "progress.json"
    if (-not (Test-Path -LiteralPath $progressPath)) {
        throw "Expected batch review progress file: $progressPath"
    }

    $progress = Get-Content -LiteralPath $progressPath -Raw | ConvertFrom-Json
    $status = [string](Get-TilesetProperty $progress "status" "")
    if ($status -ne "complete") {
        throw "Expected progress status complete, found '$status'"
    }

    $progressTotal = [int](Get-TilesetProperty $progress "total_cases" -1)
    $progressCompleted = [int](Get-TilesetProperty $progress "completed_cases" -1)
    if ($progressTotal -ne $results.Count -or $progressCompleted -ne $results.Count) {
        throw "Expected progress complete count $($results.Count)/$($results.Count), found $progressCompleted/$progressTotal"
    }

    foreach ($field in @("error_count", "warning_count")) {
        $actual = [int](Get-TilesetProperty $progress $field -1)
        if ($actual -lt 0) {
            throw "Expected progress field '$field' to be present and nonnegative"
        }
    }

    if ($ExpectedAvailableCaseCount -ge 0) {
        $actual = [int](Get-TilesetProperty $progress "available_case_count" -1)
        if ($actual -ne $ExpectedAvailableCaseCount) {
            throw "Expected progress available_case_count $ExpectedAvailableCaseCount, found $actual"
        }
    }
    if ($ExpectedCaseOffset -ge 0) {
        $actual = [int](Get-TilesetProperty $progress "case_offset" -1)
        if ($actual -ne $ExpectedCaseOffset) {
            throw "Expected progress case_offset $ExpectedCaseOffset, found $actual"
        }
    }
    if ($ExpectedCaseLimit -ge 0) {
        $actual = [int](Get-TilesetProperty $progress "case_limit" -1)
        if ($actual -ne $ExpectedCaseLimit) {
            throw "Expected progress case_limit $ExpectedCaseLimit, found $actual"
        }
    }
}

Write-Host "Batch review case slice assertion passed"
Write-Host "Report: $QaReportPath"
Write-Host "Cases: $($results.Count); offset $((Get-TilesetProperty $report 'case_offset' 0)); limit $((Get-TilesetProperty $report 'case_limit' '<all>'))"
if ($AssertProgressFile) {
    Write-Host "Progress file: complete"
}
