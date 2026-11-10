#requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BuildDirectory,
    [Parameter(Mandatory)][string]$OutputRoot,
    [string]$ReferenceRoot,
    [switch]$SkipBuild
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$source = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..')).TrimEnd('\','/')
$output = [IO.Path]::GetFullPath($OutputRoot).TrimEnd('\','/')
$build = [IO.Path]::GetFullPath($BuildDirectory)
if ($output -eq $source -or $output.StartsWith($source + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Desktop evidence and generated campaign data must be outside the checkout.'
}
if (Test-Path -LiteralPath $output) { throw 'Use a new output directory to preserve earlier evidence.' }
if (-not [Environment]::UserInteractive) { throw 'Run desktop QA in the signed-in Windows session.' }
if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build-windows-msvc.ps1') -Configuration Release -BuildDir $build -Jobs 4 -Targets application_desktop_qa
}
$executable = Join-Path $build 'application_desktop_qa.exe'
if (-not (Test-Path -LiteralPath $executable)) { throw 'The application_desktop_qa target has not been built.' }
function Get-Sha256([string]$Path) {
    $sha = [Security.Cryptography.SHA256]::Create()
    $stream = [IO.File]::OpenRead($Path)
    try { return [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','') }
    finally { $stream.Dispose(); $sha.Dispose() }
}
[IO.Directory]::CreateDirectory($output) | Out-Null
$commit = (& git -C $source rev-parse HEAD).Trim()
if ($LASTEXITCODE) { throw 'Cannot bind source revision.' }
$status = @(& git -C $source status --porcelain)
$dependencies = @(& git -C $source submodule status --recursive)
if ($LASTEXITCODE) { throw 'Cannot bind dependency revisions.' }
# A local candidate may contain edits; record them explicitly and hash source
# files so that a base commit alone can never masquerade as the tested build.
$sourceFiles = @(& git -C $source ls-files --cached --others --exclude-standard)
$sourceInventory = @(foreach ($relative in ($sourceFiles | Sort-Object -Unique)) {
    $path = Join-Path $source $relative
    if (Test-Path -LiteralPath $path -PathType Leaf) {
        [ordered]@{path=$relative; sha256=(Get-Sha256 $path)}
    }
})
$sourceInventory | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $output 'source-inventory.json') -Encoding UTF8
$assets = Join-Path $build 'Resources'
$inventory = @(Get-ChildItem -LiteralPath $assets -File -Recurse | Sort-Object FullName | ForEach-Object {
    [ordered]@{path=$_.FullName.Substring($assets.Length+1).Replace('\','/'); sha256=(Get-Sha256 $_.FullName)}
})
$inventory | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $output 'assets.json') -Encoding UTF8
$assetHash = Get-Sha256 (Join-Path $output 'assets.json')
$executableHash = Get-Sha256 $executable
$catalog = @(& $executable --list-cases | ConvertFrom-Csv)
if ($LASTEXITCODE -or $catalog.Count -ne 184) { throw 'The authoritative visual matrix must contain 184 cases.' }
$catalog | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $output 'visual-catalog.json') -Encoding UTF8
$cases = [Collections.Generic.List[object]]::new()
foreach ($case in $catalog) {
    $cases.Add([ordered]@{id=$case.id; status='uncaptured'; reason='Requires its catalog fixture, state, viewport and desktop review; interaction-suite images do not substitute for this case.'})
}
$previousEnvironment = @{}
foreach ($name in @('TTRPG_DATA_ROOT','TTRPG_DB_PATH','TTRPG_SETTINGS_PATH')) { $previousEnvironment[$name] = [Environment]::GetEnvironmentVariable($name,'Process') }
$runs = [Collections.Generic.List[object]]::new()
try {
    foreach ($profile in @('standard','compact')) {
    foreach ($locale in @('en-US','de-DE')) {
        $directory = Join-Path $output ($profile + '/' + $locale)
        $data = Join-Path $directory 'data'
        [IO.Directory]::CreateDirectory($data) | Out-Null
        $env:TTRPG_DATA_ROOT = $data
        $env:TTRPG_DB_PATH = Join-Path $data 'TTRPG.db'
        $env:TTRPG_SETTINGS_PATH = Join-Path $data 'GMEditor.settings'
        $arguments = @(('"'+$directory+'"'),$locale)
        if ($profile -eq 'compact') { $arguments += '--compact' }
        $process = Start-Process -FilePath $executable -ArgumentList $arguments -WorkingDirectory $build -WindowStyle Hidden -Wait -PassThru `
            -RedirectStandardOutput (Join-Path $directory 'stdout.log') -RedirectStandardError (Join-Path $directory 'stderr.log')
        $runs.Add([ordered]@{profile=$profile; locale=$locale; exit_code=$process.ExitCode; passed=($process.ExitCode -eq 0)})
        if ($process.ExitCode -eq 0) {
            & (Join-Path $PSScriptRoot 'assert-player-framebuffers.ps1') -CaptureDirectory $directory
        }
        foreach ($bitmap in (Get-ChildItem -LiteralPath $directory -Filter capture.bmp -File -Recurse)) {
            $details = @{}
            foreach ($line in (Get-Content -LiteralPath (Join-Path $bitmap.DirectoryName 'capture.txt'))) {
                $parts = $line -split '=',2
                if ($parts.Count -eq 2) { $details[$parts[0]]=$parts[1] }
            }
            $cases.Add([ordered]@{
                id=('interaction__'+$bitmap.Directory.Name+'__'+$locale+'__'+$profile); status='captured'
                image=$bitmap.FullName.Substring($output.Length+1).Replace('\','/'); image_sha256=(Get-Sha256 $bitmap.FullName)
                commit=$commit; source_inventory_sha256=(Get-Sha256 (Join-Path $output 'source-inventory.json')); source_dirty=($status.Count -gt 0)
                executable_sha256=$executableHash; assets_sha256=$assetHash
                viewport=@($details.framebuffer -split 'x' | ForEach-Object { [int]$_ })
                locale=$locale; text_scale=([double]::Parse($details.text_scale,[Globalization.CultureInfo]::InvariantCulture)); dpi=@($details.content_scale -split ',' | ForEach-Object { [double]::Parse($_,[Globalization.CultureInfo]::InvariantCulture) })
                renderer=($details.vendor+' / '+$details.renderer+' / '+$details.version)
            })
        }
    }
    }
} finally {
    foreach ($name in $previousEnvironment.Keys) { [Environment]::SetEnvironmentVariable($name,$previousEnvironment[$name],'Process') }
}
$passed = @($runs | Where-Object { -not $_.passed }).Count -eq 0 -and $runs.Count -eq 4
[ordered]@{commit=$commit; source_status=$status; dependencies=$dependencies; executable_sha256=$executableHash;
    created_utc=[DateTime]::UtcNow.ToString('o'); interaction_runs=$runs.ToArray(); interaction_passed=$passed;
    catalog_count=$catalog.Count; reference_approval_modified=$false; cases=$cases.ToArray()} |
    ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $output 'captures.json') -Encoding UTF8
$arguments = @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+(Join-Path $PSScriptRoot 'compare-visual-captures.ps1')+'"'),
    '-CandidateRoot',('"'+$output+'"'),'-ReportPath',('"'+(Join-Path $output 'comparison.json')+'"'))
if ($ReferenceRoot) { $arguments += @('-ReferenceRoot',('"'+[IO.Path]::GetFullPath($ReferenceRoot)+'"')) }
$comparison = Start-Process -FilePath powershell -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru `
    -RedirectStandardOutput (Join-Path $output 'comparison.stdout.log') -RedirectStandardError (Join-Path $output 'comparison.stderr.log')
if ($comparison.ExitCode -notin @(0,2)) { throw "Visual comparison could not validate the bundle: $output" }
if (-not $passed) { throw "Actual application interaction failed; inspect locale logs in $output" }
Write-Output "English/German application interactions passed. Evidence: $output"
Write-Output 'Reference comparisons and the 184-case acceptance matrix have separate results; no human approvals were created.'
