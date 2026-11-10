#requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BuildDirectory,
    [Parameter(Mandatory)][string]$OutputRoot,
    [ValidateSet(9,10,11)][int]$RecipeVersion=11,
    [switch]$AllowDirty
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$source=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..')).TrimEnd('\','/')
$output=[IO.Path]::GetFullPath($OutputRoot).TrimEnd('\','/')
$build=[IO.Path]::GetFullPath($BuildDirectory)
if($output -eq $source -or $output.StartsWith($source+'\',[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Keep disposable art evidence outside the checkout.'
}
if(Test-Path -LiteralPath $output){throw 'Use a new evidence directory.'}
$status=@(& git -C $source status --porcelain)
if($status.Count -and -not $AllowDirty){throw 'Dirty review runs require -AllowDirty; they are not release acceptance.'}
$exe=Join-Path $build 'map_generation_preview_export.exe'
if(-not(Test-Path -LiteralPath $exe)){throw 'Build map_generation_preview_export first.'}
[IO.Directory]::CreateDirectory($output)|Out-Null
$manifest=[ordered]@{
    schema_version=1;created_utc=[DateTime]::UtcNow.ToString('o');commit=(& git -C $source rev-parse HEAD).Trim()
    dirty=$status;recipe_version=$RecipeVersion;generator_version=12
    executable_sha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
    human_art_acceptance=$false;campaign_launch_authorized=$false;cases=@()
}
$previous=@{}
foreach($name in @('TTRPG_DATA_ROOT','TTRPG_DB_PATH','TTRPG_SETTINGS_PATH')) {
    $previous[$name]=[Environment]::GetEnvironmentVariable($name,'Process')
}
try {
    $env:TTRPG_DATA_ROOT=Join-Path $output 'data'
    $env:TTRPG_DB_PATH=Join-Path $output 'data/TTRPG.db'
    $env:TTRPG_SETTINGS_PATH=Join-Path $output 'data/GMEditor.settings'
    # Fixed, complete scene inventory. Retain every result, including sparse or
    # unattractive scenes, and reuse both seeds for each artwork version.
    foreach($kind in @('open','forest','river','ruins','village','city','cave','dungeon')) {
        foreach($seed in @(612345,612346)) {
            $relative="$kind-$seed";$directory=Join-Path $output $relative
            [IO.Directory]::CreateDirectory($directory)|Out-Null
            $arguments=@('--out',('"'+(Join-Path $directory 'map.json')+'"'),
                '--png',('"'+(Join-Path $directory 'map.png')+'"'),
                '--generator-version',12,'--recipe-version',$RecipeVersion,'--style-id','illustrated_atlas',
                '--scope','local','--preset','tactical','--tactical-template',$kind,
                '--width',32,'--height',24,'--seed',$seed,'--quality-attempts',12,'--pixels-per-cell',32,'--locale','en')
            $process=Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $build -WindowStyle Hidden -Wait -PassThru `
                -RedirectStandardOutput (Join-Path $directory 'stdout.log') -RedirectStandardError (Join-Path $directory 'stderr.log')
            $record=[ordered]@{kind=$kind;seed=$seed;path=$relative;arguments=$arguments;exit_code=$process.ExitCode;png_sha256=$null;report_sha256=$null}
            if($process.ExitCode -eq 0) {
                $record.png_sha256=(Get-FileHash -LiteralPath (Join-Path $directory 'map.png') -Algorithm SHA256).Hash
                $record.report_sha256=(Get-FileHash -LiteralPath (Join-Path $directory 'map.json') -Algorithm SHA256).Hash
            }
            $manifest.cases+=$record
            $manifest|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $output 'manifest.json') -Encoding UTF8
            if($process.ExitCode){throw "Local art export failed: $relative"}
            Write-Output "Captured $relative, recipe $RecipeVersion"
        }
    }
}
finally {foreach($name in $previous.Keys){[Environment]::SetEnvironmentVariable($name,$previous[$name],'Process')}}
