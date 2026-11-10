#requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BuildDirectory,
    [Parameter(Mandatory)][string]$OutputRoot,
    [ValidateSet('world','region','tactical','forest','river','ruins','city','cave','dungeon')][string[]]$Kinds=@('world','region','tactical'),
    [ValidateSet('en-US','de-DE')][string]$Locale='en-US',
    [switch]$SkipBuild
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$source=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..')).TrimEnd('\','/')
$output=[IO.Path]::GetFullPath($OutputRoot).TrimEnd('\','/')
$build=[IO.Path]::GetFullPath($BuildDirectory)
if($output -eq $source -or $output.StartsWith($source+'\',[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Art capture data and native evidence must stay outside the checkout.'
}
if(Test-Path -LiteralPath $output){throw 'Use a new capture directory to preserve evidence.'}
if(-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build-windows-msvc.ps1') -Configuration Release -BuildDir $build -Jobs 4 -Targets application_desktop_qa
}
$exe=Join-Path $build 'application_desktop_qa.exe'
if(-not(Test-Path -LiteralPath $exe)){throw 'Build application_desktop_qa first.'}
[IO.Directory]::CreateDirectory($output)|Out-Null
$files=@(& git -C $source ls-files --cached --others --exclude-standard)
@(foreach($relative in ($files|Sort-Object -Unique)) {
    $path=Join-Path $source $relative
    if(Test-Path -LiteralPath $path -PathType Leaf) {
        [ordered]@{path=$relative;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
    }
})|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $output 'source-inventory.json') -Encoding UTF8
$manifest=[ordered]@{
    schema_version=1;created_utc=[DateTime]::UtcNow.ToString('o')
    commit=(& git -C $source rev-parse HEAD).Trim();source_status=@(& git -C $source status --porcelain)
    source_inventory_sha256=(Get-FileHash -LiteralPath (Join-Path $output 'source-inventory.json') -Algorithm SHA256).Hash
    executable_sha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
    fixture='Generated art showcase; no personal campaign data';renderer='Actual application OpenGL framebuffer'
    locale=$Locale;recipe_version=[int]([regex]::Match((Get-Content -Raw (Join-Path $source 'Models/Map/CurrentMapArtRecipe.hpp')),'kCurrentMapIllustrationRecipe\s*=\s*(\d+)').Groups[1].Value);physical_monitor_acceptance=$false;human_art_acceptance=$false
    campaign_launch_authorized=$false;runs=@()
}
$previous=@{}
foreach($name in @('TTRPG_DATA_ROOT','TTRPG_DB_PATH','TTRPG_SETTINGS_PATH')) {
    $previous[$name]=[Environment]::GetEnvironmentVariable($name,'Process')
}
try {
    foreach($kind in $Kinds) {
        $directory=Join-Path $output $kind
        $data=Join-Path $directory 'data'
        [IO.Directory]::CreateDirectory($data)|Out-Null
        $env:TTRPG_DATA_ROOT=$data
        $env:TTRPG_DB_PATH=Join-Path $data 'TTRPG.db'
        $env:TTRPG_SETTINGS_PATH=Join-Path $data 'GMEditor.settings'
        $arguments=@(('"'+$directory+'"'),$Locale,('--art-'+$kind))
        $process=Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $build -WindowStyle Hidden -Wait -PassThru `
            -RedirectStandardOutput (Join-Path $directory 'stdout.log') -RedirectStandardError (Join-Path $directory 'stderr.log')
        $run=[ordered]@{kind=$kind;exit_code=$process.ExitCode;pixel_controls_passed=$false;captures=@()}
        $manifest.runs+=$run
        $manifest|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $output 'manifest.json') -Encoding UTF8
        if($process.ExitCode){throw "Art showcase failed: $directory"}
        & (Join-Path $PSScriptRoot 'assert-player-framebuffers.ps1') -CaptureDirectory $directory
        $run.pixel_controls_passed=$true
        foreach($bitmap in (Get-ChildItem -LiteralPath $directory -Filter capture.bmp -File -Recurse)) {
            $run.captures+=[ordered]@{path=$bitmap.FullName.Substring($output.Length+1).Replace('\','/');sha256=(Get-FileHash -LiteralPath $bitmap.FullName -Algorithm SHA256).Hash}
        }
        $manifest|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $output 'manifest.json') -Encoding UTF8
        Write-Output "Captured actual $kind player window; arm/publish/revocation and blackout controls passed."
    }
}
finally {
    foreach($name in $previous.Keys){[Environment]::SetEnvironmentVariable($name,$previous[$name],'Process')}
}
Write-Output "Native art evidence: $output"
