#requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BuildDirectory,
    [Parameter(Mandatory)][string]$OutputRoot,
    [switch]$SkipBuild,
    [switch]$PaceWrites
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$build = [IO.Path]::GetFullPath($BuildDirectory)
$output = [IO.Path]::GetFullPath($OutputRoot)
if ($output.StartsWith($repository + '\', [StringComparison]::OrdinalIgnoreCase) -or $output -eq $repository) {
    throw 'Benchmark data and evidence must be outside the checkout.'
}
if (Test-Path -LiteralPath $output) { throw 'Use a new output directory for each measurement run.' }
if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build-windows-msvc.ps1') -Configuration Release -BuildDir $build -Jobs 4 -ConfigureOnly
    & cmake --build $build --target persistence_benchmark --parallel 4
    if ($LASTEXITCODE) { throw 'Benchmark compilation failed.' }
}
$cache = Get-Content -LiteralPath (Join-Path $build 'CMakeCache.txt')
if (-not ($cache -contains 'CMAKE_BUILD_TYPE:STRING=Release')) { throw 'Measurements require an optimized Release build.' }
$sourceEntry = @($cache | Where-Object { $_.StartsWith('CMAKE_HOME_DIRECTORY:INTERNAL=') })
if ($sourceEntry.Count -ne 1 -or [IO.Path]::GetFullPath($sourceEntry[0].Substring('CMAKE_HOME_DIRECTORY:INTERNAL='.Length)) -ne $repository) {
    throw 'The build directory belongs to a different source checkout.'
}
$competing = @(Get-Process -Name cl,ctest,ninja,map_generation_release_qa,TTRPG_Editor,Runner.Worker -ErrorAction SilentlyContinue)
if ($competing.Count) { throw "Wait for build, test and editor workloads to finish: $($competing.ProcessName -join ', ')" }
$executable = Join-Path $build 'persistence_benchmark.exe'
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) { throw 'Benchmark executable missing.' }
$commit = (git -C $repository rev-parse HEAD).Trim()
if ($LASTEXITCODE) { throw 'Could not identify source commit.' }
$dirty = @(git -C $repository status --porcelain).Count -ne 0
if ($dirty) { throw 'Commit the measured source first so the report identifies a reproducible revision.' }
[IO.Directory]::CreateDirectory($output) | Out-Null
$phases = @('campaign_copy','database_write','registry_rebuild','snapshot','complete_save')
function Get-Quantile([double[]]$Values, [double]$Quantile) {
    $ordered = @($Values | Sort-Object)
    return $ordered[[Math]::Max(0, [int][Math]::Ceiling($ordered.Count * $Quantile) - 1)]
}
$fixtures = [ordered]@{}
foreach ($size in @('small','large')) {
    $fixtureRoot = Join-Path $output $size
    $arguments = @($size, $fixtureRoot)
    if ($PaceWrites) { $arguments += '--pace-writes' }
    & $executable @arguments *> (Join-Path $output "$size.log")
    if ($LASTEXITCODE) { throw "$size persistence fixture failed; see its log." }
    $csv = Join-Path $fixtureRoot 'samples.csv'
    $rows = @(Import-Csv -LiteralPath $csv)
    if ($rows.Count -ne 155) { throw "$size fixture has an incomplete sample inventory." }
    $measurements = [ordered]@{}
    foreach ($phase in $phases) {
        $samples = @($rows | Where-Object phase -eq $phase)
        if ($samples.Count -ne 31) { throw "Expected 31 measured samples for $size/$phase." }
        $durations = @($samples | ForEach-Object { [double]::Parse($_.milliseconds, [Globalization.CultureInfo]::InvariantCulture) })
        $bytes = @($samples | ForEach-Object { [double]$_.process_write_transfer_bytes })
        $measurements[$phase] = [ordered]@{
            samples=31
            milliseconds_p50=(Get-Quantile $durations 0.5)
            milliseconds_p95=(Get-Quantile $durations 0.95)
            process_write_transfer_bytes_p50=(Get-Quantile $bytes 0.5)
            process_write_transfer_bytes_p95=(Get-Quantile $bytes 0.95)
        }
    }
    $fixtures[$size] = [ordered]@{round_trip_passed=$true; phases=$measurements; samples_sha256=(Get-FileHash -LiteralPath $csv).Hash}
}
if (@(Get-Process -Name cl,ctest,ninja,map_generation_release_qa,TTRPG_Editor,Runner.Worker -ErrorAction SilentlyContinue).Count) {
    throw 'Another workload started during measurement. Keep the raw evidence but rerun on an idle host.'
}
$report = [ordered]@{
    schema='gm-editor.persistence-benchmark.v1'
    commit=$commit
    working_tree_dirty=$dirty
    executable_sha256=(Get-FileHash -LiteralPath $executable).Hash
    fixture_source_sha256=(Get-FileHash -LiteralPath (Join-Path $repository 'tools/persistence_benchmark.cpp')).Hash
    completed_utc=[DateTime]::UtcNow.ToString('o')
    configuration='Release'
    warmup_iterations=3
    pace_writes_to_new_timestamp_interval=[bool]$PaceWrites
    quantiles='nearest rank'
    byte_metric='GetProcessIoCounters WriteTransferCount deltas; process writes, not physical disk traffic or file growth'
    cpu=@(Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores,NumberOfLogicalProcessors)
    memory_bytes=(Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory
    toolchain=@($cache | Where-Object { $_ -match '^CMAKE_(CXX_COMPILER|CXX_FLAGS_RELEASE|GENERATOR):' })
    dependencies=@(git -C $repository submodule status --recursive)
    fixtures=$fixtures
}
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $output 'summary.json') -Encoding UTF8
Write-Output "Persistence timing, process-write counts and round trips recorded in $output"
