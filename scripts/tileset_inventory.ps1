param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")

$resolvedSourceRoot = Resolve-TilesetPath $SourceRoot
if (-not (Test-Path -LiteralPath $resolvedSourceRoot)) {
    throw "Missing v2 source root: $resolvedSourceRoot"
}
if (-not $OutputPath) {
    $OutputPath = Join-Path $resolvedSourceRoot "review/tileset_inventory_report.json"
}
$resolvedOutputPath = Resolve-TilesetPath $OutputPath
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $resolvedOutputPath) | Out-Null

$bundle = Get-TilesetV2ManifestBundle $resolvedSourceRoot
$records = @(Get-TilesetV2AssetRecords $bundle)
$manifestPathLookup = @{}
foreach ($record in $records) {
    if (-not [string]::IsNullOrWhiteSpace($record.SourcePath)) {
        $manifestPathLookup[(ConvertTo-TilesetRelativePath $record.SourcePath)] = $true
    }
}

$pngFiles = @(Get-ChildItem -LiteralPath $resolvedSourceRoot -Recurse -Filter "*.png" | Where-Object {
    $_.FullName -notmatch "\\review\\"
})
$fileReports = @()
foreach ($file in $pngFiles) {
    $parse = ConvertFrom-TilesetAssetFileName $file.FullName
    $info = Get-TilesetImageInfo $file.FullName
    $relativePath = ConvertTo-TilesetRelativePath $file.FullName
    $fileReports += [pscustomobject]@{
        path = $relativePath
        id_from_filename = $parse.Id
        parse_ok = $parse.IsValid
        parse_errors = $parse.Errors
        style = $parse.Style
        scale = $parse.Scale
        asset_class = $parse.AssetClass
        owner = $parse.Owner
        neighbor = $parse.Neighbor
        mask = $parse.Mask
        variant = $parse.Variant
        width = $info.Width
        height = $info.Height
        listed_in_manifest = $manifestPathLookup.ContainsKey($relativePath)
    }
}

$missingFiles = @()
foreach ($record in $records) {
    if ([string]::IsNullOrWhiteSpace($record.SourcePath)) {
        continue
    }
    $fullPath = Resolve-TilesetPath $record.SourcePath
    if (-not (Test-Path -LiteralPath $fullPath)) {
        $missingFiles += [pscustomobject]@{
            id = $record.Id
            manifest = $record.Manifest
            source_path = ConvertTo-TilesetRelativePath $record.SourcePath
        }
    }
}

$duplicateIds = @()
$recordsWithIds = @($records | Where-Object { -not [string]::IsNullOrWhiteSpace($_.Id) })
$recordsWithIds | Group-Object Id | Where-Object { $_.Count -gt 1 } | ForEach-Object {
    $duplicateIds += [pscustomobject]@{
        id = $_.Name
        count = $_.Count
        manifests = @($_.Group | ForEach-Object { $_.Manifest })
    }
}

$report = [pscustomobject]@{
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    source_root = ConvertTo-TilesetRelativePath $resolvedSourceRoot
    summary = [pscustomobject]@{
        manifest_asset_count = $records.Count
        png_file_count = $pngFiles.Count
        unlisted_png_count = @($fileReports | Where-Object { -not $_.listed_in_manifest }).Count
        missing_manifest_file_count = $missingFiles.Count
        duplicate_id_count = $duplicateIds.Count
    }
    files = $fileReports
    missing_manifest_files = $missingFiles
    duplicate_ids = $duplicateIds
}

$report | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $resolvedOutputPath
Write-Host "Tileset inventory wrote $resolvedOutputPath"
Write-Host "PNG files: $($pngFiles.Count); manifest assets: $($records.Count); missing files: $($missingFiles.Count); duplicate IDs: $($duplicateIds.Count)"
