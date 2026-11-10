#requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BuildDirectory,
    [Parameter(Mandatory)][string]$OutputRoot,
    [ValidateSet(3,4,5,6,7,8,9,10,11)][int]$RecipeVersion = 11,
    [switch]$AllowDirty
)
$ErrorActionPreference='Stop'
$source=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..')).TrimEnd('\','/')
$output=[IO.Path]::GetFullPath($OutputRoot).TrimEnd('\','/')
$build=[IO.Path]::GetFullPath($BuildDirectory)
if($output -eq $source -or $output.StartsWith($source+'\',[StringComparison]::OrdinalIgnoreCase)) {throw 'Keep gallery evidence outside the checkout.'}
if(Test-Path -LiteralPath $output) {throw 'Use a new evidence directory.'}
$status=@(& git -C $source status --porcelain)
if($status.Count -and -not $AllowDirty) {throw 'Commit the source before producing acceptance evidence.'}
$exe=Join-Path $build 'map_generation_preview_export.exe'
if(-not(Test-Path -LiteralPath $exe)) {throw 'Build map_generation_preview_export first.'}
[IO.Directory]::CreateDirectory($output)|Out-Null
$cases=@()
foreach($kind in @('world','region','tactical')) {
    foreach($seed in @(612345,612346)) {$cases+=[ordered]@{group='fixed';kind=$kind;seed=$seed}}
}
# This random sample was frozen before inspecting any baseline outputs on
# 2026-09-13. Reuse every seed; never resample or drop an unattractive result.
$sample=@{world=@(2631136792,2609465013,1532199059);region=@(1001953925,1038803534,3297445993);tactical=@(3762563748,1762090474,464468523)}
foreach($kind in @('world','region','tactical')) {
    foreach($seed in $sample[$kind]) {$cases+=[ordered]@{group='random';kind=$kind;seed=$seed}}
}
$cases|ConvertTo-Json -Depth 4|Set-Content (Join-Path $output 'seeds.json') -Encoding UTF8
$assets=Join-Path $build 'Resources'
@(Get-ChildItem -LiteralPath $assets -File -Recurse|Sort-Object FullName|ForEach-Object {
    [ordered]@{path=$_.FullName.Substring($assets.Length+1);sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
})|ConvertTo-Json -Depth 4|Set-Content (Join-Path $output 'assets.json') -Encoding UTF8
$manifest=[ordered]@{commit=(& git -C $source rev-parse HEAD).Trim();dirty=$status;recipe_version=$RecipeVersion;
    dependencies=@(& git -C $source submodule status --recursive);executable_sha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash;
    assets_sha256=(Get-FileHash -LiteralPath (Join-Path $output 'assets.json') -Algorithm SHA256).Hash;
    human_art_approval='pending';cases=@()}
$previous=@{}
foreach($name in @('TTRPG_DATA_ROOT','TTRPG_DB_PATH','TTRPG_SETTINGS_PATH')) {$previous[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
try {
    $env:TTRPG_DATA_ROOT=Join-Path $output 'data';$env:TTRPG_DB_PATH=Join-Path $output 'data/TTRPG.db';$env:TTRPG_SETTINGS_PATH=Join-Path $output 'data/settings'
    foreach($case in $cases) {
        $directory=Join-Path $output ($case.group+'/'+$case.kind+'-'+$case.seed)
        [IO.Directory]::CreateDirectory($directory)|Out-Null
        $version=if($case.kind -eq 'world'){11}else{12}
        $scope=if($case.kind -eq 'world'){'world'}else{'local'}
        $preset=if($case.kind -eq 'tactical'){'tactical'}else{'balanced'}
        $width=if($case.kind -eq 'tactical'){24}else{40};$height=if($case.kind -eq 'tactical'){24}else{30}
        $arguments=@('--out',('"'+(Join-Path $directory 'map.json')+'"'),'--png',('"'+(Join-Path $directory 'map.png')+'"'),
            '--generator-version',$version,'--recipe-version',$RecipeVersion,'--style-id','illustrated_atlas','--scope',$scope,'--preset',$preset,
            '--width',$width,'--height',$height,'--seed',$case.seed,'--quality-attempts',12,'--pixels-per-cell',32,'--locale','en','--mountains',40)
        $process=Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $build -WindowStyle Hidden -Wait -PassThru `
            -RedirectStandardOutput (Join-Path $directory 'stdout.log') -RedirectStandardError (Join-Path $directory 'stderr.log')
        $record=[ordered]@{group=$case.group;kind=$case.kind;seed=$case.seed;arguments=$arguments;exit_code=$process.ExitCode;png_sha256=$null}
        if($process.ExitCode -eq 0) {$record.png_sha256=(Get-FileHash -LiteralPath (Join-Path $directory 'map.png') -Algorithm SHA256).Hash}
        $manifest.cases+=$record
        $manifest|ConvertTo-Json -Depth 8|Set-Content (Join-Path $output 'manifest.json') -Encoding UTF8
        if($process.ExitCode) {throw "Gallery failed: $directory"}
        Write-Output "Captured $($case.group) $($case.kind) $($case.seed)"
    }
}
finally {foreach($name in $previous.Keys) {[Environment]::SetEnvironmentVariable($name,$previous[$name],'Process')}}
