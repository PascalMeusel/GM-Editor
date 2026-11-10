#requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BuildDirectory,
    [Parameter(Mandatory)][string]$OutputRoot,
    [ValidateRange(1280,7680)][int]$Width=1280,
    [ValidateRange(720,4320)][int]$Height=900
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$source=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..')).TrimEnd('\','/')
$build=[IO.Path]::GetFullPath($BuildDirectory)
$output=[IO.Path]::GetFullPath($OutputRoot).TrimEnd('\','/')
if($output -eq $source -or $output.StartsWith($source+'\',[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Keep captures and generated campaign data outside the checkout.'
}
if(Test-Path -LiteralPath $output){throw 'Use a new evidence directory.'}
if(-not [Environment]::UserInteractive){throw 'Run in the signed-in Windows desktop session.'}
if(@(git -C $source status --porcelain).Count){throw 'Commit the reviewed source before binding captures.'}
$commit=(git -C $source rev-parse HEAD).Trim()
if($LASTEXITCODE){throw 'Cannot identify source revision.'}
$executable=Join-Path $build 'creation_workspace_qa.exe'
if(-not(Test-Path -LiteralPath $executable)){throw 'Build the creation_workspace_qa target first.'}
$previous=@{}
foreach($name in @('TTRPG_DATA_ROOT','TTRPG_DB_PATH','TTRPG_SETTINGS_PATH')) {
    $previous[$name]=[Environment]::GetEnvironmentVariable($name,'Process')
}
$captures=[Collections.Generic.List[object]]::new()
[IO.Directory]::CreateDirectory($output)|Out-Null
try {
    foreach($locale in @('en-US','de-DE')) {
        $directory=Join-Path $output $locale
        $env:TTRPG_DATA_ROOT=Join-Path $directory 'data'
        $env:TTRPG_DB_PATH=Join-Path $env:TTRPG_DATA_ROOT 'TTRPG.db'
        $env:TTRPG_SETTINGS_PATH=Join-Path $env:TTRPG_DATA_ROOT 'GMEditor.settings'
        [IO.Directory]::CreateDirectory($directory)|Out-Null
        $process=Start-Process -FilePath $executable -ArgumentList @(('"'+$directory+'"'),$locale,$Width,$Height) -WorkingDirectory $build -WindowStyle Hidden -Wait -PassThru `
            -RedirectStandardOutput (Join-Path $directory 'stdout.log') -RedirectStandardError (Join-Path $directory 'stderr.log')
        if($process.ExitCode){throw "Creation gallery failed for $locale."}
        $images=@(Get-ChildItem -LiteralPath $directory -Filter capture.bmp -File -Recurse)
        if($images.Count -ne 16){throw 'Incomplete creation gallery.'}
        foreach($bitmap in $images) {
            $details=Get-Content -LiteralPath (Join-Path $bitmap.DirectoryName 'capture.txt')
            if($details -notcontains "framebuffer=${Width}x${Height}"){throw 'Actual framebuffer differs from requested viewport.'}
            $captures.Add([ordered]@{locale=$locale;view=$bitmap.Directory.Name;sha256=(Get-FileHash -LiteralPath $bitmap.FullName).Hash;details=$details})
        }
    }
    $resources=@(Get-ChildItem -LiteralPath (Join-Path $build 'Resources') -File -Recurse | ForEach-Object {
        [ordered]@{path=$_.FullName.Substring($build.Length).TrimStart('\','/');sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
    })
    [ordered]@{commit=$commit;dependencies=@(git -C $source submodule status --recursive);
        executable_sha256=(Get-FileHash -LiteralPath $executable).Hash;
        application_sha256=(Get-FileHash -LiteralPath (Join-Path $build 'TTRPG_Editor.exe')).Hash;
        width=$Width;height=$Height;captures=$captures;runtime_resources=$resources;
        coverage='QA-directed production views; actual input coverage is reported by run-desktop-qa and workspace_ui_interaction';
        reference_approval=$false;completed_utc=[DateTime]::UtcNow.ToString('o')} |
        ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $output 'manifest.json') -Encoding UTF8
} finally {
    foreach($entry in $previous.GetEnumerator()){[Environment]::SetEnvironmentVariable($entry.Key,$entry.Value,'Process')}
}
