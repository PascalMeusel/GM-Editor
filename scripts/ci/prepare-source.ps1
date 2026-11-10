#requires -Version 5.1
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# Only the disposable service checkout may be rematerialized. Never operate
# on an owner's worktree, even if this helper is invoked there accidentally.
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..')).TrimEnd('\')
$expectedRoot = 'C:\GMEditorCI\runner\_work\TTRPG_Editor_cpp\TTRPG_Editor_cpp'
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
if ($env:GITHUB_ACTIONS -ne 'true' -or $identity.Name -notlike '*\GmEditorCI' -or
    $projectRoot -ne $expectedRoot -or $env:GITHUB_WORKSPACE.TrimEnd('\') -ne $expectedRoot) {
    throw 'Source preparation is restricted to the disposable GM Editor CI checkout.'
}
$head = (git -C $projectRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE) { throw 'Could not identify CI checkout.' }

# Repair the historic CRLF checkout once, and again whenever attributes change.
# Rewriting every source file made Ninja compile every translation unit on each
# PR even when only a few files changed. Normal checkout preserves clean bytes.
$attributes = (git -C $projectRoot rev-parse 'HEAD:.gitattributes').Trim()
if ($LASTEXITCODE) { throw 'Could not identify checkout attributes.' }
$stamp = 'C:\GMEditorCI\source-attributes.txt'
$previous = if (Test-Path -LiteralPath $stamp) { (Get-Content -LiteralPath $stamp -Raw).Trim() } else { '' }
if ($previous -ne $attributes) {
    git -C $projectRoot read-tree --empty
    if ($LASTEXITCODE) { throw 'Could not reset disposable checkout index.' }
    git -C $projectRoot -c core.autocrlf=false reset --hard --quiet $head
    if ($LASTEXITCODE) { throw 'Could not materialize committed source bytes.' }
    Set-Content -LiteralPath $stamp -Value $attributes -Encoding ASCII
}
if ((git -C $projectRoot rev-parse HEAD).Trim() -ne $head) { throw 'CI source revision changed.' }
Write-Output "Prepared committed source bytes at $head; unchanged timestamps retained."
