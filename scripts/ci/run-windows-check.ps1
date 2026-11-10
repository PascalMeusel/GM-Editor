#requires -Version 5.1
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateSet('Debug','Release','Package')][string]$Check,
      [Parameter(Mandatory)][string]$BuildRoot,
      [Parameter(Mandatory)][string]$EvidenceRoot,
      [switch]$FullDebug)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($env:GITHUB_ACTIONS -eq 'true') {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    if ($principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator) -or $identity.Name -notlike '*\GmEditorCI') {
        throw 'CI must run under the dedicated non-admin GmEditorCI service identity.'
    }
}
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$buildBase = [IO.Path]::GetFullPath($BuildRoot)
$evidence = [IO.Path]::GetFullPath($EvidenceRoot)
[IO.Directory]::CreateDirectory($evidence) | Out-Null
$initialStatus = @(git -C $projectRoot status --porcelain=v1 --untracked-files=all)
if ($LASTEXITCODE -or $initialStatus.Count) {
    $initialStatus | Write-Output
    throw 'CI requires a clean source checkout.'
}
$sha = (git -C $projectRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE) { throw 'Could not identify source revision.' }
function Free-GiB([string]$Path) {
    $drive=New-Object IO.DriveInfo([IO.Path]::GetPathRoot([IO.Path]::GetFullPath($Path)))
    return $drive.AvailableFreeSpace / 1GB
}
# Reserve build headroom above the user's 50 GiB floor. Only the established
# disposable CI build root is eligible for automatic pruning; never personal
# campaigns, the runner installation, verification evidence or owner backups.
if((Free-GiB $buildBase) -lt 58) {
    & (Join-Path $PSScriptRoot 'prune-local-builds.ps1') -KeepCommit $sha -KeepPrevious 0
    if((Free-GiB $buildBase) -lt 58) {
        if(-not(Test-Path -LiteralPath 'F:\') -or (Free-GiB 'F:\') -lt 58) {throw 'Need at least 50 GiB free plus 8 GiB build reserve after project cleanup; F: is unavailable or full.'}
        $buildBase=Join-Path 'F:\GMEditorCI\builds' $sha
        [IO.Directory]::CreateDirectory($buildBase)|Out-Null
    }
}
$configuration = if ($Check -eq 'Package') { 'Release' } else { $Check }
$build = Join-Path $buildBase $configuration
function Assert-NoTestRuntimeFilesInSource {
    foreach ($relative in @('build','Resources/Campaigns','Resources/Backups','Resources/Recovery')) {
        $directory = Join-Path $projectRoot $relative
        if (Test-Path -LiteralPath $directory -PathType Container) {
            $file = Get-ChildItem -LiteralPath $directory -Recurse -File -Force | Select-Object -First 1
            if ($file) { throw "Test/runtime output was written into the source checkout: $($file.FullName)" }
        }
    }
}
Assert-NoTestRuntimeFilesInSource
try {
    # Fail before an expensive build if a checkout has altered integrity-bound assets.
    & (Join-Path $projectRoot 'scripts/test-release-evidence-contract.ps1') -RepositoryRoot $projectRoot
    if($Check -eq 'Package') {
        # Package the exact Release executable already tested in this run.
        # Reject stale or missing evidence rather than configuring/building again.
        $releasePath=Join-Path $evidence 'Release-manifest.json'
        if(-not(Test-Path -LiteralPath $releasePath)){throw 'Package requires this run''s successful Release manifest.'}
        $release=Get-Content -LiteralPath $releasePath -Raw|ConvertFrom-Json
        $exe=Join-Path $build 'TTRPG_Editor.exe'
        if($release.commit -ne $sha -or $release.build_directory -ne $build -or
           -not(Test-Path -LiteralPath $exe) -or
           (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash -ne $release.executable_sha256) {
            throw 'Package Release revision, directory or executable hash is stale.'
        }
    } else {
        & (Join-Path $projectRoot 'scripts/build-windows-msvc.ps1') -Configuration $configuration -BuildDir $build -Jobs 4
    }
    if ($Check -ne 'Package') {
        $report = Join-Path $evidence "$Check-ctest.xml"
        # Per-test storage is isolated in CMake; declared producer/report
        # dependencies still order dependent cases. Share the four-worker cap.
        $testArguments=@('--test-dir',$build,'--output-on-failure','--parallel','4','--no-tests=error','--output-junit',$report)
        if($Check -eq 'Debug' -and -not $FullDebug) {$testArguments+=@('--label-exclude','extended-debug')}
        & ctest @testArguments
        if ($LASTEXITCODE) { throw "$Check CTest suite failed." }
    } else {
        $relativeOutput = "dist/engineering-$sha"
        & (Join-Path $projectRoot 'scripts/package-steam-windows.ps1') -BuildDirectory $build -SteamOutputDirectory $relativeOutput -EngineeringCandidate -CreateZip
        $depot = Join-Path $projectRoot "$relativeOutput/GMEditor-Depot"
        & (Join-Path $projectRoot 'scripts/smoke-steam-depot-installed.ps1') -DepotRoot $depot -ExpectedPackageKind engineering_candidate -ReportPath (Join-Path $evidence 'installed-smoke.json')
        Copy-Item -LiteralPath (Join-Path $projectRoot $relativeOutput) -Destination (Join-Path $evidence 'package') -Recurse -Force
    }
    $manifest = [ordered]@{ commit=$sha; check=$Check; configuration=$configuration; public_release_approved=$false; completed_utc=[DateTime]::UtcNow.ToString('o'); submodules=@(git -C $projectRoot submodule status --recursive) }
    $manifest.test_profile=if($Check -eq 'Debug' -and -not $FullDebug){'focused-debug; exhaustive generator workloads remain in full Release and manual full Debug'}elseif($Check -eq 'Package'){'installed-package'}else{'complete'}
    $manifest.build_directory=$build
    $manifest.free_gib_after=[Math]::Round((Free-GiB $buildBase),2)
    if($manifest.free_gib_after -lt 50) {throw 'Build exceeded reserved disk headroom; stop and clean before any further job.'}
    $exe = Join-Path $build 'TTRPG_Editor.exe'
    if (Test-Path -LiteralPath $exe) { $manifest.executable_sha256 = (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash }
    $manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $evidence "$Check-manifest.json") -Encoding UTF8
} finally {
    Assert-NoTestRuntimeFilesInSource
    if (@(git -C $projectRoot status --porcelain=v1 --untracked-files=all).Count) { throw 'Build/tests modified the source checkout.' }
}
