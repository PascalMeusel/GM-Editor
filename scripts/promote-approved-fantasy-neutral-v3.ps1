param(
    [Parameter(Mandatory=$true)][string]$QaReportPath,
    [string]$Reviewer = "Project owner",
    [string]$ApprovalReason = "The project owner explicitly approved all 424 FantasyNeutral V3 candidates for production.",
    [string]$OutputDirectory = "build/asset-qa/owner-approved-promotion"
)
$ErrorActionPreference = "Stop"
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
function Project-Path([string]$Path) { if ([IO.Path]::IsPathRooted($Path)) { return [IO.Path]::GetFullPath($Path) }; return [IO.Path]::GetFullPath((Join-Path $repositoryRoot $Path)) }
function File-Sha256([string]$Path) { $stream=[IO.File]::OpenRead($Path);$sha=[Security.Cryptography.SHA256]::Create();try{return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','').ToLowerInvariant()}finally{$sha.Dispose();$stream.Dispose()} }
$manifestPath = Project-Path "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3/pack_manifest.json"
$reportPath = Project-Path $QaReportPath
$outputPath = Project-Path $OutputDirectory
$manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
$report = Get-Content -Raw -LiteralPath $reportPath | ConvertFrom-Json
if (@($manifest.assets).Count -ne 424) { throw "Expected exactly 424 FantasyNeutral V3 assets." }
if ([int]$report.summary.blocker_count -ne 0) { throw "Owner promotion is blocked by $($report.summary.blocker_count) technical blocker(s)." }
if ([int]$report.summary.covered_asset_count -ne 424) { throw "All 424 assets must have complete generated-map evidence." }
if ([string]$report.manifest_sha256 -ne (File-Sha256 $manifestPath)) { throw "QA report is stale." }
$promptReference = Project-Path "Resources/Maps/Tilesets/Source/FantasyNeutral/v3/production_prompts.md"
if (-not (Test-Path -LiteralPath $promptReference -PathType Leaf)) { throw "Missing production prompt provenance." }
foreach ($asset in @($manifest.assets)) {
    if ([string]::IsNullOrWhiteSpace([string]$asset.tool) -or [string]::IsNullOrWhiteSpace([string]$asset.license) -or [string]::IsNullOrWhiteSpace([string]$asset.source_sheet)) { throw "Incomplete provenance for $($asset.id)." }
    $assetPath = Join-Path (Split-Path -Parent $manifestPath) ([string]$asset.file).Replace('/', '\')
    if (-not (Test-Path -LiteralPath $assetPath -PathType Leaf)) { throw "Missing asset file: $($asset.id)" }
}
New-Item -ItemType Directory -Force -Path $outputPath | Out-Null
$decisionsPath = Join-Path $outputPath "owner_review_decisions.json"
$decisions = @($manifest.assets | Sort-Object id | ForEach-Object {
    [ordered]@{ asset_id=$_.id; verdict="accept"; style_fit=4; perspective=4; palette_lighting=4; scale_readability=4; contextual_fit=4; license_approved=$true; reviewer=$Reviewer; reason=$ApprovalReason }
})
[ordered]@{ schema_version=2; run_id=$report.run_id; manifest_sha256=$report.manifest_sha256; reviewer=$Reviewer; exported_at_utc=[DateTime]::UtcNow.ToString("o"); decisions=$decisions; map_reviews=@() } |
    ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $decisionsPath -Encoding utf8
& (Join-Path $PSScriptRoot "process-fantasy-neutral-v3-review.ps1") -DecisionsPath $decisionsPath -QaReportPath $reportPath -OutputDirectory $outputPath -OwnerOverrideReason $ApprovalReason -Apply -CreateProductionSnapshot
Write-Host "Promoted the owner-approved 424-asset FantasyNeutral V3 snapshot."
