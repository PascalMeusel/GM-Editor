param(
    [Parameter(Mandatory=$true)][string]$Executable,
    [Parameter(Mandatory=$true)][string]$ExpectedExecutableSha256,
    [Parameter(Mandatory=$true)][string]$OutputRoot,
    [Parameter(Mandatory=$true)][string]$WorkingDirectory,
    [Parameter(Mandatory=$true)][string]$SourceInventory,
    [Parameter(Mandatory=$true)][string]$ResourceInventory,
    [switch]$FinishAllViewports,
    [switch]$NoSnapshots
)
$ErrorActionPreference = 'Stop'
$runOutput = [IO.Path]::GetFullPath($OutputRoot)
if (Test-Path -LiteralPath $runOutput) { throw "Refusing existing evidence directory: $runOutput" }
$executablePath = (Resolve-Path -LiteralPath $Executable).Path
$workingPath = (Resolve-Path -LiteralPath $WorkingDirectory).Path
$sourceEvidence = (Resolve-Path -LiteralPath $SourceInventory).Path
$resourceEvidence = (Resolve-Path -LiteralPath $ResourceInventory).Path
$binaryHash = (Get-FileHash -LiteralPath $executablePath -Algorithm SHA256).Hash
if ($binaryHash -ne $ExpectedExecutableSha256) { throw 'Executable hash differs from the explicitly bound build.' }
New-Item -ItemType Directory -Path $runOutput | Out-Null
$fixtureRoot = Join-Path $runOutput 'run'
$keys = @('TTRPG_DATA_ROOT','TTRPG_DB_PATH','TTRPG_SETTINGS_PATH','TEMP','TMP')
$prior = @{}
foreach ($key in $keys) { $prior[$key] = [Environment]::GetEnvironmentVariable($key,'Process') }
$manifest = [ordered]@{
    start_utc = [DateTime]::UtcNow.ToString('o'); executable = $executablePath; executable_sha256 = $binaryHash
    working_directory = $workingPath; fixture_root = $fixtureRoot; hidden_windows = $true
    source_inventory_sha256 = (Get-FileHash -LiteralPath $sourceEvidence -Algorithm SHA256).Hash
    resource_inventory_sha256 = (Get-FileHash -LiteralPath $resourceEvidence -Algorithm SHA256).Hash
    build_binding = 'Caller supplied reviewed source/resource inventories and required exact executable hash; this wrapper does not build.'
    synchronization = $(if ($FinishAllViewports) { 'diagnostic_finish_all_viewports' } else { 'ordinary_application' })
    durability = $(if ($NoSnapshots) { 'diagnostic_snapshots_disabled' } else { 'default_backups_and_recovery_enabled' })
    watchdog_seconds = 240; exit_code = $null; failure = ''; artifacts = @()
}
$child = $null
try {
    Copy-Item -LiteralPath $sourceEvidence -Destination (Join-Path $runOutput 'source-inventory-evidence.json')
    Copy-Item -LiteralPath $resourceEvidence -Destination (Join-Path $runOutput 'resource-inventory-evidence.json')
    [Environment]::SetEnvironmentVariable('TTRPG_DATA_ROOT',(Join-Path $fixtureRoot 'data'),'Process')
    [Environment]::SetEnvironmentVariable('TTRPG_DB_PATH',(Join-Path $fixtureRoot 'data/fixture.db'),'Process')
    [Environment]::SetEnvironmentVariable('TTRPG_SETTINGS_PATH',(Join-Path $fixtureRoot 'data/settings.ini'),'Process')
    [Environment]::SetEnvironmentVariable('TEMP',(Join-Path $fixtureRoot 'temp'),'Process')
    [Environment]::SetEnvironmentVariable('TMP',(Join-Path $fixtureRoot 'temp'),'Process')
    $manifest.isolated_environment = @{}
    foreach ($key in $keys) { $manifest.isolated_environment[$key] = [Environment]::GetEnvironmentVariable($key,'Process') }
    $manifest.cpu = @(Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores,NumberOfLogicalProcessors)
    $manifest.gpu_driver = @(Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion)
    $manifest.os = @(Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version,BuildNumber)
    $arguments = '"' + $fixtureRoot + '"'
    if ($FinishAllViewports) { $arguments += ' --finish-all-viewports' }
    if ($NoSnapshots) { $arguments += ' --no-snapshots' }
    $child = Start-Process -FilePath $executablePath -ArgumentList $arguments -WorkingDirectory $workingPath -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $runOutput 'stdout.log') -RedirectStandardError (Join-Path $runOutput 'stderr.log')
    if (-not $child.WaitForExit(240000)) {
        $child.Kill(); $child.WaitForExit(); throw '240-second child watchdog expired; run incomplete.'
    }
    $child.Refresh(); $manifest.exit_code = $child.ExitCode
} catch { $manifest.failure = $_.Exception.Message }
finally {
    if ($child -and -not $child.HasExited) {
        $child.Kill(); $child.WaitForExit()
        if (-not $manifest.failure) { $manifest.failure = 'Child interrupted before completion.' }
    }
    foreach ($key in $keys) { [Environment]::SetEnvironmentVariable($key,$prior[$key],'Process') }
    $manifest.end_utc = [DateTime]::UtcNow.ToString('o')
    $artifacts = @('stdout.log','stderr.log','run/frames.csv','run/operations.csv','run/review-stages.csv','run/summary.json','run/durability-receipts.jsonl',
        'run/original.png','run/replacement.png','run/ordinary-sentinel.png')
    foreach ($relative in $artifacts) {
        $artifactPath = Join-Path $runOutput $relative
        if (Test-Path -LiteralPath $artifactPath -PathType Leaf) {
            $manifest.artifacts += [ordered]@{ path=$relative; bytes=(Get-Item -LiteralPath $artifactPath).Length
                sha256=(Get-FileHash -LiteralPath $artifactPath -Algorithm SHA256).Hash }
        }
    }
    $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $runOutput 'manifest.json') -Encoding utf8
    if ($child) { $child.Dispose() }
}
if ($manifest.failure) { throw $manifest.failure }
if ($manifest.exit_code -ne 0) { throw "Player performance run returned $($manifest.exit_code); inspect preserved summary and logs." }
Write-Output "Player performance evidence: $runOutput"
