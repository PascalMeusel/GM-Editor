param(
    [Parameter(Mandatory=$true)][string]$ReportRoot,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [int]$ExpectedMapCount = 10
)
$ErrorActionPreference = "Stop"
$reportFile = (Get-ChildItem -LiteralPath $ReportRoot -Recurse -Filter asset_qa_report.json |
    Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1).FullName
if ([string]::IsNullOrWhiteSpace($reportFile)) { throw "Missing asset QA smoke report." }
$report = Get-Content -Raw -LiteralPath $reportFile | ConvertFrom-Json
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$decisionsFile = Join-Path $OutputDirectory "empty_review_decisions.json"
[ordered]@{
    schema_version=2; run_id=$report.run_id; manifest_sha256=$report.manifest_sha256
    reviewer="Automated review-gate test"; exported_at_utc="2000-01-01T00:00:00Z"
    decisions=@(); map_reviews=@()
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $decisionsFile -Encoding utf8
& (Join-Path $PSScriptRoot "process-fantasy-neutral-v3-review.ps1") -DecisionsPath $decisionsFile -QaReportPath $reportFile -OutputDirectory $OutputDirectory
$validation = Get-Content -Raw -LiteralPath (Join-Path $OutputDirectory "review_validation.json") | ConvertFrom-Json
$receipt = Get-Content -Raw -LiteralPath (Join-Path $OutputDirectory "review_integrity_receipt.json") | ConvertFrom-Json
if ([bool]$validation.map_reviews_complete) { throw "An empty map review must not pass." }
if ([int]$validation.map_review_error_count -ne $ExpectedMapCount) { throw "Every untriaged map must block the full-map gate." }
if ([string]$receipt.algorithm -ne "sha256" -or ([string]$receipt.integrity_signature).Length -ne 64) { throw "Missing signed review integrity receipt." }
Write-Host "FantasyNeutral V3 review gates passed."
