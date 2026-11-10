#requires -Version 5.1
[CmdletBinding()]
param([Parameter(Mandatory)][ValidatePattern('^[0-9a-f]{40}$')][string]$KeepCommit,
      [ValidateRange(0,2)][int]$KeepPrevious=1)
$ErrorActionPreference = 'Stop'
# Only disposable, SHA-named CI build directories are eligible. Evidence and
# worktrees are deliberately outside this root and are never pruned here.
$root = [IO.Path]::GetFullPath('C:\GMEditorCI\builds').TrimEnd('\')
if (-not (Test-Path -LiteralPath $root)) { return }
$rootItem = Get-Item -LiteralPath $root -Force
if ($rootItem.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'CI build root must not be a reparse point.' }
$old = @(Get-ChildItem -LiteralPath $root -Directory | Where-Object { $_.Name -match '^[0-9a-f]{40}$' -and $_.Name -ne $KeepCommit } | Sort-Object LastWriteTimeUtc -Descending)
foreach ($item in ($old | Select-Object -Skip $KeepPrevious)) {
    $resolved = [IO.Path]::GetFullPath($item.FullName)
    if (-not $resolved.StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase) -or
        (Split-Path -Parent $resolved) -ne $root -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw 'Refusing unsafe build cleanup target.'
    }
    if (@(Get-ChildItem -LiteralPath $resolved -Recurse -Force -Attributes ReparsePoint).Count) {
        throw 'Refusing recursive cleanup of a build containing reparse points.'
    }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}
