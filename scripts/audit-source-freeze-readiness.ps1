#requires -Version 5.1

[CmdletBinding(PositionalBinding = $false)]
param(
    [string]$ExpectedTag = "v0.9.0",
    [string]$DepotRoot = "dist/steam/windows-x64/GMEditor-Depot",
    [string]$CTestLog = "",
    [string]$MapQaReport = "",
    [string]$OutputRoot = "build/release-evidence/source-freeze",
    [string]$RunId = "",
    [switch]$RequirePushed,
    [switch]$RequireReady
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
function Resolve-ProjectPath([string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) { return [System.IO.Path]::GetFullPath($Path) }
    return [System.IO.Path]::GetFullPath((Join-Path $projectRoot $Path))
}
function Invoke-Git {
    param([Parameter(ValueFromRemainingArguments = $true)][string[]]$Arguments)
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $output = @(& git -C $projectRoot @Arguments 2>&1)
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousPreference
    }
    return [PSCustomObject]@{ Output = $output; ExitCode = $exitCode; Text = ($output -join "`n") }
}
function Test-RelevantPath([string]$RelativePath) {
    $path = $RelativePath.Replace('\', '/').TrimStart('./')
    if ([string]::IsNullOrWhiteSpace($path)) { return $false }
    $first = @($path.Split('/'))[0]
    if ($first -in @(".git", ".vs", "dist", "out", "TestResults", "foundation_test_project_root")) { return $false }
    if ($first -eq "build" -or $first -match '^build[-_]') { return $false }
    return $true
}
function Get-StatusPath([string]$Line) {
    if ($Line.Length -lt 4) { return "" }
    $path = $Line.Substring(3).Trim()
    if ($path.Contains(" -> ")) { $path = $path.Substring($path.LastIndexOf(" -> ") + 4) }
    if ($path.Length -ge 2 -and $path.StartsWith('"') -and $path.EndsWith('"')) {
        # core.quotepath=false keeps ordinary Unicode paths readable. Quoted paths
        # containing unusual control characters remain a release blocker and are
        # intentionally not guessed here.
        $path = $path.Substring(1, $path.Length - 2)
    }
    return $path.Replace('\', '/')
}

$inside = Invoke-Git rev-parse --is-inside-work-tree
if ($inside.ExitCode -ne 0 -or $inside.Text.Trim() -ne "true") { throw "Project root is not a Git working tree: $projectRoot" }

$statusStartResult = Invoke-Git -c core.quotepath=false status --porcelain=v1 --untracked-files=all
if ($statusStartResult.ExitCode -ne 0) { throw "git status failed: $($statusStartResult.Text)" }
$statusStart = @($statusStartResult.Output | ForEach-Object { [string]$_ } | Where-Object { $_ -ne "" })
$headResult = Invoke-Git rev-parse HEAD
$treeResult = Invoke-Git rev-parse 'HEAD^{tree}'
$branchResult = Invoke-Git symbolic-ref --quiet --short HEAD
if ($headResult.ExitCode -ne 0 -or $treeResult.ExitCode -ne 0) { throw "Could not resolve Git HEAD and tree." }
$headCommit = $headResult.Text.Trim()
$headTree = $treeResult.Text.Trim()
$branch = if ($branchResult.ExitCode -eq 0) { $branchResult.Text.Trim() } else { "DETACHED" }

$tagNames = @((Invoke-Git tag --points-at HEAD).Output | ForEach-Object { ([string]$_).Trim() } | Where-Object { $_ } | Sort-Object -Unique)
$expectedTagExists = $false
$expectedTagAtHead = $false
$expectedTagCommit = ""
if (-not [string]::IsNullOrWhiteSpace($ExpectedTag)) {
    $tagResult = Invoke-Git rev-parse "$ExpectedTag^{commit}"
    if ($tagResult.ExitCode -eq 0) {
        $expectedTagExists = $true
        $expectedTagCommit = $tagResult.Text.Trim()
        $expectedTagAtHead = $expectedTagCommit -eq $headCommit
    }
}

$gitDirResult = Invoke-Git rev-parse --git-dir
if ($gitDirResult.ExitCode -ne 0) { throw "Could not resolve Git directory." }
$gitDir = $gitDirResult.Text.Trim()
if (-not [System.IO.Path]::IsPathRooted($gitDir)) { $gitDir = [System.IO.Path]::GetFullPath((Join-Path $projectRoot $gitDir)) }
$operationMarkers = @(
    "MERGE_HEAD", "CHERRY_PICK_HEAD", "REVERT_HEAD", "BISECT_LOG", "rebase-apply", "rebase-merge"
)
$activeOperations = @($operationMarkers | Where-Object { Test-Path -LiteralPath (Join-Path $gitDir $_) })

$diffCheck = Invoke-Git diff --check HEAD --
$diffCheckPass = $diffCheck.ExitCode -eq 0
$unmergedResult = Invoke-Git diff --name-only --diff-filter=U
$unmerged = @($unmergedResult.Output | ForEach-Object { ([string]$_).Trim() } | Where-Object { $_ })
$submoduleResult = Invoke-Git submodule status --recursive
$submoduleLines = @($submoduleResult.Output | ForEach-Object { [string]$_ } | Where-Object { $_ })
$submodulesClean = $submoduleResult.ExitCode -eq 0 -and @($submoduleLines | Where-Object { $_ -match '^[+-U]' }).Count -eq 0

$upstreamResult = Invoke-Git rev-parse --abbrev-ref --symbolic-full-name '@{upstream}'
$upstream = if ($upstreamResult.ExitCode -eq 0) { $upstreamResult.Text.Trim() } else { "" }
$ahead = $null
$behind = $null
if (-not [string]::IsNullOrWhiteSpace($upstream)) {
    $counts = Invoke-Git rev-list --left-right --count "$upstream...HEAD"
    if ($counts.ExitCode -eq 0 -and $counts.Text.Trim() -match '^(\d+)\s+(\d+)$') {
        $behind = [int]$Matches[1]
        $ahead = [int]$Matches[2]
    }
}
$pushed = -not [string]::IsNullOrWhiteSpace($upstream) -and $ahead -eq 0 -and $behind -eq 0

$dirtyByPath = @{}
foreach ($line in $statusStart) {
    $path = Get-StatusPath $line
    if (-not [string]::IsNullOrWhiteSpace($path)) { $dirtyByPath[$path.ToLowerInvariant()] = $line.Substring(0, 2) }
}
$trackedResult = Invoke-Git -c core.quotepath=false ls-files
if ($trackedResult.ExitCode -ne 0) { throw "git ls-files failed: $($trackedResult.Text)" }
$inventoryPaths = @{}
foreach ($pathValue in $trackedResult.Output) {
    $path = ([string]$pathValue).Replace('\', '/')
    if (Test-RelevantPath $path) { $inventoryPaths[$path.ToLowerInvariant()] = $path }
}
foreach ($line in $statusStart) {
    $path = Get-StatusPath $line
    if (Test-RelevantPath $path) { $inventoryPaths[$path.ToLowerInvariant()] = $path }
}

$inventory = New-Object 'System.Collections.Generic.List[object]'
$capturedFileState = @{}
foreach ($path in @($inventoryPaths.Values | Sort-Object)) {
    $absolute = Join-Path $projectRoot $path
    $statusKey = $path.ToLowerInvariant()
    $status = if ($dirtyByPath.ContainsKey($statusKey)) { [string]$dirtyByPath[$statusKey] } else { "  " }
    if (Test-Path -LiteralPath $absolute -PathType Leaf) {
        $item = Get-Item -LiteralPath $absolute
        $capturedFileState[$statusKey] = "$($item.Length)|$($item.LastWriteTimeUtc.Ticks)|present"
        $inventory.Add([PSCustomObject][ordered]@{
            Path = $path
            GitStatus = $status
            Bytes = [long]$item.Length
            Sha256 = (Get-FileHash -LiteralPath $absolute -Algorithm SHA256).Hash.ToLowerInvariant()
        })
    }
    else {
        $capturedFileState[$statusKey] = "0|0|missing"
        $inventory.Add([PSCustomObject][ordered]@{ Path = $path; GitStatus = $status; Bytes = 0; Sha256 = "MISSING" })
    }
}

$depot = Resolve-ProjectPath $DepotRoot
$artifact = $null
if (Test-Path -LiteralPath (Join-Path $depot "GMEditor.exe") -PathType Leaf) {
    $artifact = [PSCustomObject][ordered]@{
        depotRoot = $depot
        executable = Join-Path $depot "GMEditor.exe"
        executableSha256 = (Get-FileHash -LiteralPath (Join-Path $depot "GMEditor.exe") -Algorithm SHA256).Hash.ToLowerInvariant()
        checksumManifestSha256 = if (Test-Path -LiteralPath (Join-Path $depot "SHA256SUMS.txt") -PathType Leaf) {
            (Get-FileHash -LiteralPath (Join-Path $depot "SHA256SUMS.txt") -Algorithm SHA256).Hash.ToLowerInvariant()
        } else { "" }
        ctestLog = if (-not [string]::IsNullOrWhiteSpace($CTestLog)) { Resolve-ProjectPath $CTestLog } else { "" }
        ctestLogSha256 = if (-not [string]::IsNullOrWhiteSpace($CTestLog) -and (Test-Path -LiteralPath (Resolve-ProjectPath $CTestLog) -PathType Leaf)) {
            (Get-FileHash -LiteralPath (Resolve-ProjectPath $CTestLog) -Algorithm SHA256).Hash.ToLowerInvariant()
        } else { "" }
        mapQaReport = if (-not [string]::IsNullOrWhiteSpace($MapQaReport)) { Resolve-ProjectPath $MapQaReport } else { "" }
        mapQaReportSha256 = if (-not [string]::IsNullOrWhiteSpace($MapQaReport) -and (Test-Path -LiteralPath (Resolve-ProjectPath $MapQaReport) -PathType Leaf)) {
            (Get-FileHash -LiteralPath (Resolve-ProjectPath $MapQaReport) -Algorithm SHA256).Hash.ToLowerInvariant()
        } else { "" }
    }
}

if ([string]::IsNullOrWhiteSpace($RunId)) {
    $RunId = "source-$($headCommit.Substring(0, 12))-$([DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss'))"
}
if ($RunId -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]{2,95}$') { throw "RunId contains unsafe characters." }
$runRoot = Join-Path (Resolve-ProjectPath $OutputRoot) $RunId
if (Test-Path -LiteralPath $runRoot) { throw "Source-freeze audit already exists and will not be overwritten: $runRoot" }
[System.IO.Directory]::CreateDirectory($runRoot) | Out-Null

$inventoryPath = Join-Path $runRoot "working-tree-inventory.csv"
$statusPath = Join-Path $runRoot "git-status.txt"
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$inventoryCsv = @($inventory.ToArray() | ConvertTo-Csv -NoTypeInformation)
[System.IO.File]::WriteAllLines($inventoryPath, $inventoryCsv, $utf8NoBom)
[System.IO.File]::WriteAllLines($statusPath, $statusStart, $utf8NoBom)
$inventorySha256 = (Get-FileHash -LiteralPath $inventoryPath -Algorithm SHA256).Hash.ToLowerInvariant()
$statusSha256 = (Get-FileHash -LiteralPath $statusPath -Algorithm SHA256).Hash.ToLowerInvariant()

$statusEndResult = Invoke-Git -c core.quotepath=false status --porcelain=v1 --untracked-files=all
$statusEnd = @($statusEndResult.Output | ForEach-Object { [string]$_ } | Where-Object { $_ -ne "" })
$fileStateStable = $true
foreach ($path in $inventoryPaths.Values) {
    $key = $path.ToLowerInvariant()
    $absolute = Join-Path $projectRoot $path
    $currentState = if (Test-Path -LiteralPath $absolute -PathType Leaf) {
        $item = Get-Item -LiteralPath $absolute
        "$($item.Length)|$($item.LastWriteTimeUtc.Ticks)|present"
    }
    else { "0|0|missing" }
    if (-not $capturedFileState.ContainsKey($key) -or $capturedFileState[$key] -ne $currentState) {
        $fileStateStable = $false
        break
    }
}
$captureStable = $statusEndResult.ExitCode -eq 0 -and
    (($statusStart -join "`n") -ceq ($statusEnd -join "`n")) -and $fileStateStable
$relevantDirty = @($statusStart | Where-Object { Test-RelevantPath (Get-StatusPath $_) })
$relevantUntracked = @($relevantDirty | Where-Object { $_.StartsWith("??") })
$relevantTrackedDirty = @($relevantDirty | Where-Object { -not $_.StartsWith("??") })

$blockers = New-Object 'System.Collections.Generic.List[string]'
if ($statusStart.Count -gt 0) { $blockers.Add("Working tree has $($statusStart.Count) dirty path(s); this is not a release commit.") }
if ($relevantDirty.Count -gt 0) { $blockers.Add("$($relevantDirty.Count) dirty path(s) are release-relevant ($($relevantTrackedDirty.Count) tracked, $($relevantUntracked.Count) untracked).") }
if ($unmerged.Count -gt 0) { $blockers.Add("Working tree contains $($unmerged.Count) unmerged path(s).") }
if ($activeOperations.Count -gt 0) { $blockers.Add("Git operation in progress: $($activeOperations -join ', ').") }
if (-not $diffCheckPass) { $blockers.Add("git diff --check reports whitespace or conflict-marker errors.") }
if (-not $submodulesClean) { $blockers.Add("Submodule state is unresolved or dirty.") }
if ($branch -eq "DETACHED") { $blockers.Add("HEAD is detached.") }
if ([string]::IsNullOrWhiteSpace($ExpectedTag)) { $blockers.Add("No expected release tag was supplied.") }
elseif (-not $expectedTagExists) { $blockers.Add("Expected release tag '$ExpectedTag' does not exist.") }
elseif (-not $expectedTagAtHead) { $blockers.Add("Expected release tag '$ExpectedTag' does not point to HEAD.") }
if ($RequirePushed -and -not $pushed) { $blockers.Add("HEAD is not proven synchronized with an upstream branch.") }
if (-not $captureStable) { $blockers.Add("Working tree changed while the source inventory was being captured; rerun after all agents and tools are idle.") }

$ready = $blockers.Count -eq 0
$report = [PSCustomObject][ordered]@{
    schemaVersion = 1
    generatedAtUtc = [DateTime]::UtcNow.ToString("o")
    status = if ($ready) { "ready" } else { "blocked" }
    releaseMeaning = if ($ready) { "Clean tagged source state captured." } else { "Inventory evidence only; this report is not a release commit or tag." }
    projectRoot = $projectRoot
    headCommit = $headCommit
    headTree = $headTree
    branch = $branch
    upstream = $upstream
    ahead = $ahead
    behind = $behind
    expectedTag = $ExpectedTag
    expectedTagExists = $expectedTagExists
    expectedTagAtHead = $expectedTagAtHead
    tagsAtHead = $tagNames
    requirePushed = [bool]$RequirePushed
    pushed = $pushed
    dirtyPathCount = $statusStart.Count
    relevantDirtyPathCount = $relevantDirty.Count
    relevantTrackedDirtyPathCount = $relevantTrackedDirty.Count
    relevantUntrackedPathCount = $relevantUntracked.Count
    relevantInventoryFileCount = $inventory.Count
    unmergedPathCount = $unmerged.Count
    activeGitOperations = $activeOperations
    diffCheckPass = $diffCheckPass
    submodulesClean = $submodulesClean
    captureStable = $captureStable
    statusSha256 = $statusSha256
    inventorySha256 = $inventorySha256
    artifact = $artifact
    blockers = $blockers.ToArray()
}
$jsonPath = Join-Path $runRoot "source-freeze-readiness.json"
$markdownPath = Join-Path $runRoot "source-freeze-readiness.md"
[System.IO.File]::WriteAllText($jsonPath, ($report | ConvertTo-Json -Depth 6), $utf8NoBom)

$blockerText = if ($blockers.Count -eq 0) { "- keine / none" } else { (@($blockers | ForEach-Object { "- $_" }) -join "`r`n") }
$markdown = @"
# Source-freeze readiness / Quellfreeze-Bereitschaft

- Status: **$($report.status.ToUpperInvariant())**
- HEAD: $headCommit
- Tree: $headTree
- Branch: $branch
- Expected tag: $ExpectedTag (exists: $expectedTagExists; at HEAD: $expectedTagAtHead)
- Dirty paths: $($statusStart.Count); release-relevant: $($relevantDirty.Count)
- Relevant inventory: $($inventory.Count) files; SHA-256 $inventorySha256
- Capture stable: $captureStable

## Blockers

$blockerText

Ein blocked-Bericht inventarisiert den Arbeitsstand, ersetzt aber weder einen Commit noch einen Tag. Nach Abschluss aller parallelen Aenderungen muss der freigegebene Stand sauber committed, getaggt, erneut gebaut und ueber die gesamte hashgebundene Technik- und manuelle Pruefkette gefuehrt werden.

A blocked report inventories the working state but is not a commit or tag. Once all parallel changes are complete, the approved state must be cleanly committed, tagged, rebuilt, and passed through the complete hash-bound technical and manual evidence chain.
"@
[System.IO.File]::WriteAllText($markdownPath, $markdown, $utf8NoBom)

Write-Host "Source-freeze readiness: $($report.status.ToUpperInvariant())"
Write-Host "Dirty paths: $($statusStart.Count) total; $($relevantDirty.Count) release-relevant."
Write-Host "Inventory: $($inventory.Count) files; SHA-256 $inventorySha256"
Write-Host "Report: $jsonPath"
foreach ($blocker in $blockers) { Write-Warning $blocker }
if ($RequireReady -and -not $ready) { throw "Source-freeze readiness gate is blocked with $($blockers.Count) blocker(s)." }
