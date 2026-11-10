#requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ArchiveDirectory,
    [string]$SourceDirectory,
    [string[]]$EvidenceDirectories = @(),
    [switch]$VerifyOnly,
    [string]$RestoreDirectory
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
function Get-ArchiveHash([string]$Path) {
    $algorithm = [Security.Cryptography.SHA256]::Create()
    $stream = [IO.File]::OpenRead($Path)
    try { return ([BitConverter]::ToString($algorithm.ComputeHash($stream))).Replace('-','') }
    finally { $stream.Dispose(); $algorithm.Dispose() }
}
function Full-Path([string]$Path) { [IO.Path]::GetFullPath($Path).TrimEnd('\','/') }
function Is-Below([string]$Path,[string]$Root) { $Path.Equals($Root,[StringComparison]::OrdinalIgnoreCase) -or $Path.StartsWith($Root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) }
function Invoke-Git([string]$Directory,[string[]]$Arguments) {
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { $result = @(& git -C $Directory @Arguments 2>&1); $gitExit = $LASTEXITCODE }
    finally { $ErrorActionPreference = $previousPreference }
    if ($gitExit) { throw "Git operation failed: $($Arguments[0]); $($result -join ' ')" }
    return ($result -join "`n")
}
function Assert-RegularTree([string]$Root) {
    foreach ($item in @((Get-Item -LiteralPath $Root -Force)) + @(Get-ChildItem -LiteralPath $Root -Recurse -Force)) {
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Archive inputs must not contain links: $($item.FullName)" }
    }
}
function Assert-NoLinkedAncestor([string]$Path) {
    $current = $Path
    while ($current) {
        if (Test-Path -LiteralPath $current) {
            if ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Archive/restore paths must not traverse links' }
        }
        $current = [IO.Path]::GetDirectoryName($current)
    }
}
function Verify-Archive([string]$Root) {
    Assert-RegularTree $Root
    $manifest = Get-Content -LiteralPath (Join-Path $Root 'manifest.json') -Raw | ConvertFrom-Json
    if ($manifest.schema -ne 'gm-editor.engineering-archive.v1' -or @($manifest.files).Count -eq 0) { throw 'Invalid archive manifest' }
    $seen = @{}
    foreach ($file in $manifest.files) {
        $path = Full-Path (Join-Path $Root $file.path)
        if ([IO.Path]::IsPathRooted($file.path) -or -not (Is-Below $path $Root) -or $seen.ContainsKey($path)) { throw 'Unsafe or duplicate manifest path' }
        $seen[$path] = $true
        $item = Get-Item -LiteralPath $path
        if ($item.PSIsContainer -or $item.Length -ne $file.bytes -or (Get-ArchiveHash $path) -ne $file.sha256) { throw "Archive integrity mismatch: $($file.path)" }
    }
    $actual = @(Get-ChildItem -LiteralPath $Root -File -Recurse -Force | Where-Object FullName -ne (Join-Path $Root 'manifest.json'))
    if ($actual.Count -ne $seen.Count) { throw 'Archive contains unlisted files' }
    foreach ($file in $actual) { if (-not $seen.ContainsKey($file.FullName)) { throw 'Archive contains an unlisted file' } }
    return $manifest
}
$archive = Full-Path $ArchiveDirectory
Assert-NoLinkedAncestor $archive
if ($VerifyOnly -or $RestoreDirectory) {
    $manifest = Verify-Archive $archive
    if ($RestoreDirectory) {
        $restore = Full-Path $RestoreDirectory
        Assert-NoLinkedAncestor $restore
        if (Test-Path -LiteralPath $restore) { throw 'Restore requires a new directory' }
        if (Is-Below $restore $archive) { throw 'Restore must be outside the archive' }
        [IO.Directory]::CreateDirectory($restore) | Out-Null
        Invoke-Git $restore @('clone','--config','core.longpaths=true','--no-checkout',(Join-Path $archive 'source.bundle'),'.') | Out-Null
        Invoke-Git $restore @('checkout','--detach',$manifest.commit) | Out-Null
        Invoke-Git $restore @('submodule','init') | Out-Null
        foreach ($module in $manifest.submodules) {
            $target = Full-Path (Join-Path $restore $module.path)
            $bundle = Full-Path (Join-Path $archive $module.bundle)
            if ([IO.Path]::IsPathRooted($module.path) -or $target -eq $restore -or -not (Is-Below $target $restore) -or -not (Is-Below $bundle $archive)) { throw 'Unsafe submodule path' }
            [IO.Directory]::CreateDirectory($target) | Out-Null
            Invoke-Git $target @('clone','--config','core.longpaths=true','--no-checkout',$bundle,'.') | Out-Null
            Invoke-Git $target @('checkout','--detach',$module.commit) | Out-Null
            Invoke-Git $target @('submodule','init') | Out-Null
        }
        if ((Invoke-Git $restore @('rev-parse','HEAD')).Trim() -ne $manifest.commit -or (Invoke-Git $restore @('status','--porcelain')).Trim()) { throw 'Restored checkout differs from archived revision' }
        Write-Output "Offline source restore verified at $restore"
    }
    Write-Output "Verified $(@($manifest.files).Count) archive files for $($manifest.commit)"
    return
}
if ([string]::IsNullOrWhiteSpace($SourceDirectory)) { $SourceDirectory = Join-Path $PSScriptRoot '..' }
$source = Full-Path $SourceDirectory
$source = Full-Path ((Invoke-Git $source @('rev-parse','--show-toplevel')).Trim())
if (Is-Below $archive $source) { throw 'Archive must be outside the source checkout' }
if (Test-Path -LiteralPath $archive) { throw 'Archive requires a new directory; existing archives are never overwritten' }
if ((Invoke-Git $source @('status','--porcelain','--untracked-files=all')).Trim()) { throw 'Commit source changes before archiving' }
$commit = (Invoke-Git $source @('rev-parse','HEAD')).Trim()
$inputs = @()
foreach ($directory in $EvidenceDirectories) {
    $inputPath = Full-Path $directory
    if ((Is-Below $archive $inputPath) -or (Is-Below $inputPath $source) -or -not (Test-Path -LiteralPath $inputPath -PathType Container)) { throw 'Evidence must be an existing external directory, separate from archive and checkout' }
    Assert-RegularTree $inputPath
    $inputs += $inputPath
}
$drive = New-Object IO.DriveInfo ([IO.Path]::GetPathRoot($archive))
$common = (Invoke-Git $source @('rev-parse','--git-common-dir')).Trim()
if (-not [IO.Path]::IsPathRooted($common)) { $common = Join-Path $source $common }
$gitBytes = (Get-ChildItem -LiteralPath $common -File -Recurse -Force | Measure-Object -Property Length -Sum).Sum
$evidenceBytes = 0L
foreach ($inputPath in $inputs) {
    $evidenceBytes += (Get-ChildItem -LiteralPath $inputPath -File -Recurse -Force | Measure-Object -Property Length -Sum).Sum
}
# Conservative allowance for Git repacking and explicit evidence copies.
$required = 50GB + 2GB + (2 * $gitBytes) + $evidenceBytes
if ($drive.AvailableFreeSpace -lt $required) { throw 'Archive would breach the 50 GiB free-space floor; select another drive' }
[IO.Directory]::CreateDirectory($archive) | Out-Null
Invoke-Git $source @('bundle','create',(Join-Path $archive 'source.bundle'),'--all') | Out-Null
$modules = @()
$index = 0
foreach ($line in ((Invoke-Git $source @('submodule','status','--recursive')) -split "`n")) {
    if (-not $line.Trim()) { continue }
    if ($line -notmatch '^ ([0-9a-f]{40}) (.+?)(?: \(|$)') { throw 'All submodules must be initialized at their recorded revisions' }
    $moduleCommit = $Matches[1]; $modulePath = $Matches[2]
    $bundle = "submodule-$index.bundle"; $index++
    Invoke-Git (Join-Path $source $modulePath) @('bundle','create',(Join-Path $archive $bundle),'HEAD') | Out-Null
    $modules += [ordered]@{path=$modulePath;commit=$moduleCommit;bundle=$bundle}
}
$index = 0
$origins = @()
foreach ($inputPath in $inputs) {
    $relative = "evidence-$index"; $index++
    $target = Join-Path $archive $relative
    [IO.Directory]::CreateDirectory($target) | Out-Null
    foreach ($file in Get-ChildItem -LiteralPath $inputPath -File -Recurse -Force) {
        $suffix = $file.FullName.Substring($inputPath.Length+1)
        $destination = Join-Path $target $suffix
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination)) | Out-Null
        [IO.File]::Copy($file.FullName,$destination,$false)
        if ((Get-ArchiveHash $file.FullName) -ne (Get-ArchiveHash $destination)) { throw 'Evidence changed while being copied' }
    }
    $origins += [ordered]@{source=$inputPath;archive_path=$relative}
}
$files = @(Get-ChildItem -LiteralPath $archive -File -Recurse -Force | ForEach-Object {
    [ordered]@{path=$_.FullName.Substring($archive.Length+1);bytes=$_.Length;sha256=(Get-ArchiveHash $_.FullName)}
})
$manifest = [ordered]@{schema='gm-editor.engineering-archive.v1';commit=$commit;created_utc=[DateTime]::UtcNow.ToString('o');submodules=$modules;evidence=$origins;files=$files;scope='Source history and explicitly selected engineering evidence; no runner credentials or personal runtime data'}
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $archive 'manifest.json') -Encoding UTF8
Verify-Archive $archive | Out-Null
Write-Output "Created and verified $($files.Count) files at $archive"
