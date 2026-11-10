param(
    [ValidateSet("pr", "nightly", "release")][string]$Tier = "pr",
    [string]$BuildDirectory = "build-release",
    [string]$OutputRoot = "build/map-generation-qa",
    [int]$MapCount = 0,
    [int]$ShardCount = 1,
    [int]$StartSeed = 700000,
    [string]$BaselineReportPath = "",
    [string]$RunId = "",
    [string]$ExecutablePath = "",
    [switch]$ReportOnly
)
$ErrorActionPreference = "Stop"
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
function Resolve-ProjectPath([string]$Path) { if ([IO.Path]::IsPathRooted($Path)) { return [IO.Path]::GetFullPath($Path) }; return [IO.Path]::GetFullPath((Join-Path $repositoryRoot $Path)) }
$buildPath = Resolve-ProjectPath $BuildDirectory
$executable = if (-not [string]::IsNullOrWhiteSpace($ExecutablePath)) {
    Resolve-ProjectPath $ExecutablePath
} else {
    @(
        (Join-Path $buildPath "map_generation_release_qa.exe"),
        (Join-Path $buildPath "Release/map_generation_release_qa.exe"),
        (Join-Path $buildPath "map_generation_release_qa")
    ) | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
}
if ([string]::IsNullOrWhiteSpace($executable)) { throw "Build map_generation_release_qa before running the QA profile." }
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) { throw "QA executable not found: $executable" }
$effectiveShardCount = [Math]::Max(1, $ShardCount)
if ([string]::IsNullOrWhiteSpace($RunId)) {
    # A QA report is release evidence.  Never append fresh output to a run that
    # could already contain stale shard reports from a different executable.
    $runName = "{0}_{1}_{2}_{3}" -f $Tier, $StartSeed, $effectiveShardCount, [Guid]::NewGuid().ToString("N")
} else {
    $runName = $RunId.Trim()
    if ($runName -ne [IO.Path]::GetFileName($runName) -or $runName -in @(".", "..")) {
        throw "RunId must be a single directory name, not a path."
    }
}
$runRoot = Join-Path (Resolve-ProjectPath $OutputRoot) $runName
if (Test-Path -LiteralPath $runRoot) {
    throw "Refusing to reuse an existing QA run directory: $runRoot. Use a new RunId or OutputRoot."
}
New-Item -ItemType Directory -Path $runRoot -ErrorAction Stop | Out-Null
$shardJobs = @()
for ($shard = 0; $shard -lt $effectiveShardCount; ++$shard) {
    $shardDirectory = Join-Path $runRoot ("shard_{0:d3}" -f $shard)
    New-Item -ItemType Directory -Path $shardDirectory -ErrorAction Stop | Out-Null
    $arguments = @("--tier",$Tier,"--out",$shardDirectory,"--start-seed",$StartSeed,"--shard-index",$shard,"--shard-count",$effectiveShardCount)
    if ($MapCount -gt 0) { $arguments += @("--maps",$MapCount) }
    if ($ReportOnly) { $arguments += "--report-only" }
    # Windows PowerShell can lose Process.ExitCode for an asynchronously
    # started process after its native handle has already been released. Run
    # each native shard inside a PowerShell job and return the captured
    # LASTEXITCODE explicitly while preserving parallel shard execution.
    $stdoutPath = Join-Path $shardDirectory "stdout.log"
    $stderrPath = Join-Path $shardDirectory "stderr.log"
    $backgroundJob = Start-Job -ScriptBlock {
        param($Executable, $ExecutableArguments, $StdoutPath, $StderrPath)
        try {
            & $Executable @ExecutableArguments 1> $StdoutPath 2> $StderrPath
            $nativeExitCode = $LASTEXITCODE
            if ($null -eq $nativeExitCode) { $nativeExitCode = if ($?) { 0 } else { 1 } }
            [pscustomobject]@{ ExitCode=[int]$nativeExitCode; ErrorMessage="" }
        } catch {
            [IO.File]::WriteAllText($StderrPath, $_.Exception.ToString(), [Text.UTF8Encoding]::new($false))
            [pscustomobject]@{ ExitCode=1; ErrorMessage=$_.Exception.Message }
        }
    } -ArgumentList $executable, $arguments, $stdoutPath, $stderrPath
    $shardJobs += [pscustomobject]@{ ShardIndex=$shard; Directory=$shardDirectory; Job=$backgroundJob }
}
Wait-Job -Job @($shardJobs | ForEach-Object { $_.Job }) | Out-Null
foreach ($job in $shardJobs) {
    $backgroundJob = $job.Job
    $jobResult = @(Receive-Job -Job $backgroundJob -ErrorAction SilentlyContinue)
    if ($backgroundJob.State -ne "Completed" -or $jobResult.Count -ne 1 -or
        $null -eq $jobResult[0].PSObject.Properties["ExitCode"]) {
        Remove-Job -Job $backgroundJob -Force -ErrorAction SilentlyContinue
        throw "QA shard $($job.ShardIndex) completed without a trustworthy exit code."
    }
    $exitCode = [int]$jobResult[0].ExitCode
    $job | Add-Member -NotePropertyName ExitCode -NotePropertyValue $exitCode
    Remove-Job -Job $backgroundJob -Force -ErrorAction SilentlyContinue
}
$failedJobs = @($shardJobs | Where-Object { $_.ExitCode -ne 0 })
if ($failedJobs.Count -gt 0) {
    $failureSummary = @($failedJobs | ForEach-Object {
        "shard {0} exited {1} (stdout: {2}; stderr: {3})" -f $_.ShardIndex, $_.ExitCode,
            (Join-Path $_.Directory "stdout.log"), (Join-Path $_.Directory "stderr.log")
    }) -join [Environment]::NewLine
    throw "Map-generation QA shard failure:`n$failureSummary"
}

$mergeArguments = @{ InputRoot=$runRoot; OutputDirectory=(Join-Path $runRoot "merged"); ExpectedShardCount=$effectiveShardCount }
if ($MapCount -gt 0) { $mergeArguments.ExpectedRequestedMapCount = $MapCount }
if (-not [string]::IsNullOrWhiteSpace($BaselineReportPath)) { $mergeArguments.BaselineReportPath = (Resolve-ProjectPath $BaselineReportPath) }
& (Join-Path $PSScriptRoot "merge-map-generation-qa.ps1") @mergeArguments
$merged = Get-Content -Raw -LiteralPath (Join-Path $runRoot "merged/generation_qa_report.json") | ConvertFrom-Json
Write-Host "Map generation QA: $($merged.processed_map_count) maps, $($merged.failure_count) failures, passed=$($merged.passed)"
Write-Host "Report: $(Join-Path $runRoot 'merged/generation_qa_report.json')"
if (-not $ReportOnly -and -not [bool]$merged.passed) { exit 2 }
