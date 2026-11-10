param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = "Debug",
    [string]$BuildDir = "build",
    [ValidateRange(1,4)][int]$Jobs = 4,
    [string[]]$Targets = @(),
    [switch]$ConfigureOnly,
    [switch]$StaticRuntime
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"

if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "Could not find vswhere.exe. Install Visual Studio with the Desktop development with C++ workload."
}

$vsPath = & $vswhere -latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) {
    throw "Could not find Visual Studio C++ build tools. Install the Desktop development with C++ workload."
}

$vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path -LiteralPath $vcvars)) {
    throw "Could not find vcvars64.bat at: $vcvars"
}

$buildPath = if ([IO.Path]::IsPathRooted($BuildDir)) { [IO.Path]::GetFullPath($BuildDir) } else { [IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDir)) }
# Import the compiler environment without embedding build paths in shell code.
$compilerEnvironment = & $env:ComSpec /d /c "call `"$vcvars`" >nul && set"
if ($LASTEXITCODE) { throw 'Visual Studio environment initialization failed.' }
foreach ($entry in $compilerEnvironment) {
    if ($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') }
}
$ninjaDirectory = Join-Path $vsPath 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja'
$env:PATH = "$ninjaDirectory;$env:PATH"
$configureArguments = @('-S', $repoRoot, '-B', $buildPath, '-G', 'Ninja', "-DCMAKE_BUILD_TYPE=$Configuration")
if ($StaticRuntime) {
    $configureArguments += '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>'
}
& cmake @configureArguments
if ($LASTEXITCODE) { throw 'CMake configuration failed.' }
if (-not $ConfigureOnly) {
    $buildArguments = @('--build', $buildPath, '--parallel', $Jobs)
    if ($Targets.Count) { $buildArguments += '--target'; $buildArguments += $Targets }
    & cmake @buildArguments
    if ($LASTEXITCODE -and $Jobs -gt 1) {
        Write-Warning 'Parallel CMake build failed; retrying unfinished work with one compiler process.'
        $retryArguments = @('--build', $buildPath, '--parallel', '1')
        if ($Targets.Count) { $retryArguments += '--target'; $retryArguments += $Targets }
        & cmake @retryArguments
    }
    if ($LASTEXITCODE) { throw 'CMake build failed, including the serial retry.' }
}
