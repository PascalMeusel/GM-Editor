param(
    [string]$ManifestPath = "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3/pack_manifest.json"
)
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$manifestFile = if ([IO.Path]::IsPathRooted($ManifestPath)) { [IO.Path]::GetFullPath($ManifestPath) } else { [IO.Path]::GetFullPath((Join-Path $repositoryRoot $ManifestPath)) }
$manifest = Get-Content -Raw -LiteralPath $manifestFile | ConvertFrom-Json
$assetRoot = Split-Path -Parent $manifestFile
$changed = [Collections.Generic.List[object]]::new()
foreach ($asset in @($manifest.assets | Where-Object { -not [bool]$_.transparent })) {
    $path = Join-Path $assetRoot ([string]$asset.file).Replace('/', '\')
    $loaded = [Drawing.Bitmap]::new($path)
    $bitmap = [Drawing.Bitmap]::new($loaded.Width, $loaded.Height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CompositingMode = [Drawing.Drawing2D.CompositingMode]::SourceCopy
        $graphics.DrawImageUnscaled($loaded, 0, 0)
    } finally { $graphics.Dispose(); $loaded.Dispose() }
    $nonOpaque = 0
    $rectangle = [Drawing.Rectangle]::new(0, 0, $bitmap.Width, $bitmap.Height)
    $data = $bitmap.LockBits($rectangle, [Drawing.Imaging.ImageLockMode]::ReadWrite, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $byteCount = [Math]::Abs($data.Stride) * $bitmap.Height
        $pixels = [byte[]]::new($byteCount)
        [Runtime.InteropServices.Marshal]::Copy($data.Scan0, $pixels, 0, $byteCount)
        for ($y = 0; $y -lt $bitmap.Height; ++$y) {
            $rowOffset = $y * [Math]::Abs($data.Stride)
            for ($x = 0; $x -lt $bitmap.Width; ++$x) {
                $alphaIndex = $rowOffset + $x * 4 + 3
                if ($pixels[$alphaIndex] -ne 255) { ++$nonOpaque; $pixels[$alphaIndex] = 255 }
            }
        }
        [Runtime.InteropServices.Marshal]::Copy($pixels, 0, $data.Scan0, $byteCount)
    } finally { $bitmap.UnlockBits($data) }
    if ($nonOpaque -gt 0) {
        $temporary = "$path.opaque-repair.png"
        if (Test-Path -LiteralPath $temporary -PathType Leaf) { Remove-Item -LiteralPath $temporary -Force }
        $bitmap.Save($temporary, [Drawing.Imaging.ImageFormat]::Png)
        $bitmap.Dispose()
        [IO.File]::Delete($path)
        [IO.File]::Move($temporary, $path)
        $changed.Add([pscustomobject]@{ asset_id=$asset.id; normalized_pixels=$nonOpaque; file=$asset.file })
    } else { $bitmap.Dispose() }
}
$changed | ConvertTo-Json -Depth 4
Write-Host "Normalized opaque alpha for $($changed.Count) FantasyNeutral V3 asset(s)."
