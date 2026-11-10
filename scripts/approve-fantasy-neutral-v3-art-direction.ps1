param(
    [Parameter(Mandatory=$true)][string]$QaReportPath,
    [Parameter(Mandatory=$true)][string]$Reviewer,
    [Parameter(Mandatory=$true)][string]$Reason,
    [string]$QaProfilePath = "Resources/Maps/Tilesets/Source/FantasyNeutral/v3/qa_profile.json",
    [switch]$Apply
)

$ErrorActionPreference = "Stop"
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
function Resolve-ProjectPath([string]$Path) {
    if ([IO.Path]::IsPathRooted($Path)) { return [IO.Path]::GetFullPath($Path) }
    return [IO.Path]::GetFullPath((Join-Path $repositoryRoot $Path))
}

$profileFile = Resolve-ProjectPath $QaProfilePath
$reportFile = Resolve-ProjectPath $QaReportPath
$profile = Get-Content -Raw -LiteralPath $profileFile | ConvertFrom-Json
$report = Get-Content -Raw -LiteralPath $reportFile | ConvertFrom-Json
if ([string]$report.art_direction_id -ne [string]$profile.art_direction_id) { throw "QA report uses another art direction." }
if (@($profile.provisional_reference_asset_ids).Count -lt 16) { throw "At least 16 reference assets are required." }
if (@($profile.reference_maps).Count -lt 5) { throw "Five reference maps are required." }

$assetIds = @($report.metrics | Select-Object -ExpandProperty asset_id)
$missingAssets = @($profile.provisional_reference_asset_ids | Where-Object { $_ -notin $assetIds })
if ($missingAssets.Count -gt 0) { throw "Reference assets missing from QA evidence: $($missingAssets -join ', ')" }
$referenceSeeds = @($report.maps | Select-Object -ExpandProperty seed)
$missingMaps = @($profile.reference_maps | Where-Object { [int]$_.seed -notin $referenceSeeds })
if ($missingMaps.Count -gt 0) { throw "Reference maps missing from QA evidence. Run the canonical 1,000-map corpus first." }
$referenceBlockers = @($report.findings | Where-Object { $_.severity -eq "blocker" -and $_.asset_id -in $profile.provisional_reference_asset_ids })
if ($referenceBlockers.Count -gt 0) { throw "Reference assets still have blocking QA findings." }

$decision = [pscustomobject]@{ reviewer=$Reviewer; reason=$Reason; approved_at_utc=[DateTime]::UtcNow.ToString("o"); qa_run_id=$report.run_id; manifest_sha256=$report.manifest_sha256 }
if (-not $Apply) {
    Write-Host "Art direction can be approved. Re-run with -Apply after reviewing the five reference maps and sixteen reference assets."
    $decision | ConvertTo-Json -Depth 5
    exit 0
}
$profile.approval_status = "approved"
$profile.approved_reference_asset_ids = @($profile.provisional_reference_asset_ids)
foreach ($map in @($profile.reference_maps)) { $map.approved = $true }
if ($null -eq $profile.PSObject.Properties["approval_history"]) { $profile | Add-Member -NotePropertyName approval_history -NotePropertyValue @() }
$profile.approval_history = @($profile.approval_history) + @($decision)
$profile | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $profileFile -Encoding utf8
Write-Host "Approved art direction $($profile.art_direction_id) for the reviewed QA run."

