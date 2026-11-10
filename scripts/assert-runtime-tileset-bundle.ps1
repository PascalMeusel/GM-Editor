[CmdletBinding(PositionalBinding = $false)]
param(
    [Parameter(Mandatory = $true)]
    [string]$BundleRoot
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")

$bundleRootPath = Resolve-TilesetPath $BundleRoot
if (-not (Test-Path -LiteralPath $bundleRootPath)) {
    throw "Missing runtime tileset bundle root: $bundleRootPath"
}

$requiredPaths = @(
    "Styles/fantasy_neutral.json",
    "Source/FantasyNeutral/v2/ways.json",
    "Source/FantasyNeutral/v2/taxonomy.json",
    "Generated/FantasyNeutral/v2/packed_runtime_manifest.json",
    "Generated/FantasyNeutral/v2/packed_candidate_manifest.json"
)

foreach ($relativePath in $requiredPaths) {
    $path = Join-Path $bundleRootPath $relativePath
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Runtime tileset bundle is missing: $path"
    }
}

$candidateManifestPath = Join-Path $bundleRootPath "Generated/FantasyNeutral/v2/packed_candidate_manifest.json"
$candidateManifest = Get-Content -Raw -LiteralPath $candidateManifestPath | ConvertFrom-Json
$candidateAssets = @(ConvertTo-TilesetArray (Get-TilesetProperty $candidateManifest "assets" @()))
$candidateCount = @($candidateAssets | Where-Object { (Get-TilesetProperty $_ "status" "") -eq "candidate" }).Count
if ($candidateCount -le 0) {
    throw "Expected bundled packed_candidate_manifest.json to contain candidate assets"
}

Write-Host "Runtime tileset bundle OK"
Write-Host "Bundle root: $(ConvertTo-TilesetRelativePath $bundleRootPath)"
Write-Host "Candidate assets: $candidateCount"
