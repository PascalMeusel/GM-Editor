#requires -Version 5.1
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$testBase = if ($env:TTRPG_DB_PATH) { Split-Path -Parent $env:TTRPG_DB_PATH } else { [IO.Path]::GetTempPath() }
$testRoot = Join-Path $testBase ('gm-visual-comparison-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($testRoot) | Out-Null
function Get-Sha256([string]$Path) {
    $sha = [Security.Cryptography.SHA256]::Create()
    $stream = [IO.File]::OpenRead($Path)
    try { return [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','') }
    finally { $stream.Dispose(); $sha.Dispose() }
}
function Write-Bitmap([string]$Path, [byte]$Blue=5) {
    $stream = [IO.File]::Create($Path)
    $writer = New-Object IO.BinaryWriter($stream)
    try {
        $writer.Write([byte]66); $writer.Write([byte]77); $writer.Write([int]58); $writer.Write([int]0); $writer.Write([int]54)
        $writer.Write([int]40); $writer.Write([int]1); $writer.Write([int]1); $writer.Write([int16]1); $writer.Write([int16]24)
        foreach ($value in @(0,4,0,0,0,0)) { $writer.Write([int]$value) }
        $writer.Write($Blue); $writer.Write([byte]10); $writer.Write([byte]15); $writer.Write([byte]0)
    } finally { $writer.Dispose() }
}
$script:checked = 0
function Check-Comparison([string]$Name, [scriptblock]$Change, [int]$ExpectedExit, [string]$ExpectedStatus='') {
    $directory = Join-Path $testRoot $Name
    $candidateRoot = Join-Path $directory 'candidate'; $referenceRoot = Join-Path $directory 'reference'
    [IO.Directory]::CreateDirectory($candidateRoot) | Out-Null
    [IO.Directory]::CreateDirectory($referenceRoot) | Out-Null
    Write-Bitmap (Join-Path $candidateRoot 'test.bmp')
    Write-Bitmap (Join-Path $referenceRoot 'test.bmp')
    $hash = (Get-Sha256 (Join-Path $candidateRoot 'test.bmp'))
    $candidate = [pscustomobject]@{ id='synthetic-unit-case'; status='captured'; image='test.bmp'; image_sha256=$hash;
        commit=('a'*40); executable_sha256=('b'*64); assets_sha256=('c'*64); viewport=@(1,1); locale='en-US'; dpi=@(1,1); renderer='synthetic-unit-renderer' }
    $reference = $candidate | ConvertTo-Json -Depth 6 | ConvertFrom-Json
    $reference | Add-Member NoteProperty approval ([pscustomobject]@{reviewer='synthetic unit-test control'; approved_at_utc='2026-01-01T00:00:00Z'; image_sha256=$hash})
    $report = Join-Path $directory 'comparison.json'
    & $Change $candidate $reference $candidateRoot $referenceRoot
    [ordered]@{cases=@($candidate)} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $candidateRoot 'captures.json') -Encoding UTF8
    [ordered]@{cases=@($reference)} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $referenceRoot 'captures.json') -Encoding UTF8
    $referenceBefore = (Get-Sha256 (Join-Path $referenceRoot 'captures.json'))
    $arguments = @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"' + (Join-Path $PSScriptRoot 'compare-visual-captures.ps1') + '"'),
        '-CandidateRoot',('"' + $candidateRoot + '"'),'-ReferenceRoot',('"' + $referenceRoot + '"'),'-ReportPath',('"' + $report + '"'))
    $process = Start-Process -FilePath powershell -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru `
        -RedirectStandardOutput (Join-Path $directory 'stdout.log') -RedirectStandardError (Join-Path $directory 'stderr.log')
    if ($process.ExitCode -ne $ExpectedExit) { throw "$Name returned $($process.ExitCode), expected $ExpectedExit; evidence: $directory" }
    if ($ExpectedStatus) {
        $result = Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
        if ($result.cases[0].status -ne $ExpectedStatus) { throw "$Name has wrong comparison state: $($result.cases[0].status)" }
        if ($result.all_cases_matched -ne ($ExpectedStatus -eq 'match')) { throw "$Name has wrong overall result" }
    }
    if ((Get-Sha256 (Join-Path $referenceRoot 'captures.json')) -ne $referenceBefore) { throw "$Name modified reference data" }
    ++$script:checked
}
Check-Comparison 'identical' {} 0 'match'
Check-Comparison 'changed-pixels' { param($c,$r,$cr,$rr) Write-Bitmap (Join-Path $cr 'test.bmp') 25; $c.image_sha256=(Get-Sha256 (Join-Path $cr 'test.bmp')) } 2 'different'
Check-Comparison 'uncaptured' { param($c,$r) $c.status='uncaptured' } 2 'uncaptured'
Check-Comparison 'unapproved' { param($c,$r) $r.PSObject.Properties.Remove('approval') } 2 'needs-reference'
Check-Comparison 'stale-approval' { param($c,$r) $r.approval.image_sha256='stale' } 2 'needs-reference'
Check-Comparison 'locale' { param($c,$r) $c.locale='de-DE' } 2 'incompatible-reference'
Check-Comparison 'dpi' { param($c,$r) $c.dpi=@(1.5,1.5) } 2 'incompatible-reference'
Check-Comparison 'driver' { param($c,$r) $c.renderer='other-driver' } 2 'incompatible-reference'
Check-Comparison 'assets' { param($c,$r) $c.assets_sha256=('d'*64) } 2 'incompatible-reference'
Check-Comparison 'missing-binding' { param($c,$r) $c.PSObject.Properties.Remove('commit') } 1
Check-Comparison 'false-dimensions' { param($c,$r) $c.viewport=@(2,1) } 1
Check-Comparison 'tampered-image' { param($c,$r,$cr,$rr) [IO.File]::WriteAllText((Join-Path $cr 'test.bmp'),'not a framebuffer') } 1
Check-Comparison 'path-escape' { param($c,$r) $c.image='../reference/test.bmp' } 1

Check-Comparison 'different-text-scale' { param($c,$r,$cd,$rd)
    $c | Add-Member NoteProperty text_scale 1.5
    $r | Add-Member NoteProperty text_scale 1.0
} 2 'incompatible-reference'

Write-Output "$script:checked native Windows visual comparison controls passed. Synthetic evidence: $testRoot"
