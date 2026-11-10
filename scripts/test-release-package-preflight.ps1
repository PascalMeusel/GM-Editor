#requires -Version 5.1

[CmdletBinding(PositionalBinding = $false)]
param()

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

# Import only pure preflight helpers from the packager AST. Never execute the
# packaging entrypoint, replace a depot, or launch the application in this test.
$packageScript = Join-Path $PSScriptRoot "package-steam-windows.ps1"
$parseTokens = $null
$parseErrors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile($packageScript, [ref]$parseTokens, [ref]$parseErrors)
if ($parseErrors.Count -gt 0) { throw "Packager has PowerShell syntax errors: $($parseErrors[0].Message)" }
foreach ($name in @("Assert-FileExists", "Test-IsStrictChildPath", "Assert-ReleaseBuildConfiguration")) {
    $functions = @($ast.FindAll({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name }, $false))
    if ($functions.Count -ne 1) { throw "Expected exactly one preflight helper '$name'." }
    . ([scriptblock]::Create($functions[0].Extent.Text))
}

$testBase = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\', '/')
$testRoot = Join-Path $testBase ("gm-editor-package-preflight-" + [Guid]::NewGuid().ToString("N"))
$fixtureBuild = Join-Path $testRoot "build"
$fixtureProject = Join-Path $testRoot "source"
$fixtureExecutable = Join-Path $fixtureBuild "TTRPG_Editor.exe"
$cachePath = Join-Path $fixtureBuild "CMakeCache.txt"
$utf8 = New-Object Text.UTF8Encoding($false)
[IO.Directory]::CreateDirectory($fixtureBuild) | Out-Null
[IO.Directory]::CreateDirectory($fixtureProject) | Out-Null
$caseCount = 0

function Write-Cache([string]$Configuration, [string]$Source = $fixtureProject) {
    [IO.File]::WriteAllText($cachePath, "CMAKE_HOME_DIRECTORY:INTERNAL=$Source`r`n$Configuration`r`n", $utf8)
}
function Check-Pass([string]$Executable = $fixtureExecutable) {
    Assert-ReleaseBuildConfiguration -BuildRoot $fixtureBuild -ProjectRoot $fixtureProject -ExecutablePath $Executable
    $script:caseCount++
}
function Check-Failure([string]$Fragment, [string]$Executable = $fixtureExecutable) {
    try { Assert-ReleaseBuildConfiguration -BuildRoot $fixtureBuild -ProjectRoot $fixtureProject -ExecutablePath $Executable }
    catch {
        if ($_.Exception.Message -notlike "*$Fragment*") { throw "Expected '$Fragment', got '$($_.Exception.Message)'." }
        $script:caseCount++
        return
    }
    throw "Expected preflight rejection containing '$Fragment'."
}

try {
    Check-Failure "Required file is missing"
    Write-Cache "CMAKE_BUILD_TYPE:STRING=Release"
    Check-Pass
    Write-Cache "CMAKE_BUILD_TYPE:STRING=Debug"
    Check-Failure "not configured as Release"
    Write-Cache "CMAKE_BUILD_TYPE:STRING=Release" (Join-Path $testRoot "other-source")
    Check-Failure "different source checkout"
    [IO.File]::WriteAllText($cachePath, "CMAKE_BUILD_TYPE:STRING=Release`r`n", $utf8)
    Check-Failure "exactly one source checkout"
    Write-Cache "CMAKE_CONFIGURATION_TYPES:STRING=Debug;Release;RelWithDebInfo"
    Check-Failure "multi-configuration"
    Check-Pass (Join-Path $fixtureBuild "Release/TTRPG_Editor.exe")
    Write-Cache "CMAKE_CONFIGURATION_TYPES:STRING=Debug;RelWithDebInfo"
    Check-Failure "multi-configuration" (Join-Path $fixtureBuild "Release/TTRPG_Editor.exe")
    Write-Cache "CMAKE_BUILD_TYPE:STRING=Release"
    Check-Failure "outside its build directory" (Join-Path $testRoot "TTRPG_Editor.exe")
    Write-Cache "CMAKE_BUILD_TYPE:STRING=Release`r`nCMAKE_BUILD_TYPE:STRING=Debug"
    Check-Failure "not configured as Release"
    $readinessScript = Join-Path $PSScriptRoot "audit-release-local-readiness.ps1"
    $readinessCount = (Get-Command $readinessScript).Parameters["ExpectedCTestCount"]
    if (@($readinessCount.Attributes | Where-Object {
        $_ -is [Management.Automation.ParameterAttribute] -and $_.Mandatory
    }).Count) { throw "An omitted CTest count must fail without PowerShell prompting unattended callers." }
    $readinessAst = [Management.Automation.Language.Parser]::ParseFile($readinessScript, [ref]$parseTokens, [ref]$parseErrors)
    if ($parseErrors.Count -gt 0) { throw "Readiness audit has PowerShell syntax errors: $($parseErrors[0].Message)" }
    $countParameter = @($readinessAst.ParamBlock.Parameters | Where-Object { $_.Name.VariablePath.UserPath -eq "ExpectedCTestCount" })
    if ($countParameter.Count -ne 1 -or $countParameter[0].DefaultValue.SafeGetValue() -ne 0) {
        throw "Readiness audit must have an omitted-count sentinel, not a historical test-count default."
    }
    # Evaluate only the parameter block and first guard; no evidence reads,
    # packaging entrypoint or application process can execute in this fixture.
    $guard = $readinessAst.EndBlock.Statements[2]
    if ($guard -isnot [Management.Automation.Language.IfStatementAst]) { throw "Readiness count guard must precede all evidence operations." }
    $countContract = [scriptblock]::Create($readinessAst.ParamBlock.Extent.Text + "`r`n" + $guard.Extent.Text)
    $countArguments = @{
        InstalledSmokeReport = "unused"; BinaryAuditReport = "unused"; CTestLog = "unused"
        MapQaReport = "unused"; OutputPath = "unused"
    }
    $omittedCountRejected = $false
    try { & $countContract @countArguments }
    catch {
        if ($_.Exception.Message -notlike "*ExpectedCTestCount must be supplied explicitly*") { throw }
        $omittedCountRejected = $true
    }
    if (-not $omittedCountRejected) { throw "Readiness audit accepted an omitted CTest count." }
    $caseCount++
    & $countContract @countArguments -ExpectedCTestCount 55
    $caseCount++
    . (Join-Path $PSScriptRoot 'portrait-runtime-inventory.ps1')
    $portraitFixture=Join-Path $testRoot 'portraits'
    [IO.Directory]::CreateDirectory((Join-Path $portraitFixture 'Distinct')) | Out-Null
    [IO.Directory]::CreateDirectory((Join-Path $portraitFixture 'Company')) | Out-Null
    $portraitAssets=@(foreach ($collection in @('Distinct','Company')) {
        foreach ($index in 0..46) {
            $name='cast_'+[char](97+[int]($index/26))+[char](97+($index%26))+'.png'
            $relative="$collection/$name"; $path=Join-Path $portraitFixture $relative
            [IO.File]::WriteAllText($path,'fixture pixels',$utf8)
            # Known SHA-256 of the UTF-8 fixture, independent of the verifier.
            [pscustomobject]@{file=$relative;sha256='9804c5b214c3b092b4b98fcd4c9e85121540f7c703d44f3444fff34446b75123'}
        }
    })
    $portraitManifest=Join-Path $portraitFixture 'runtime_manifest.json'
    @{schema_version=1;assets=$portraitAssets}|ConvertTo-Json -Depth 4|Set-Content -LiteralPath $portraitManifest -Encoding UTF8
    if (@(Get-PortraitRuntimeInventory -Root $portraitFixture).Count -ne 98) {throw 'Complete portrait fixture was rejected.'}
    $caseCount++
    $firstPortrait=Join-Path $portraitFixture $portraitAssets[0].file
    [IO.File]::WriteAllText($firstPortrait,'tampered pixels',$utf8)
    $rejected=$false
    try {Get-PortraitRuntimeInventory -Root $portraitFixture|Out-Null} catch {if ($_.Exception.Message -notlike '*hash mismatch*') {throw};$rejected=$true}
    if (-not $rejected) {throw 'Tampered portrait was accepted.'};$caseCount++
    Remove-Item -LiteralPath $firstPortrait
    $rejected=$false
    try {Get-PortraitRuntimeInventory -Root $portraitFixture|Out-Null} catch {if ($_.Exception.Message -notlike '*portrait missing*') {throw};$rejected=$true}
    if (-not $rejected) {throw 'Missing portrait was accepted.'};$caseCount++
    $portraitAssets[0].file='../outside.png'
    @{schema_version=1;assets=$portraitAssets}|ConvertTo-Json -Depth 4|Set-Content -LiteralPath $portraitManifest -Encoding UTF8
    $rejected=$false
    try {Get-PortraitRuntimeInventory -Root $portraitFixture|Out-Null} catch {if ($_.Exception.Message -notlike '*Invalid or duplicate*') {throw};$rejected=$true}
    if (-not $rejected) {throw 'Escaping portrait path was accepted.'};$caseCount++
    Write-Host "release package preflight: $caseCount cases passed; no application launched"
}
finally {
    if (Test-Path -LiteralPath $testRoot) {
        $resolvedTestRoot = (Resolve-Path -LiteralPath $testRoot).Path.TrimEnd('\', '/')
        if (-not $resolvedTestRoot.Equals($testRoot, [StringComparison]::OrdinalIgnoreCase) -or
            -not (Test-IsStrictChildPath -Child $resolvedTestRoot -Parent $testBase) -or
            (Split-Path -Leaf $resolvedTestRoot) -notlike "gm-editor-package-preflight-*") {
            throw "Refusing to remove an unexpected test fixture path: $resolvedTestRoot"
        }
        Remove-Item -LiteralPath $resolvedTestRoot -Recurse -Force
    }
}
