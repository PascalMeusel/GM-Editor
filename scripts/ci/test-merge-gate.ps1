#requires -Version 5.1
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'merge-gate.ps1')
function Fixture {
    [pscustomobject]@{
        headRefOid = ('a' * 40); baseRefName = 'master'; isCrossRepository = $false; isDraft = $false; mergeStateStatus = 'CLEAN'
        statusCheckRollup = @('windows-debug','windows-release','windows-package' | ForEach-Object {
            [pscustomobject]@{ name=$_; status='COMPLETED'; conclusion='SUCCESS'; detailsUrl='https://github.com/PascalMeusel/TTRPG_Editor_cpp/actions/runs/123/job/456' }
        })
    }
}
function RunFixture { [pscustomobject]@{ id=123; head_sha=('a' * 40); status='completed'; conclusion='success'; path='.github/workflows/build.yml'; event='pull_request' } }
function Reject([scriptblock]$Mutation) {
    $pr = Fixture; $run = RunFixture
    & $Mutation $pr $run
    $rejected = $false
    try { Assert-GmMergeGate $pr ('a' * 40) @($run) } catch { $rejected = $true }
    if (-not $rejected) { throw 'Unsafe merge fixture was accepted.' }
}
Assert-GmMergeGate (Fixture) ('a' * 40) @((RunFixture))
Reject { param($pr,$run) $pr.headRefOid = 'b' * 40 }
Reject { param($pr,$run) $pr.statusCheckRollup = @($pr.statusCheckRollup[0]) }
Reject { param($pr,$run) $pr.statusCheckRollup += $pr.statusCheckRollup[0] }
Reject { param($pr,$run) $pr.statusCheckRollup[0].conclusion = 'FAILURE' }
Reject { param($pr,$run) $pr.statusCheckRollup[0].conclusion = 'SKIPPED' }
Reject { param($pr,$run) $pr.statusCheckRollup[0].status = 'IN_PROGRESS' }
Reject { param($pr,$run) $run.head_sha = 'b' * 40 }
Reject { param($pr,$run) $run.conclusion = 'failure' }
Reject { param($pr,$run) $run.path = '.github/workflows/unrelated.yml' }
Reject { param($pr,$run) $pr.mergeStateStatus = 'DIRTY' }
Reject { param($pr,$run) $pr.isCrossRepository = $true }
Reject { param($pr,$run) $pr.isDraft = $true }
Reject { param($pr,$run) $pr.baseRefName = 'other' }
Write-Host 'Merge gate: valid candidate accepted; 13 unsafe/stale cases rejected.'
