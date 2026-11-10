#requires -Version 5.1
$ErrorActionPreference = 'Stop'
$script = Join-Path $PSScriptRoot 'engineering-archive.ps1'
$root = Join-Path ([IO.Path]::GetTempPath()) ('gm-archive-contract-' + [guid]::NewGuid().ToString('N'))
$source = Join-Path $root 'source';$evidence = Join-Path $root 'evidence';$archive = Join-Path $root 'archive'
[IO.Directory]::CreateDirectory($source) | Out-Null
[IO.Directory]::CreateDirectory($evidence) | Out-Null
$archiveOriginalGlobal = $env:GIT_CONFIG_GLOBAL
try {
    $env:GIT_CONFIG_GLOBAL = Join-Path $root 'isolated-global.gitconfig'
    [IO.File]::WriteAllText($env:GIT_CONFIG_GLOBAL,"[core]`nlongpaths = false`n")
function Add-LongPathFixture([string]$Repository) {
    & git -C $Repository config core.longpaths true
    $relative = ((1..5 | ForEach-Object { 'historical-artwork-' + ('x' * 22) }) -join '/') + '/image.txt'
    $path = [IO.Path]::GetFullPath((Join-Path $Repository $relative))
    $extended = '\\?\' + $path
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($extended)) | Out-Null
    [IO.File]::WriteAllText($extended,'generated long-path artwork fixture')
    & git -C $Repository add -- $relative
    if ($LASTEXITCODE) { throw 'Could not stage generated long-path fixture' }
    return $relative
}
& git init -q $source
& git -C $source config user.email 'fixture@example.invalid'
& git -C $source config user.name 'Generated archive fixture'
'generated source' | Set-Content (Join-Path $source 'source.txt')
$longRelative = Add-LongPathFixture $source
& git -C $source add source.txt
& git -C $source commit -qm 'Generated fixture'
$module = Join-Path $root 'dependency'
& git init -q $module
& git -C $module config user.email 'fixture@example.invalid'
& git -C $module config user.name 'Generated dependency fixture'
'generated dependency' | Set-Content (Join-Path $module 'dependency.txt')
$moduleLongRelative = Add-LongPathFixture $module
& git -C $module add dependency.txt
& git -C $module commit -qm 'Generated dependency'
& git -C $source -c core.longpaths=true -c protocol.file.allow=always submodule add -q $module 'dependency'
if ($LASTEXITCODE) { throw 'Could not create generated submodule fixture' }
& git -C (Join-Path $source 'dependency') config core.longpaths true
& git -C $source commit -qam 'Pin generated dependency'
'generated evidence' | Set-Content (Join-Path $evidence 'report.txt')
& $script -SourceDirectory $source -ArchiveDirectory $archive -EvidenceDirectories @($evidence)
& powershell -NoProfile -ExecutionPolicy Bypass -File $script -ArchiveDirectory $archive -VerifyOnly
if ($LASTEXITCODE) { throw 'Standalone archive verification failed' }
& powershell -NoProfile -ExecutionPolicy Bypass -File $script -ArchiveDirectory $archive -RestoreDirectory (Join-Path $root 'restored')
if ($LASTEXITCODE) { throw 'Standalone archive restore failed' }
if ((Get-Content (Join-Path $root 'restored/source.txt') -Raw) -ne (Get-Content (Join-Path $source 'source.txt') -Raw)) { throw 'Offline restore lost source content' }
if ((Get-Content (Join-Path $root 'restored/dependency/dependency.txt') -Raw) -ne (Get-Content (Join-Path $module 'dependency.txt') -Raw)) { throw 'Offline restore lost dependency content' }
foreach ($relative in @($longRelative, ('dependency/' + $moduleLongRelative))) {
    $restoredFile = '\\?\' + [IO.Path]::GetFullPath((Join-Path (Join-Path $root 'restored') $relative))
    if ([IO.File]::ReadAllText($restoredFile) -ne 'generated long-path artwork fixture') { throw 'Offline restore omitted a long-path file' }
}
& $script -SourceDirectory (Join-Path $root 'restored') -ArchiveDirectory (Join-Path $root 'restored-archive')
function Reject([scriptblock]$Operation,[string]$Reason) {
    $rejected=$false
    try { & $Operation | Out-Null } catch { $rejected=$true }
    if (-not $rejected) { throw "Expected rejection: $Reason" }
    Write-Output "Rejected: $Reason"
}
Reject { & $script -SourceDirectory $source -ArchiveDirectory $archive } 'existing archive'
Reject { & $script -SourceDirectory $source -ArchiveDirectory (Join-Path $source 'archive') } 'archive inside checkout'
'changed source' | Add-Content (Join-Path $source 'source.txt')
Reject { & $script -SourceDirectory $source -ArchiveDirectory (Join-Path $root 'dirty-archive') } 'dirty source'
'Tampered evidence' | Set-Content (Join-Path $archive 'evidence-0/report.txt')
Reject { & $script -ArchiveDirectory $archive -VerifyOnly } 'tampered evidence'
$manifestPath=Join-Path $archive 'manifest.json'
$manifest=Get-Content $manifestPath -Raw | ConvertFrom-Json
$manifest.files[0].path='../evidence/report.txt'
$manifest | ConvertTo-Json -Depth 8 | Set-Content $manifestPath -Encoding UTF8
Reject { & $script -ArchiveDirectory $archive -VerifyOnly } 'manifest traversal'
Write-Output "Engineering archive contracts passed. Generated fixtures: $root"

} finally { $env:GIT_CONFIG_GLOBAL = $archiveOriginalGlobal }
