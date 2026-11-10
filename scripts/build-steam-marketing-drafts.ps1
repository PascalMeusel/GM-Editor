#requires -Version 5.1

[CmdletBinding(PositionalBinding = $false)]
param(
    [string]$LandscapeSource = "marketing/steam/source/gm-editor-key-art-landscape-draft.png",
    [string]$PortraitSource = "marketing/steam/source/gm-editor-key-art-portrait-draft.png",
    [string]$WordmarkSource = "marketing/brand/gm-editor-wordmark-1600.png",
    [string]$MarkSource = "marketing/brand/gm-editor-mark-1024.png",
    [string]$OutputDirectory = "marketing/steam/draft"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.Drawing

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))

function Resolve-ProjectPath {
    param([Parameter(Mandatory = $true)][string]$Path)
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $projectRoot $Path))
}

function New-Canvas {
    param([int]$Width, [int]$Height, [bool]$Transparent = $false)
    $bitmap = New-Object System.Drawing.Bitmap($Width, $Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
    $graphics.Clear($(if ($Transparent) { [System.Drawing.Color]::Transparent } else { [System.Drawing.Color]::FromArgb(255, 22, 25, 22) }))
    $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceOver
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    return @($bitmap, $graphics)
}

function Draw-CoverImage {
    param(
        [Parameter(Mandatory = $true)][System.Drawing.Graphics]$Graphics,
        [Parameter(Mandatory = $true)][System.Drawing.Image]$Image,
        [int]$Width,
        [int]$Height,
        [double]$FocusX = 0.5,
        [double]$FocusY = 0.5
    )
    $scale = [Math]::Max($Width / [double]$Image.Width, $Height / [double]$Image.Height)
    $drawWidth = $Image.Width * $scale
    $drawHeight = $Image.Height * $scale
    $overflowX = [Math]::Max(0.0, $drawWidth - $Width)
    $overflowY = [Math]::Max(0.0, $drawHeight - $Height)
    $x = -$overflowX * [Math]::Min(1.0, [Math]::Max(0.0, $FocusX))
    $y = -$overflowY * [Math]::Min(1.0, [Math]::Max(0.0, $FocusY))
    $Graphics.DrawImage($Image, [single]$x, [single]$y, [single]$drawWidth, [single]$drawHeight)
}

function Draw-Logo {
    param(
        [Parameter(Mandatory = $true)][System.Drawing.Graphics]$Graphics,
        [Parameter(Mandatory = $true)][System.Drawing.Image]$Logo,
        [int]$CanvasWidth,
        [int]$CanvasHeight,
        [double]$WidthFraction,
        [double]$XFraction,
        [double]$YFraction
    )
    $width = $CanvasWidth * $WidthFraction
    $height = $width * $Logo.Height / [double]$Logo.Width
    if ($height -gt $CanvasHeight * 0.36) {
        $height = $CanvasHeight * 0.36
        $width = $height * $Logo.Width / [double]$Logo.Height
    }
    $Graphics.DrawImage($Logo,
                        [single]($CanvasWidth * $XFraction),
                        [single]($CanvasHeight * $YFraction),
                        [single]$width,
                        [single]$height)
}

function Save-Png {
    param([System.Drawing.Bitmap]$Bitmap, [string]$Path)
    $Bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
}

function Export-Capsule {
    param(
        [string]$Name,
        [System.Drawing.Image]$Source,
        [int]$Width,
        [int]$Height,
        [bool]$WithLogo,
        [double]$FocusX = 0.5,
        [double]$FocusY = 0.5,
        [double]$LogoWidth = 0.48,
        [double]$LogoX = 0.05,
        [double]$LogoY = 0.08
    )
    $canvas = New-Canvas -Width $Width -Height $Height
    $bitmap = $canvas[0]
    $graphics = $canvas[1]
    try {
        Draw-CoverImage -Graphics $graphics -Image $Source -Width $Width -Height $Height -FocusX $FocusX -FocusY $FocusY
        if ($WithLogo) {
            Draw-Logo -Graphics $graphics -Logo $script:wordmark -CanvasWidth $Width -CanvasHeight $Height `
                -WidthFraction $LogoWidth -XFraction $LogoX -YFraction $LogoY
        }
        Save-Png -Bitmap $bitmap -Path (Join-Path $script:outputRoot $Name)
    }
    finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

$landscapePath = Resolve-ProjectPath $LandscapeSource
$portraitPath = Resolve-ProjectPath $PortraitSource
$wordmarkPath = Resolve-ProjectPath $WordmarkSource
$markPath = Resolve-ProjectPath $MarkSource
$outputRoot = Resolve-ProjectPath $OutputDirectory

foreach ($path in @($landscapePath, $portraitPath, $wordmarkPath, $markPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required marketing source is missing: $path"
    }
}
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null

$landscape = [System.Drawing.Image]::FromFile($landscapePath)
$portrait = [System.Drawing.Image]::FromFile($portraitPath)
$wordmark = [System.Drawing.Image]::FromFile($wordmarkPath)
$mark = [System.Drawing.Image]::FromFile($markPath)
try {
    Export-Capsule "store-header-920x430.png" $landscape 920 430 $true 0.50 0.55 0.49 0.045 0.07
    Export-Capsule "store-small-462x174.png" $landscape 462 174 $true 0.43 0.51 0.60 0.035 0.11
    Export-Capsule "store-main-1232x706.png" $landscape 1232 706 $true 0.50 0.50 0.46 0.045 0.07
    Export-Capsule "store-vertical-748x896.png" $portrait 748 896 $true 0.50 0.46 0.82 0.07 0.055
    Export-Capsule "library-capsule-600x900.png" $portrait 600 900 $true 0.56 0.49 0.84 0.06 0.055
    Export-Capsule "library-header-920x430.png" $landscape 920 430 $true 0.50 0.54 0.49 0.045 0.07
    Export-Capsule "library-hero-clean-3840x1240.png" $landscape 3840 1240 $false 0.50 0.64
    Export-Capsule "press-key-art-3840x2160.png" $landscape 3840 2160 $false 0.50 0.50
    Export-Capsule "social-share-1920x1080.png" $landscape 1920 1080 $true 0.50 0.50 0.44 0.045 0.065
    Export-Capsule "store-page-background-1438x810.png" $landscape 1438 810 $false 0.50 0.50
    Export-Capsule "event-cover-800x450.png" $landscape 800 450 $true 0.50 0.50 0.44 0.045 0.065
    Export-Capsule "event-header-1920x622.png" $landscape 1920 622 $true 0.50 0.58 0.34 0.04 0.10

    $logoCanvas = New-Canvas -Width 1280 -Height 336 -Transparent $true
    try {
        $logoCanvas[1].DrawImage($wordmark, 0, 0, 1280, 336)
        Save-Png -Bitmap $logoCanvas[0] -Path (Join-Path $outputRoot "library-logo-1280x336.png")
    }
    finally {
        $logoCanvas[1].Dispose()
        $logoCanvas[0].Dispose()
    }

    $shortcutCanvas = New-Canvas -Width 512 -Height 512 -Transparent $true
    try {
        $shortcutCanvas[1].DrawImage($mark, 0, 0, 512, 512)
        Save-Png -Bitmap $shortcutCanvas[0] -Path (Join-Path $outputRoot "shortcut-icon-512.png")
    }
    finally {
        $shortcutCanvas[1].Dispose()
        $shortcutCanvas[0].Dispose()
    }

    $appCanvas = New-Canvas -Width 184 -Height 184
    try {
        $appCanvas[1].DrawImage($mark, 0, 0, 184, 184)
        $appCanvas[0].Save((Join-Path $outputRoot "app-icon-184.jpg"), [System.Drawing.Imaging.ImageFormat]::Jpeg)
    }
    finally {
        $appCanvas[1].Dispose()
        $appCanvas[0].Dispose()
    }
}
finally {
    $landscape.Dispose()
    $portrait.Dispose()
    $wordmark.Dispose()
    $mark.Dispose()
}

$outputs = @(Get-ChildItem -LiteralPath $outputRoot -File | Sort-Object Name)
Write-Host "Steam marketing drafts generated: $($outputs.Count)"
Write-Host "Output: $outputRoot"
foreach ($file in $outputs) {
    Write-Host "  $($file.Name) ($($file.Length) bytes)"
}
