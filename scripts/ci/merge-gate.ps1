# Pure validation shared by the integration command and offline regression tests.
function Assert-GmMergeGate {
    param([Parameter(Mandatory)]$PullRequest, [Parameter(Mandatory)][string]$ExpectedHead,
          [Parameter(Mandatory)][object[]]$WorkflowRuns)
    if ($PullRequest.headRefOid -ne $ExpectedHead) { throw 'PR head changed; refusing stale validation.' }
    if ($PullRequest.baseRefName -ne 'master' -or $PullRequest.isCrossRepository -or $PullRequest.isDraft) { throw 'Expected a ready same-repository PR targeting master.' }
    if ($PullRequest.mergeStateStatus -ne 'CLEAN') { throw "PR is not cleanly mergeable: $($PullRequest.mergeStateStatus)" }
    foreach ($name in @('windows-debug', 'windows-release', 'windows-package')) {
        $checks = @($PullRequest.statusCheckRollup | Where-Object { $_.name -eq $name })
        if ($checks.Count -ne 1) { throw "Required check must appear exactly once: $name" }
        $check = $checks[0]
        if ($check.status -ne 'COMPLETED' -or $check.conclusion -ne 'SUCCESS') { throw "Required check is not successful: $name" }
        if ($check.detailsUrl -notmatch '/actions/runs/(\d+)(?:/|$)') { throw "Check is not a verifiable Actions run: $name" }
        $runId = $Matches[1]
        $runs = @($WorkflowRuns | Where-Object { [string]$_.id -eq $runId })
        if ($runs.Count -ne 1 -or $runs[0].head_sha -ne $ExpectedHead -or
            $runs[0].status -ne 'completed' -or $runs[0].conclusion -ne 'success' -or
            $runs[0].path -ne '.github/workflows/build.yml' -or $runs[0].event -ne 'pull_request') {
            throw "Required workflow is stale, unsuccessful, or unexpected: $name"
        }
    }
}
