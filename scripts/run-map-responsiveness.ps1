#requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BuildDirectory,
    [Parameter(Mandatory)][string]$OutputRoot,
    [ValidateRange(3,11)][int]$RecipeVersion=11,
    [ValidateSet('world','region','tactical','city','ruins','cave','dungeon')][string[]]$Kinds=@('world','region','tactical'),
    [switch]$AllowDirty,
    [switch]$SkipBuild
)
$ErrorActionPreference='Stop'
if(-not [Environment]::UserInteractive) {throw 'Run in the signed-in desktop session, with CI and other benchmarks idle.'}
$source=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..')).TrimEnd('\','/')
$output=[IO.Path]::GetFullPath($OutputRoot).TrimEnd('\','/')
$build=[IO.Path]::GetFullPath($BuildDirectory)
if($output -eq $source -or $output.StartsWith($source+'\',[StringComparison]::OrdinalIgnoreCase)) {throw 'Keep evidence outside the checkout.'}
if(Test-Path -LiteralPath $output) {throw 'Use a new evidence directory.'}
$status=@(& git -C $source status --porcelain)
if($status.Count -and -not $AllowDirty) {throw 'Commit the source before producing acceptance evidence.'}
[IO.Directory]::CreateDirectory($output)|Out-Null
if(-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build-windows-msvc.ps1') -Configuration Release -BuildDir $build -Jobs 4 -Targets map_application_benchmark *> (Join-Path $output 'build.log')
    if($LASTEXITCODE) {throw 'Benchmark build failed.'}
}
$exe=Join-Path $build 'map_application_benchmark.exe'
$assets=Join-Path $build 'Resources'
@(Get-ChildItem -LiteralPath $assets -File -Recurse|Sort-Object FullName|ForEach-Object {
    [ordered]@{path=$_.FullName.Substring($assets.Length+1);sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
})|ConvertTo-Json -Depth 4|Set-Content (Join-Path $output 'assets.json') -Encoding UTF8
$manifest=[ordered]@{commit=(& git -C $source rev-parse HEAD).Trim();dirty=$status;build_skipped=[bool]$SkipBuild;
    executable_sha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash;
    assets_sha256=(Get-FileHash -LiteralPath (Join-Path $output 'assets.json') -Algorithm SHA256).Hash;
    dependencies=@(& git -C $source submodule status --recursive);
    cpu=@(Get-CimInstance Win32_Processor|Select-Object Name,NumberOfCores,NumberOfLogicalProcessors);
    gpu=@(Get-CimInstance Win32_VideoController|Select-Object Name,DriverVersion,CurrentHorizontalResolution,CurrentVerticalResolution);
    viewport='1280x720';window_visible=$false;input_trickle_queue=$false;locale='en';map="Dimensions recorded per run, seed612345, recipe$RecipeVersion, current illustrated assets";recipe_version=$RecipeVersion;
    scope='Development-PC engineering benchmark; not minimum-hardware certification or physical display acceptance';runs=@()}
$previous=@{}
foreach($name in @('TTRPG_DATA_ROOT','TTRPG_DB_PATH','TTRPG_SETTINGS_PATH')) {$previous[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
try {
    foreach($scope in $Kinds) {
        $directory=Join-Path $output $scope;[IO.Directory]::CreateDirectory($directory)|Out-Null
        $env:TTRPG_DATA_ROOT=Join-Path $directory 'data';$env:TTRPG_DB_PATH=Join-Path $directory 'data/TTRPG.db';$env:TTRPG_SETTINGS_PATH=Join-Path $directory 'data/settings'
        $process=Start-Process -FilePath $exe -ArgumentList @($scope,('"'+$directory+'"'),[string]$RecipeVersion) -WorkingDirectory $build -WindowStyle Hidden -Wait -PassThru `
            -RedirectStandardOutput (Join-Path $directory 'result.log') -RedirectStandardError (Join-Path $directory 'stderr.log')
        $artifacts=@(Get-ChildItem -LiteralPath $directory -File|Sort-Object Name|ForEach-Object {
            [ordered]@{path=$_.Name;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
        })
        $manifest.runs+=[ordered]@{scope=$scope;dimensions=$(if($scope -in @('ruins','cave','dungeon')){'32x24'}else{'80x80'});exit_code=$process.ExitCode;passed=($process.ExitCode -eq 0);artifacts=$artifacts}
        $manifest|ConvertTo-Json -Depth 8|Set-Content (Join-Path $output 'manifest.json') -Encoding UTF8
        Write-Output "$scope exit code $($process.ExitCode)"
    }
}
finally {foreach($name in $previous.Keys) {[Environment]::SetEnvironmentVariable($name,$previous[$name],'Process')}}
if(@($manifest.runs|Where-Object {-not $_.passed}).Count) {throw 'One or more responsiveness workloads failed. Inspect the retained results.'}
