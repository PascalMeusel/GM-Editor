param(
    [string]$SourceRoot = "Resources/Maps/Tilesets/Source/FantasyNeutral/v2",
    [string]$OutputDirectory = "",
    [string[]]$Families = @("grass_open", "snow", "water_shallow", "dirt"),
    [int]$TileSize = 40,
    [int]$Width = 16,
    [int]$Height = 16
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")

$resolvedSourceRoot = Resolve-TilesetPath $SourceRoot
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $resolvedSourceRoot "review/same_family_repeat_tests"
}
$resolvedOutputDirectory = Resolve-TilesetPath $OutputDirectory
New-Item -ItemType Directory -Force -Path $resolvedOutputDirectory | Out-Null

$bundle = Get-TilesetV2ManifestBundle $resolvedSourceRoot
$assets = @((Get-TilesetManifestAssets $bundle.Tiles) + (Get-TilesetManifestAssets $bundle.Transitions))
$results = @()

foreach ($family in $Families) {
    $familyAssets = @($assets | Where-Object {
        (Get-TilesetProperty $_ "owner" "") -eq $family -and
        (Get-TilesetProperty $_ "asset_class" "") -in @("base", "same_auto") -and
        (Get-TilesetProperty $_ "status" "") -in @("candidate", "accepted")
    })
    if ($familyAssets.Count -eq 0) {
        $results += [pscustomobject]@{
            family = $family
            status = "skipped"
            reason = "no_candidate_or_accepted_assets"
        }
        continue
    }

    $review = New-TilesetReviewBitmap ($Width * $TileSize) ($Height * $TileSize)
    try {
        for ($y = 0; $y -lt $Height; ++$y) {
            for ($x = 0; $x -lt $Width; ++$x) {
                $mask = 0
                if ($y -gt 0) { $mask = $mask -bor 1 }
                if ($x -lt ($Width - 1)) { $mask = $mask -bor 2 }
                if ($y -lt ($Height - 1)) { $mask = $mask -bor 4 }
                if ($x -gt 0) { $mask = $mask -bor 8 }
                $asset = Select-TilesetAsset $familyAssets $family @("same_auto") $mask $x $y 17
                if ($null -eq $asset) {
                    $asset = Select-TilesetAsset $familyAssets $family @("base") $null $x $y 19
                }
                $dest = [Drawing.Rectangle]::new($x * $TileSize, $y * $TileSize, $TileSize, $TileSize)
                Draw-TilesetAsset $review.Graphics $asset $dest
            }
        }

        $outputPath = Join-Path $resolvedOutputDirectory "$family`__same_family_repeat.png"
        $review.Bitmap.Save($outputPath, [Drawing.Imaging.ImageFormat]::Png)
        $results += [pscustomobject]@{
            family = $family
            status = "generated"
            output_path = ConvertTo-TilesetRelativePath $outputPath
            asset_count = $familyAssets.Count
        }
    }
    finally {
        $review.Graphics.Dispose()
        $review.Bitmap.Dispose()
    }
}

$reportPath = Join-Path $resolvedOutputDirectory "same_family_repeat_tests.json"
$report = [pscustomobject]@{
    generated_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    tile_size = $TileSize
    width = $Width
    height = $Height
    results = $results
}
$report | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $reportPath
Write-Host "Same-family repeat tests wrote $reportPath"
