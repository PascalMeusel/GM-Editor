param(
    [Parameter(Mandatory=$true)][string]$DecisionsPath,
    [Parameter(Mandatory=$true)][string]$QaReportPath,
    [string]$ManifestPath = "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3/pack_manifest.json",
    [string]$QaProfilePath = "Resources/Maps/Tilesets/Source/FantasyNeutral/v3/qa_profile.json",
    [string]$OutputDirectory = "build/asset-qa/review-results",
    [string]$OwnerOverrideReason = "",
    [switch]$Apply,
    [switch]$CreateProductionSnapshot
)

$ErrorActionPreference = "Stop"
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
function Resolve-ProjectPath([string]$Path) {
    if ([IO.Path]::IsPathRooted($Path)) { return [IO.Path]::GetFullPath($Path) }
    return [IO.Path]::GetFullPath((Join-Path $repositoryRoot $Path))
}
function Clean-Token([string]$Value) { return ($Value -replace '[^a-zA-Z0-9_.-]', '_') }
function Get-Sha256([string]$Path) {
    $stream = [IO.File]::OpenRead($Path); $sha = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '').ToLowerInvariant() }
    finally { $sha.Dispose(); $stream.Dispose() }
}
function Get-TextSha256([string]$Value) {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($Value)))).Replace('-', '').ToLowerInvariant() }
    finally { $sha.Dispose() }
}

$manifestFile = Resolve-ProjectPath $ManifestPath
$decisionsFile = Resolve-ProjectPath $DecisionsPath
$reportFile = Resolve-ProjectPath $QaReportPath
$profileFile = Resolve-ProjectPath $QaProfilePath
$outputPath = Resolve-ProjectPath $OutputDirectory
New-Item -ItemType Directory -Force -Path $outputPath | Out-Null

$manifest = Get-Content -Raw -LiteralPath $manifestFile | ConvertFrom-Json
$decisions = Get-Content -Raw -LiteralPath $decisionsFile | ConvertFrom-Json
$qa = Get-Content -Raw -LiteralPath $reportFile | ConvertFrom-Json
$profile = Get-Content -Raw -LiteralPath $profileFile | ConvertFrom-Json
$manifestHash = Get-Sha256 $manifestFile
if ([string]$decisions.manifest_sha256 -ne $manifestHash) { throw "Review decisions target a different manifest hash." }
if ([string]$qa.manifest_sha256 -ne $manifestHash) { throw "QA report targets a different manifest hash." }
if ([string]$decisions.run_id -ne [string]$qa.run_id) { throw "Review decisions and QA report have different run IDs." }
if ([string]::IsNullOrWhiteSpace([string]$decisions.reviewer)) { throw "The review export must name its reviewer." }

$knownMapIds = @{}
foreach ($map in @($qa.maps)) { $knownMapIds[[string]$map.id] = $true }
$mapReviewsById = @{}
foreach ($mapReview in @($decisions.map_reviews)) {
    $mapId = [string]$mapReview.map_id
    if (-not $knownMapIds.ContainsKey($mapId)) { throw "Unknown reviewed map ID: $mapId" }
    if ($mapReviewsById.ContainsKey($mapId)) { throw "Duplicate map review: $mapId" }
    $mapReviewsById[$mapId] = $mapReview
}
$mapReviewErrors = [Collections.Generic.List[string]]::new()
foreach ($mapId in @($qa.review_requirements.required_map_ids)) {
    if (-not $mapReviewsById.ContainsKey([string]$mapId)) { $mapReviewErrors.Add("Required map was not reviewed: $mapId"); continue }
    $review = $mapReviewsById[[string]$mapId]
    if (-not [bool]$review.reviewed) { $mapReviewErrors.Add("Required map was not marked reviewed: $mapId"); continue }
    foreach ($rating in @("style", "composition", "readability")) {
        if ([int]$review.$rating -lt [int]$profile.human_gate.minimum_rating_per_criterion) { $mapReviewErrors.Add("Map $mapId has a '$rating' rating below the gate.") }
    }
}
$mapReviewsComplete = ($mapReviewErrors.Count -eq 0 -and $mapReviewsById.Count -ge [int]$qa.review_requirements.minimum_full_map_reviews)
$ownerOverrideApplied = -not [string]::IsNullOrWhiteSpace($OwnerOverrideReason)

$assetsById = @{}
foreach ($asset in @($manifest.assets)) { $assetsById[[string]$asset.id] = $asset }
$findingsByAsset = @{}
foreach ($finding in @($qa.findings)) {
    $id = [string]$finding.asset_id
    if ([string]::IsNullOrWhiteSpace($id)) { continue }
    if (-not $findingsByAsset.ContainsKey($id)) { $findingsByAsset[$id] = @() }
    $findingsByAsset[$id] += $finding
}

$errors = [Collections.Generic.List[string]]::new()
$workOrders = [Collections.Generic.List[object]]::new()
$applied = [Collections.Generic.List[object]]::new()
$seen = @{}
foreach ($decision in @($decisions.decisions)) {
    $id = [string]$decision.asset_id
    if (-not $assetsById.ContainsKey($id)) { $errors.Add("Unknown asset ID: $id"); continue }
    if ($seen.ContainsKey($id)) { $errors.Add("Duplicate decision for asset: $id"); continue }
    $seen[$id] = $true
    $verdict = [string]$decision.verdict
    if ($verdict -notin @("pending", "accept", "repaint", "replace")) { $errors.Add("Invalid verdict '$verdict' for $id"); continue }
    if ($verdict -eq "pending") { continue }
    $reviewer = if (-not [string]::IsNullOrWhiteSpace([string]$decision.reviewer)) { [string]$decision.reviewer } else { [string]$decisions.reviewer }
    if ([string]::IsNullOrWhiteSpace($reviewer)) { $errors.Add("Reviewer is required for $id"); continue }
    $assetFindings = if ($findingsByAsset.ContainsKey($id)) { @($findingsByAsset[$id]) } else { @() }
    $blockers = @($assetFindings | Where-Object severity -eq "blocker")
    if ($verdict -eq "accept") {
        foreach ($rating in @("style_fit", "perspective", "palette_lighting", "scale_readability", "contextual_fit")) {
            if ([int]$decision.$rating -lt [int]$profile.human_gate.minimum_rating_per_criterion) { $errors.Add("$id cannot be accepted: rating '$rating' is below the gate.") }
        }
        if ($blockers.Count -gt 0) { $errors.Add("$id cannot be accepted while blocking QA findings remain.") }
    } elseif ([string]::IsNullOrWhiteSpace([string]$decision.reason)) {
        $errors.Add("A reason is required when repainting or replacing $id.")
    }
    if ($errors | Where-Object { $_ -like "$id *" -or $_ -like "*$id*" }) { continue }

    $asset = $assetsById[$id]
    $newStatus = if ($verdict -eq "accept") { "accepted" } else { "needs_repaint" }
    $historyEntry = [pscustomobject]@{
        run_id = [string]$qa.run_id; manifest_sha256 = $manifestHash; verdict = $verdict
        reviewer = $reviewer; reviewed_at_utc = [DateTime]::UtcNow.ToString("o")
        ratings = [pscustomobject]@{ style_fit=[int]$decision.style_fit; perspective=[int]$decision.perspective; palette_lighting=[int]$decision.palette_lighting; scale_readability=[int]$decision.scale_readability; contextual_fit=[int]$decision.contextual_fit }
        reason = [string]$decision.reason
    }
    $asset.qa_history = @($asset.qa_history) + @($historyEntry)
    $asset.status = $newStatus
    if ($verdict -eq "accept" -and [bool]$decision.license_approved) { $asset.license_review_status = "approved" }
    $applied.Add([pscustomobject]@{ asset_id=$id; verdict=$verdict; status=$newStatus })

    if ($verdict -in @("repaint", "replace")) {
        $evidence = @($assetFindings | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_.evidence_path) } | Select-Object -ExpandProperty evidence_path -Unique)
        $codes = @($assetFindings | Select-Object -ExpandProperty code -Unique)
        $prompt = "Create a replacement for '$id' ($($asset.semantic_role)). Match fantasy_neutral_warm_painterly@1 exactly: muted natural warm painterly texture, soft fine edges, neutral overhead light, strict 90-degree orthographic top-down. Preserve $($asset.dimensions.width)x$($asset.dimensions.height) pixels, footprint $($asset.footprint.width)x$($asset.footprint.height), edge contract '$($asset.edge_contract)', and transparency=$($asset.transparent). Exclude cartoon/mobile-game styling, black outlines, glow, high saturation, isometric perspective, strong directional shadows, labels, people, signatures and watermarks. Correct these findings: $($codes -join ', '). Use the approved reference assets and neighboring family '$($asset.compatible_neighbor_families -join ', ')' as mandatory visual references."
        $workOrders.Add([pscustomobject]@{
            schema_version=1; id=("repaint_" + (Clean-Token $id) + "_" + $qa.run_id); asset_id=$id; family=$asset.family
            semantic_role=$asset.semantic_role; verdict=$verdict; defect_codes=$codes; evidence_paths=$evidence
            reference_asset_ids=@($profile.approved_reference_asset_ids); provisional_reference_asset_ids=@($profile.provisional_reference_asset_ids)
            required_width=[int]$asset.dimensions.width; required_height=[int]$asset.dimensions.height
            requires_transparency=[bool]$asset.transparent; footprint=$asset.footprint; anchors=$asset.anchors
            compatible_neighbor_families=$asset.compatible_neighbor_families; prompt=$prompt; output_status="candidate"; supersedes=$id
        })
    }
}

$validation = [ordered]@{
    schema_version=1; run_id=$qa.run_id; validated_at_utc=[DateTime]::UtcNow.ToString("o")
    manifest_sha256=$manifestHash; decision_count=@($decisions.decisions).Count; valid_decision_count=$applied.Count
    error_count=$errors.Count; errors=$errors; applied=$applied; replacement_work_order_count=$workOrders.Count
    required_map_review_count=[int]$qa.review_requirements.required_map_count; submitted_map_review_count=$mapReviewsById.Count
    map_reviews_complete=$mapReviewsComplete; map_review_error_count=$mapReviewErrors.Count; map_review_errors=$mapReviewErrors
    owner_override_applied=$ownerOverrideApplied; owner_override_reason=$OwnerOverrideReason
}
$validation | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $outputPath "review_validation.json") -Encoding utf8
$workOrders | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $outputPath "replacement_work_orders.json") -Encoding utf8
$receiptMaterial = @(
    "run_id=$($qa.run_id)",
    "manifest_sha256=$manifestHash",
    "qa_report_sha256=$(Get-Sha256 $reportFile)",
    "decision_file_sha256=$(Get-Sha256 $decisionsFile)",
    "reviewer=$($decisions.reviewer)",
    "owner_override_reason=$OwnerOverrideReason"
) -join "`n"
$receipt = [ordered]@{
    schema_version=1; algorithm="sha256"; run_id=$qa.run_id; reviewer=[string]$decisions.reviewer
    manifest_sha256=$manifestHash; qa_report_sha256=(Get-Sha256 $reportFile); decision_file_sha256=(Get-Sha256 $decisionsFile)
    owner_override_reason=$OwnerOverrideReason; integrity_signature=(Get-TextSha256 $receiptMaterial)
}
$receipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $outputPath "review_integrity_receipt.json") -Encoding utf8
if ($errors.Count -gt 0) { Write-Host "Review validation failed with $($errors.Count) error(s)."; exit 2 }

if ($Apply) {
    $manifest.accepted_count = @($manifest.assets | Where-Object status -eq "accepted").Count
    $manifest.candidate_count = @($manifest.assets | Where-Object status -eq "candidate").Count
    $manifest.needs_repaint_count = @($manifest.assets | Where-Object status -eq "needs_repaint").Count
    $manifest | ConvertTo-Json -Depth 14 | Set-Content -LiteralPath $manifestFile -Encoding utf8
    Write-Host "Applied $($applied.Count) reviewed decisions to $manifestFile"
} else {
    Write-Host "Validated $($applied.Count) decisions. Use -Apply to update the manifest."
}

if ($CreateProductionSnapshot) {
    if (-not $Apply) { throw "Production snapshot creation requires -Apply." }
    if (-not $mapReviewsComplete -and -not $ownerOverrideApplied) { throw "Production snapshot blocked: $($mapReviewErrors.Count) full-map review gate(s) remain. An explicit owner override must include its reason." }
    if ([string]$profile.approval_status -ne "approved") { throw "Art-direction references are not approved." }
    $notAccepted = @($manifest.assets | Where-Object status -ne "accepted")
    $licensePending = @($manifest.assets | Where-Object license_review_status -ne "approved")
    if ($notAccepted.Count -gt 0) { throw "Production snapshot blocked: $($notAccepted.Count) assets are not accepted." }
    if ($licensePending.Count -gt 0) { throw "Production snapshot blocked: $($licensePending.Count) license reviews are pending." }
    $newManifestHash = Get-Sha256 $manifestFile
    $assetEntries = @($manifest.assets | Sort-Object id | ForEach-Object {
        $filePath = Join-Path (Split-Path -Parent $manifestFile) ([string]$_.file).Replace('/', '\')
        [pscustomobject]@{ id=$_.id; file=$_.file; sha256=(Get-Sha256 $filePath) }
    })
    $snapshotMaterial = ($assetEntries | ConvertTo-Json -Compress)
    $bytes = [Text.Encoding]::UTF8.GetBytes($snapshotMaterial)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $snapshotHash = ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant() } finally { $sha.Dispose() }
    $snapshot = [ordered]@{
        schema_version=1; snapshot_id=("fantasy_neutral_v3_" + $snapshotHash.Substring(0,16)); state="production"
        style_id=$manifest.style_id; style_version=$manifest.style_version; art_direction_id=$manifest.art_direction_id
        source_manifest_sha256=$newManifestHash; created_at_utc=[DateTime]::UtcNow.ToString("o"); asset_count=$assetEntries.Count
        human_review=[ordered]@{ reviewer=[string]$decisions.reviewer; required_maps=[int]$qa.review_requirements.required_map_count; reviewed_maps=$mapReviewsById.Count; complete=$mapReviewsComplete; owner_override_applied=$ownerOverrideApplied; owner_override_reason=$OwnerOverrideReason }
        assets=$assetEntries
    }
    $snapshotJson = $snapshot | ConvertTo-Json -Depth 8
    $reviewSnapshotPath = Join-Path $outputPath "production_style_snapshot.json"
    $runtimeSnapshotPath = Join-Path (Split-Path -Parent $manifestFile) "production_style_snapshot.json"
    $snapshotJson | Set-Content -LiteralPath $reviewSnapshotPath -Encoding utf8
    $runtimeTemporary = "$runtimeSnapshotPath.tmp"
    $snapshotJson | Set-Content -LiteralPath $runtimeTemporary -Encoding utf8
    if (Test-Path -LiteralPath $runtimeSnapshotPath -PathType Leaf) { Remove-Item -LiteralPath $runtimeSnapshotPath -Force }
    Move-Item -LiteralPath $runtimeTemporary -Destination $runtimeSnapshotPath
    Write-Host "Created atomic production snapshot $($snapshot.snapshot_id)"
}
