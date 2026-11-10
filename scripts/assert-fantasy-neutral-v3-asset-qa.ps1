param(
    [Parameter(Mandatory=$true)][string]$ReportPath,
    [Parameter(Mandatory=$true)][string]$DashboardPath,
    [int]$ExpectedMapCount = 10,
    [int]$ExpectedAssetCount = 424
)
$ErrorActionPreference = "Stop"
if (Test-Path -LiteralPath $ReportPath -PathType Container) {
    $ReportPath = (Get-ChildItem -LiteralPath $ReportPath -Recurse -Filter asset_qa_report.json |
        Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1).FullName
}
if (Test-Path -LiteralPath $DashboardPath -PathType Container) {
    $DashboardPath = (Get-ChildItem -LiteralPath $DashboardPath -Recurse -Filter index.html |
        Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1).FullName
}
if ([string]::IsNullOrWhiteSpace($ReportPath) -or -not (Test-Path -LiteralPath $ReportPath -PathType Leaf)) { throw "Missing asset QA report." }
if ([string]::IsNullOrWhiteSpace($DashboardPath) -or -not (Test-Path -LiteralPath $DashboardPath -PathType Leaf)) { throw "Missing asset QA dashboard." }
$report = Get-Content -Raw -LiteralPath $ReportPath | ConvertFrom-Json
if ([int]$report.schema_version -ne 1) { throw "Unexpected report schema." }
if ([int]$report.generated_map_count -ne $ExpectedMapCount) { throw "Unexpected map count." }
if ([int]$report.asset_count -ne $ExpectedAssetCount) { throw "Unexpected asset count." }
if (@($report.metrics).Count -ne $ExpectedAssetCount) { throw "Every asset must have metrics." }
if (@($report.coverage).Count -ne $ExpectedAssetCount) { throw "Every asset must have coverage evidence." }
if ([int]$report.review_requirements.required_map_count -ne $ExpectedMapCount) { throw "Without image triage every smoke map must require human review." }
if ([string]$report.review_requirements.map_review_mode -ne "all_maps_no_vision_triage") { throw "Unexpected human-review mode." }
$html = Get-Content -Raw -LiteralPath $DashboardPath
foreach ($required in @("review_decisions.json", "license_approved", "map_reviews", "reviewer", "FantasyNeutral V3")) {
    if (-not $html.Contains($required)) { throw "Dashboard is missing '$required'." }
}
Write-Host "FantasyNeutral V3 asset QA smoke assertions passed."
