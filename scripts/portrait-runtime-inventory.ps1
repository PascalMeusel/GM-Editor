# Shared strict inventory for source preflight and installed-package verification.
function Get-PortraitFileSha256 {
    param([Parameter(Mandatory)][string]$Path)
    $stream = [IO.File]::OpenRead($Path)
    $algorithm = [Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($algorithm.ComputeHash($stream)).Replace('-', '').ToLowerInvariant() }
    finally { $algorithm.Dispose(); $stream.Dispose() }
}

function Get-PortraitRuntimeInventory {
    param([Parameter(Mandatory)][string]$Root)
    $manifestPath = Join-Path $Root 'runtime_manifest.json'
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {throw "Portrait runtime manifest is missing: $manifestPath"}
    $manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
    $assets = @($manifest.assets)
    if ($manifest.schema_version -ne 1 -or $assets.Count -ne 94) {throw 'Portrait inventory must contain 94 versioned runtime sheets.'}
    $seen = @{}
    foreach ($asset in $assets) {
        $relative = [string]$asset.file
        if ($relative -cnotmatch '^(Distinct|Company)/[a-z_]+\.png$' -or $seen.ContainsKey($relative)) {throw "Invalid or duplicate portrait path: $relative"}
        $seen[$relative]=$true
        if ([string]$asset.sha256 -cnotmatch '^[0-9a-f]{64}$') {throw "Invalid portrait hash: $relative"}
        $path = Join-Path $Root $relative
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {throw "Runtime portrait missing: $relative"}
        if ((Get-PortraitFileSha256 -Path $path) -ne $asset.sha256) {throw "Runtime portrait hash mismatch: $relative"}
    }
    $legacy=@($assets | Where-Object {$_.file.StartsWith('Distinct/')} | ForEach-Object {$_.file.Substring(9)} | Sort-Object)
    $company=@($assets | Where-Object {$_.file.StartsWith('Company/')} | ForEach-Object {$_.file.Substring(8)} | Sort-Object)
    if ($legacy.Count -ne 47 -or $company.Count -ne 47 -or (Compare-Object $legacy $company)) {throw 'Portrait collections must cover the same 47 species.'}
    return @('runtime_manifest.json','README.md','prompts.json','Company/provenance.json') + @($assets | ForEach-Object {$_.file})
}
