param([Parameter(Mandatory)][ValidateRange(1,10)][int]$Batch)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars=Join-Path $vs 'VC/Auxiliary/Build/vcvars64.bat'
$compilerEnvironment=& $env:ComSpec /d /c "call `"$vcvars`" >nul && set"
foreach($entry in $compilerEnvironment) { if($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process') } }
$output=Join-Path $root 'docs/character-review'
[IO.Directory]::CreateDirectory($output)|Out-Null
$build=Join-Path $root 'build-character-review'
[IO.Directory]::CreateDirectory($build)|Out-Null
Push-Location $build
try {
    & cl /nologo /EHsc /std:c++17 /utf-8 "/I$root" (Join-Path $root 'tools/character_generation_review.cpp') /Fe:character_generation_review.exe
    if($LASTEXITCODE) { throw 'Review build failed' }
    & ./character_generation_review.exe $Batch (Join-Path $output "batch-$Batch.md")
    if($LASTEXITCODE) { throw 'Review batch failed' }
} finally { Pop-Location }
