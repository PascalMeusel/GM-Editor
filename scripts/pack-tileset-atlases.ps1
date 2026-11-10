param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$OutputRoot = "Resources/Maps/Tilesets/Generated/FantasyNeutral/v2",
    [switch]$IncludeCandidate
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")

$resolvedSourceRoot = Resolve-TilesetPath $SourceRoot
$resolvedOutputRoot = Resolve-TilesetPath $OutputRoot
$atlasDirectory = Join-Path $resolvedOutputRoot "atlases"
New-Item -ItemType Directory -Force -Path $atlasDirectory | Out-Null

$bundle = Get-TilesetV2ManifestBundle $resolvedSourceRoot
$allAssets = @((Get-TilesetManifestAssets $bundle.Tiles) + (Get-TilesetManifestAssets $bundle.Transitions) + (Get-TilesetManifestAssets $bundle.Ways) + (Get-TilesetManifestAssets $bundle.Taxonomy))
$allowedStatuses = if ($IncludeCandidate) { @("candidate", "accepted") } else { @("accepted") }
$packableAssets = @($allAssets | Where-Object {
    (Get-TilesetProperty $_ "status" "") -in $allowedStatuses -and
    -not [string]::IsNullOrWhiteSpace((Get-TilesetProperty $_ "source_path" "")) -and
    (Test-Path -LiteralPath (Get-TilesetAssetSourcePath $_))
} | Sort-Object { Get-TilesetProperty $_ "scale" "" }, { Get-TilesetProperty $_ "asset_class" "" }, { Get-TilesetProperty $_ "id" "" })

$pages = @()
$packedAssets = @()
$groups = @($packableAssets | Group-Object {
    "$((Get-TilesetProperty $_ "scale" "world"))__$((Get-TilesetProperty $_ "asset_class" "asset"))"
})

foreach ($group in $groups) {
    $groupAssets = @($group.Group)
    $pageCount = [Math]::Ceiling($groupAssets.Count / 64.0)
    for ($pageIndex = 0; $pageIndex -lt $pageCount; ++$pageIndex) {
        $pageNumber = $pageIndex + 1
        $atlasName = "fantasy_neutral__$($group.Name)__page$($pageNumber.ToString("000")).png"
        $atlasPath = Join-Path $atlasDirectory $atlasName
        $bitmap = [Drawing.Bitmap]::new(1024, 1024, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $graphics = [Drawing.Graphics]::FromImage($bitmap)
        $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.Clear([Drawing.Color]::Transparent)
        $entries = @()
        try {
            for ($cell = 0; $cell -lt 64; ++$cell) {
                $assetIndex = ($pageIndex * 64) + $cell
                if ($assetIndex -ge $groupAssets.Count) {
                    break
                }
                $asset = $groupAssets[$assetIndex]
                $column = $cell % 8
                $row = [int][Math]::Floor($cell / 8)
                $dest = [Drawing.Rectangle]::new($column * 128, $row * 128, 128, 128)
                Draw-TilesetAsset $graphics $asset $dest
                $entry = [pscustomobject]@{
                    id = Get-TilesetProperty $asset "id" ""
                    work_order_id = Get-TilesetProperty $asset "work_order_id" ""
                    asset_class = Get-TilesetProperty $asset "asset_class" ""
                    owner = Get-TilesetProperty $asset "owner" ""
                    source_path = ConvertTo-TilesetRelativePath (Get-TilesetAssetSourcePath $asset)
                    atlas_path = ConvertTo-TilesetRelativePath $atlasPath
                    column = $column
                    row = $row
                    x = $column * 128
                    y = $row * 128
                    width = 128
                    height = 128
                    status = Get-TilesetProperty $asset "status" ""
                }
                $entries += $entry
                $packedAssets += $entry
            }
        }
        finally {
            $graphics.Dispose()
        }
        $bitmap.Save($atlasPath, [Drawing.Imaging.ImageFormat]::Png)
        $bitmap.Dispose()
        $pages += [pscustomobject]@{
            id = [System.IO.Path]::GetFileNameWithoutExtension($atlasName)
            path = ConvertTo-TilesetRelativePath $atlasPath
            width = 1024
            height = 1024
            cell_width = 128
            cell_height = 128
            entries = $entries
        }
    }
}

$atlasIndex = [pscustomobject]@{
    schema_version = 1
    style = "fantasy_neutral"
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    source_root = ConvertTo-TilesetRelativePath $resolvedSourceRoot
    pages = $pages
}
$runtimeAssets = @($packedAssets | Where-Object { (Get-TilesetProperty $_ "status" "") -eq "accepted" })
$candidateAssets = @($packedAssets | Where-Object { (Get-TilesetProperty $_ "status" "") -eq "candidate" })
$runtimeManifest = [pscustomobject]@{
    schema_version = 1
    style = "fantasy_neutral"
    generated_at_utc = $atlasIndex.generated_at_utc
    manifest_role = "accepted_runtime"
    asset_count = $runtimeAssets.Count
    assets = $runtimeAssets
}
$candidateManifest = [pscustomobject]@{
    schema_version = 1
    style = "fantasy_neutral"
    generated_at_utc = $atlasIndex.generated_at_utc
    manifest_role = "candidate_preview"
    asset_count = $candidateAssets.Count
    assets = $candidateAssets
}

$atlasIndexPath = Join-Path $resolvedOutputRoot "atlas_index.json"
$runtimeManifestPath = Join-Path $resolvedOutputRoot "packed_runtime_manifest.json"
$candidateManifestPath = Join-Path $resolvedOutputRoot "packed_candidate_manifest.json"
function Write-TilesetJsonFile {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)]$Value
    )

    $json = $Value | ConvertTo-Json -Depth 12
    if ($json -is [array]) {
        $json = $json -join "`n"
    }

    $json = $json -replace "`r`n", "`n"
    $json = $json -replace "`r", "`n"
    $json = (($json -split "`n") | ForEach-Object { $_.TrimEnd() }) -join "`n"
    [System.IO.File]::WriteAllText($Path, $json.TrimEnd() + "`n", [System.Text.UTF8Encoding]::new($false))
}

Write-TilesetJsonFile -Path $atlasIndexPath -Value $atlasIndex
Write-TilesetJsonFile -Path $runtimeManifestPath -Value $runtimeManifest
Write-TilesetJsonFile -Path $candidateManifestPath -Value $candidateManifest
Write-Host "Packed $($packedAssets.Count) asset(s) into $($pages.Count) atlas page(s)."
Write-Host "Atlas index: $atlasIndexPath"
Write-Host "Runtime manifest: $runtimeManifestPath"
Write-Host "Candidate manifest: $candidateManifestPath"
