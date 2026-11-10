#requires -Version 5.1
[CmdletBinding()]
param([Parameter(Mandatory)][ValidateRange(1,2147483647)][int]$PullRequest)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'merge-gate.ps1')
$repository = 'PascalMeusel/TTRPG_Editor_cpp'
$ghCommand = Get-Command gh -ErrorAction SilentlyContinue
$gh = if ($ghCommand) { $ghCommand.Source } else { 'C:\Program Files\GitHub CLI\gh.exe' }
function Read-Pr {
    $json = & $gh pr view $PullRequest --repo $repository --json headRefOid,baseRefName,isCrossRepository,isDraft,mergeStateStatus,statusCheckRollup
    if ($LASTEXITCODE) { throw 'Could not read PR state.' }
    return $json | ConvertFrom-Json
}
$pr = Read-Pr
$expectedHead = $pr.headRefOid
$runIds = @($pr.statusCheckRollup | ForEach-Object { if ($_.detailsUrl -match '/actions/runs/(\d+)(?:/|$)') { $Matches[1] } } | Select-Object -Unique)
$runs = @($runIds | ForEach-Object {
    $json = & $gh api "repos/$repository/actions/runs/$_"
    if ($LASTEXITCODE) { throw 'Could not verify Actions run.' }
    $json | ConvertFrom-Json
})
Assert-GmMergeGate -PullRequest $pr -ExpectedHead $expectedHead -WorkflowRuns $runs
# Re-read immediately before the atomic head-matching merge. No admin bypass.
Assert-GmMergeGate -PullRequest (Read-Pr) -ExpectedHead $expectedHead -WorkflowRuns $runs
& $gh pr merge $PullRequest --repo $repository --merge --match-head-commit $expectedHead
if ($LASTEXITCODE) { throw 'GitHub rejected the merge.' }
$state = & $gh pr view $PullRequest --repo $repository --json state,mergeCommit
if ($LASTEXITCODE -or ($state | ConvertFrom-Json).state -ne 'MERGED') { throw 'Merge completion could not be verified.' }
$mergeSha=($state | ConvertFrom-Json).mergeCommit.oid
$headCommit=& $gh api "repos/$repository/git/commits/$expectedHead"
if($LASTEXITCODE) {throw 'Could not verify checked PR tree.'}
$mergeCommit=& $gh api "repos/$repository/git/commits/$mergeSha"
if($LASTEXITCODE) {throw 'Could not verify merged master tree.'}
if(($headCommit|ConvertFrom-Json).tree.sha -ne ($mergeCommit|ConvertFrom-Json).tree.sha) {
    throw 'Merged master tree differs from checked PR head; require fresh validation before continuing.'
}
Write-Output 'Merged master has the identical verified PR tree; no duplicate full CI run is required.'
Write-Output $state
