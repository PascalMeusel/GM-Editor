#requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BuildDirectory,
    [Parameter(Mandatory)][string]$OutputRoot
)
$ErrorActionPreference='Stop'
$source=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..')).TrimEnd('\','/')
$output=[IO.Path]::GetFullPath($OutputRoot).TrimEnd('\','/')
$build=[IO.Path]::GetFullPath($BuildDirectory)
if($output -eq $source -or $output.StartsWith($source+'\',[StringComparison]::OrdinalIgnoreCase)) {throw 'Keep QA data outside the checkout.'}
if(Test-Path -LiteralPath $output) {throw 'Use a new evidence directory.'}
if(-not [Environment]::UserInteractive) {throw 'Run in the signed-in desktop session.'}
$exe=Join-Path $build 'map_editing_ui_qa.exe'
if(-not (Test-Path -LiteralPath $exe)) {throw 'Build the map_editing_ui_qa target first.'}
[IO.Directory]::CreateDirectory($output)|Out-Null
$manifest=[ordered]@{commit=(& git -C $source rev-parse HEAD).Trim();dirty=@(& git -C $source status --porcelain);
    executable_sha256=(Get-FileHash -LiteralPath $exe).Hash;
    scope='Actual production ImGui mouse interactions and framebuffer captures; no physical-display or human-art approval';runs=@()}
$previous=@{}
foreach($name in @('TTRPG_DATA_ROOT','TTRPG_DB_PATH','TTRPG_SETTINGS_PATH')) {$previous[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
try {
    foreach($locale in @('en-US','de-DE')) {
        $directory=Join-Path $output $locale
        [IO.Directory]::CreateDirectory($directory)|Out-Null
        $env:TTRPG_DATA_ROOT=Join-Path $directory 'data'
        $env:TTRPG_DB_PATH=Join-Path $env:TTRPG_DATA_ROOT 'TTRPG.db'
        $env:TTRPG_SETTINGS_PATH=Join-Path $env:TTRPG_DATA_ROOT 'settings'
        $process=Start-Process -FilePath $exe -ArgumentList @(('"'+$directory+'"'),$locale,'1280','720') -WorkingDirectory $build -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput (Join-Path $directory 'stdout.log') -RedirectStandardError (Join-Path $directory 'stderr.log')
        $manifest.runs+=@{locale=$locale;viewport='1280x720';exit_code=$process.ExitCode;passed=($process.ExitCode -eq 0);
            files=@(Get-ChildItem -LiteralPath $directory -File | ForEach-Object {@{path=$_.Name;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})}
        $manifest|ConvertTo-Json -Depth 8|Set-Content (Join-Path $output 'manifest.json') -Encoding UTF8
        if($process.ExitCode) {throw "Map interaction QA failed for $locale; inspect retained stderr and captures."}
    }
} finally {foreach($name in $previous.Keys) {[Environment]::SetEnvironmentVariable($name,$previous[$name],'Process')}}
