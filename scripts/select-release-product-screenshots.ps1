#requires -Version 5.1

[CmdletBinding(PositionalBinding = $false)]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$RunRoot,

    [Parameter(Mandatory = $true)]
    [Alias("CaptureIds", "ScreenshotId", "ScreenshotIds")]
    [ValidateNotNullOrEmpty()]
    [string[]]$CaptureId,

    [ValidateNotNullOrEmpty()]
    [string]$OutputDirectory = "steam-screenshots/selected"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.Drawing

$dpiScaleTolerance = 0.08
$requiredReviewChecks = @(
    "Correct workspace and expected state are visible",
    "No important text overlaps or clips",
    "Primary action and current selection are clear",
    "Disabled actions explain why",
    "German/English copy is consistent for the selected locale",
    "Keyboard focus remains visible where applicable",
    "Player-facing output contains no GM-only information"
)
$expectedCaptureMethod = "Windows client-area CopyFromScreen after explicit manual state confirmation"
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)

function Get-NormalizedFullPath {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$BasePath
    )

    $candidate = $Path
    if (-not [System.IO.Path]::IsPathRooted($candidate)) {
        $candidate = Join-Path $BasePath $candidate
    }
    return [System.IO.Path]::GetFullPath($candidate).TrimEnd([char[]]@('\', '/'))
}

function Test-IsStrictChildPath {
    param(
        [Parameter(Mandatory = $true)][string]$Child,
        [Parameter(Mandatory = $true)][string]$Parent
    )

    $childPath = [System.IO.Path]::GetFullPath($Child).TrimEnd([char[]]@('\', '/'))
    $parentPath = [System.IO.Path]::GetFullPath($Parent).TrimEnd([char[]]@('\', '/'))
    return $childPath.StartsWith(
        $parentPath + [System.IO.Path]::DirectorySeparatorChar,
        [System.StringComparison]::OrdinalIgnoreCase)
}

function Assert-LeafFile {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Label
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Label not found: $Path"
    }
}

function Get-FileSha256 {
    param([Parameter(Mandatory = $true)][string]$Path)
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function ConvertTo-ValidatedSha256 {
    param(
        [AllowNull()][object]$Value,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $hash = ([string]$Value).Trim().ToLowerInvariant()
    if ($hash -notmatch '^[0-9a-f]{64}$') {
        throw "$Label is not a valid SHA-256 value: '$Value'."
    }
    return $hash
}

function Get-RequiredPropertyValue {
    param(
        [Parameter(Mandatory = $true)][object]$Object,
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $matches = @($Object.PSObject.Properties | Where-Object {
        $_.Name.Equals($Name, [System.StringComparison]::OrdinalIgnoreCase)
    })
    if ($matches.Count -ne 1) {
        throw "$Label must contain exactly one '$Name' property."
    }
    return $matches[0].Value
}

function Get-OptionalNamedValues {
    param(
        [Parameter(Mandatory = $true)][object]$Object,
        [Parameter(Mandatory = $true)][string[]]$Names
    )

    $values = New-Object System.Collections.Generic.List[object]
    foreach ($name in $Names) {
        $matches = @($Object.PSObject.Properties | Where-Object {
            $_.Name.Equals($name, [System.StringComparison]::OrdinalIgnoreCase)
        })
        if ($matches.Count -gt 1) {
            throw "Object contains more than one '$name' property."
        }
        if ($matches.Count -eq 1) {
            $values.Add([PSCustomObject]@{ Name = $matches[0].Name; Value = $matches[0].Value })
        }
    }
    return $values.ToArray()
}

function Read-JsonObject {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Label
    )

    try {
        $value = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    }
    catch {
        throw "$Label is not valid JSON: $($_.Exception.Message)"
    }
    if ($null -eq $value -or $value -is [System.Array]) {
        throw "$Label must contain one JSON object."
    }
    return $value
}

function ConvertTo-PositiveInt32 {
    param(
        [AllowNull()][object]$Value,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $parsed = 0
    if (-not [int]::TryParse(
        ([string]$Value),
        [System.Globalization.NumberStyles]::Integer,
        [System.Globalization.CultureInfo]::InvariantCulture,
        [ref]$parsed) -or $parsed -le 0) {
        throw "$Label must be a positive integer: '$Value'."
    }
    return $parsed
}

function ConvertTo-FiniteDouble {
    param(
        [AllowNull()][object]$Value,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $parsed = 0.0
    $text = [string]$Value
    $parsedSuccessfully = [double]::TryParse(
        $text,
        [System.Globalization.NumberStyles]::Float,
        [System.Globalization.CultureInfo]::InvariantCulture,
        [ref]$parsed)
    if (-not $parsedSuccessfully) {
        $parsedSuccessfully = [double]::TryParse(
            $text,
            [System.Globalization.NumberStyles]::Float,
            [System.Globalization.CultureInfo]::CurrentCulture,
            [ref]$parsed)
    }
    if (-not $parsedSuccessfully -or
        [double]::IsNaN($parsed) -or
        [double]::IsInfinity($parsed) -or
        $parsed -le 0.0) {
        throw "$Label must be a positive finite number: '$Value'."
    }
    return $parsed
}

function ConvertTo-StrictBoolean {
    param(
        [AllowNull()][object]$Value,
        [Parameter(Mandatory = $true)][string]$Label
    )

    if ($Value -is [bool]) {
        return [bool]$Value
    }
    $text = ([string]$Value).Trim()
    if ($text.Equals("true", [System.StringComparison]::OrdinalIgnoreCase)) {
        return $true
    }
    if ($text.Equals("false", [System.StringComparison]::OrdinalIgnoreCase)) {
        return $false
    }
    throw "$Label must be true or false: '$Value'."
}

function Assert-SafeCaptureId {
    param(
        [Parameter(Mandatory = $true)][string]$Value,
        [Parameter(Mandatory = $true)][string]$Label
    )

    if ([string]::IsNullOrWhiteSpace($Value) -or $Value -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]*$') {
        throw "$Label is empty or contains unsafe filename characters: '$Value'."
    }
}

function Assert-NoReparsePoint {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $item = Get-Item -LiteralPath $Path -Force
    if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
        throw "$Label must not be a reparse point: $Path"
    }
}

function Get-RelativePathCompat {
    param(
        [Parameter(Mandatory = $true)][string]$BaseDirectory,
        [Parameter(Mandatory = $true)][string]$TargetPath
    )

    $baseFull = [System.IO.Path]::GetFullPath($BaseDirectory).TrimEnd([char[]]@('\', '/')) + [System.IO.Path]::DirectorySeparatorChar
    $targetFull = [System.IO.Path]::GetFullPath($TargetPath)
    $baseUri = New-Object System.Uri($baseFull)
    $targetUri = New-Object System.Uri($targetFull)
    if (-not $baseUri.IsBaseOf($targetUri)) {
        throw "Path is not below the expected base directory: $targetFull"
    }
    return [System.Uri]::UnescapeDataString($baseUri.MakeRelativeUri($targetUri).ToString()).Replace('/', '\')
}

function Get-Sha256SumsBinding {
    param(
        [Parameter(Mandatory = $true)][object]$Manifest,
        [Parameter(Mandatory = $true)][string]$RunDirectory,
        [Parameter(Mandatory = $true)][string]$EditorPath,
        [Parameter(Mandatory = $true)][string]$EditorSha256
    )

    $hashProperties = @(Get-OptionalNamedValues -Object $Manifest -Names @(
        "sha256SumsSha256",
        "depotSha256SumsSha256"
    ))
    $pathProperties = @(Get-OptionalNamedValues -Object $Manifest -Names @(
        "sha256SumsPath",
        "depotSha256SumsPath"
    ))

    if ($hashProperties.Count -eq 0) {
        if ($pathProperties.Count -gt 0) {
            throw "Run manifest names SHA256SUMS.txt but does not bind it with a SHA-256 value."
        }
        return $null
    }

    $hashes = @($hashProperties | ForEach-Object {
        ConvertTo-ValidatedSha256 -Value $_.Value -Label "Run-manifest $($_.Name)"
    } | Sort-Object -Unique)
    if ($hashes.Count -ne 1) {
        throw "Run manifest contains conflicting SHA256SUMS.txt hashes."
    }
    $expectedHash = $hashes[0]

    $paths = New-Object System.Collections.Generic.List[string]
    foreach ($property in $pathProperties) {
        $pathText = ([string]$property.Value).Trim()
        if ([string]::IsNullOrWhiteSpace($pathText)) {
            throw "Run-manifest $($property.Name) is empty."
        }
        $paths.Add((Get-NormalizedFullPath -Path $pathText -BasePath $RunDirectory))
    }
    if ($paths.Count -eq 0) {
        $paths.Add((Join-Path (Split-Path -Parent $EditorPath) "SHA256SUMS.txt"))
    }
    $distinctPaths = @($paths | Sort-Object -Unique)
    if ($distinctPaths.Count -ne 1) {
        throw "Run manifest contains conflicting SHA256SUMS.txt paths."
    }

    $checksumPath = $distinctPaths[0]
    Assert-LeafFile -Path $checksumPath -Label "Hash-bound SHA256SUMS.txt"
    if ((Get-FileSha256 -Path $checksumPath) -ne $expectedHash) {
        throw "SHA256SUMS.txt hash no longer matches the run manifest."
    }

    $checksumRoot = Split-Path -Parent $checksumPath
    $editorRelativePath = (Get-RelativePathCompat -BaseDirectory $checksumRoot -TargetPath $EditorPath).Replace('\', '/')
    $entryHashes = New-Object System.Collections.Generic.List[string]
    $entryPaths = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($line in [System.IO.File]::ReadAllLines($checksumPath)) {
        if ([string]::IsNullOrWhiteSpace($line)) {
            continue
        }
        $match = [regex]::Match($line, '^([0-9A-Fa-f]{64})[ \t]+\*?(.+?)[ \t]*$')
        if (-not $match.Success) {
            throw "SHA256SUMS.txt contains an invalid line."
        }
        $entryPath = $match.Groups[2].Value.Replace('\', '/').Trim()
        if ([System.IO.Path]::IsPathRooted($entryPath) -or $entryPath -match '(^|/)\.\.(/|$)') {
            throw "SHA256SUMS.txt contains an unsafe path: $entryPath"
        }
        if (-not $entryPaths.Add($entryPath)) {
            throw "SHA256SUMS.txt contains a duplicate path: $entryPath"
        }
        if ($entryPath.Equals($editorRelativePath, [System.StringComparison]::OrdinalIgnoreCase)) {
            $entryHashes.Add($match.Groups[1].Value.ToLowerInvariant())
        }
    }
    if ($entryHashes.Count -ne 1 -or $entryHashes[0] -ne $EditorSha256) {
        throw "SHA256SUMS.txt does not bind the selected editor executable to the run-manifest hash."
    }

    return [PSCustomObject]@{
        Sha256 = $expectedHash
        Path = $checksumPath
    }
}

$run = Get-NormalizedFullPath -Path $RunRoot -BasePath (Get-Location).Path
if (-not (Test-Path -LiteralPath $run -PathType Container)) {
    throw "Visual-smoke run directory not found: $run"
}
Assert-NoReparsePoint -Path $run -Label "Visual-smoke run directory"

$manifestPath = Join-Path $run "visual-smoke-run.json"
$queuePath = Join-Path $run "capture-queue.csv"
$screenshotsDirectory = Join-Path $run "screenshots"
Assert-LeafFile -Path $manifestPath -Label "Visual-smoke run manifest"
Assert-LeafFile -Path $queuePath -Label "Visual-smoke capture queue"
if (-not (Test-Path -LiteralPath $screenshotsDirectory -PathType Container)) {
    throw "Visual-smoke screenshot directory not found: $screenshotsDirectory"
}
Assert-NoReparsePoint -Path $screenshotsDirectory -Label "Visual-smoke screenshot directory"

$outputPath = Get-NormalizedFullPath -Path $OutputDirectory -BasePath $run
if (-not (Test-IsStrictChildPath -Child $outputPath -Parent $run)) {
    throw "OutputDirectory must be a strict child of the visual-smoke run: $outputPath"
}
if ($outputPath.Equals($screenshotsDirectory, [System.StringComparison]::OrdinalIgnoreCase) -or
    (Test-IsStrictChildPath -Child $outputPath -Parent $screenshotsDirectory)) {
    throw "OutputDirectory must be separate from the source screenshots directory: $outputPath"
}
if (Test-Path -LiteralPath $outputPath) {
    throw "OutputDirectory already exists; refusing to merge or overwrite a prior selection: $outputPath"
}

$manifest = Read-JsonObject -Path $manifestPath -Label "Visual-smoke run manifest"
$runId = ([string](Get-RequiredPropertyValue -Object $manifest -Name "runId" -Label "Visual-smoke run manifest")).Trim()
if ([string]::IsNullOrWhiteSpace($runId)) {
    throw "Visual-smoke run manifest has an empty runId."
}
$editorSha256 = ConvertTo-ValidatedSha256 `
    -Value (Get-RequiredPropertyValue -Object $manifest -Name "editorSha256" -Label "Visual-smoke run manifest") `
    -Label "Run-manifest editorSha256"
$editorPathText = ([string](Get-RequiredPropertyValue -Object $manifest -Name "editorExecutable" -Label "Visual-smoke run manifest")).Trim()
if ([string]::IsNullOrWhiteSpace($editorPathText)) {
    throw "Visual-smoke run manifest has an empty editorExecutable."
}
$editorPath = Get-NormalizedFullPath -Path $editorPathText -BasePath $run
Assert-LeafFile -Path $editorPath -Label "Run-manifest editor executable"
if ((Get-FileSha256 -Path $editorPath) -ne $editorSha256) {
    throw "Editor executable hash no longer matches the visual-smoke run manifest."
}

$manifestQueueText = ([string](Get-RequiredPropertyValue -Object $manifest -Name "queue" -Label "Visual-smoke run manifest")).Trim()
if ([string]::IsNullOrWhiteSpace($manifestQueueText)) {
    throw "Visual-smoke run manifest has an empty queue path."
}
$manifestQueuePath = Get-NormalizedFullPath -Path $manifestQueueText -BasePath $run
if (-not $manifestQueuePath.Equals($queuePath, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Run-manifest queue path does not identify this run's capture-queue.csv."
}

$sha256SumsBinding = Get-Sha256SumsBinding `
    -Manifest $manifest `
    -RunDirectory $run `
    -EditorPath $editorPath `
    -EditorSha256 $editorSha256

$queue = @(Import-Csv -LiteralPath $queuePath)
$expectedCaptureCount = ConvertTo-PositiveInt32 `
    -Value (Get-RequiredPropertyValue -Object $manifest -Name "expectedCaptureCount" -Label "Visual-smoke run manifest") `
    -Label "Run-manifest expectedCaptureCount"
if ($queue.Count -ne $expectedCaptureCount) {
    throw "Capture queue has $($queue.Count) rows; run manifest expects $expectedCaptureCount."
}

$queueIdSet = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
$fileIdSet = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
$scenarioSet = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::Ordinal)
$selectorMap = New-Object 'System.Collections.Generic.Dictionary[string,object]' ([System.StringComparer]::OrdinalIgnoreCase)

for ($queueIndex = 0; $queueIndex -lt $queue.Count; ++$queueIndex) {
    $row = $queue[$queueIndex]
    foreach ($requiredColumn in @("captureId", "scenarioId", "viewportKey", "locale", "width", "height", "uiScale", "playerSafe")) {
        $null = Get-RequiredPropertyValue -Object $row -Name $requiredColumn -Label "Capture queue row $($queueIndex + 1)"
    }

    $queueCaptureId = ([string]$row.captureId).Trim()
    $scenarioId = ([string]$row.scenarioId).Trim()
    $viewportKey = ([string]$row.viewportKey).Trim()
    $locale = ([string]$row.locale).Trim()
    Assert-SafeCaptureId -Value $queueCaptureId -Label "Capture queue ID"
    Assert-SafeCaptureId -Value $scenarioId -Label "Scenario ID for '$queueCaptureId'"
    Assert-SafeCaptureId -Value $viewportKey -Label "Viewport key for '$queueCaptureId'"
    if ($locale -notmatch '^[A-Za-z]{2,3}-[A-Za-z]{2,4}$') {
        throw "Locale for '$queueCaptureId' is invalid: '$locale'."
    }
    if (-not $queueIdSet.Add($queueCaptureId)) {
        throw "Capture queue contains duplicate capture ID '$queueCaptureId'."
    }
    $null = $scenarioSet.Add($scenarioId)

    $expectedQueueCaptureId = "$scenarioId`__$viewportKey`__$locale"
    $fileCaptureId = "$scenarioId`__$viewportKey`__$($locale.Replace('-', '_'))"
    if (-not $queueCaptureId.Equals($expectedQueueCaptureId, [System.StringComparison]::Ordinal) -and
        -not $queueCaptureId.Equals($fileCaptureId, [System.StringComparison]::Ordinal)) {
        throw "Capture queue ID '$queueCaptureId' does not match its scenario, viewport, and locale."
    }
    Assert-SafeCaptureId -Value $fileCaptureId -Label "Derived capture ID"
    if (-not $fileIdSet.Add($fileCaptureId)) {
        throw "Capture queue resolves more than once to screenshot ID '$fileCaptureId'."
    }

    $entry = [PSCustomObject]@{
        Row = $row
        QueueIndex = $queueIndex
        QueueCaptureId = $queueCaptureId
        FileCaptureId = $fileCaptureId
    }
    foreach ($selector in @($queueCaptureId, $fileCaptureId) | Select-Object -Unique) {
        if ($selectorMap.ContainsKey($selector)) {
            throw "Capture selector '$selector' resolves to more than one queue row."
        }
        $selectorMap.Add($selector, $entry)
    }
}

$expectedScenarioCount = ConvertTo-PositiveInt32 `
    -Value (Get-RequiredPropertyValue -Object $manifest -Name "expectedScenarioCount" -Label "Visual-smoke run manifest") `
    -Label "Run-manifest expectedScenarioCount"
if ($scenarioSet.Count -ne $expectedScenarioCount) {
    throw "Capture queue has $($scenarioSet.Count) scenario IDs; run manifest expects $expectedScenarioCount."
}

$requestedIds = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
$selectedQueueIndexes = New-Object 'System.Collections.Generic.HashSet[int]'
$selectedRows = New-Object System.Collections.Generic.List[object]
foreach ($requested in $CaptureId) {
    $id = ([string]$requested).Trim()
    Assert-SafeCaptureId -Value $id -Label "Requested capture ID"
    if (-not $requestedIds.Add($id)) {
        throw "Requested capture ID '$id' was specified more than once."
    }
    if (-not $selectorMap.ContainsKey($id)) {
        throw "Requested capture ID '$id' does not exist in this visual-smoke run."
    }
    $selected = $selectorMap[$id]
    if (-not $selectedQueueIndexes.Add([int]$selected.QueueIndex)) {
        throw "Requested capture IDs resolve more than once to '$($selected.FileCaptureId)'."
    }
    $selectedRows.Add($selected)
}
if ($selectedRows.Count -eq 0) {
    throw "At least one explicit capture ID is required."
}

$validatedSelections = New-Object System.Collections.Generic.List[object]
foreach ($selected in @($selectedRows | Sort-Object FileCaptureId)) {
    $row = $selected.Row
    $baseName = [string]$selected.FileCaptureId
    $imagePath = Join-Path $screenshotsDirectory "$baseName.png"
    $metadataPath = Join-Path $screenshotsDirectory "$baseName.json"
    $reviewPath = Join-Path $screenshotsDirectory "$baseName.review.md"
    Assert-LeafFile -Path $imagePath -Label "Screenshot '$baseName'"
    Assert-LeafFile -Path $metadataPath -Label "Screenshot metadata '$baseName'"
    Assert-LeafFile -Path $reviewPath -Label "Screenshot review '$baseName'"
    Assert-NoReparsePoint -Path $imagePath -Label "Screenshot '$baseName'"
    Assert-NoReparsePoint -Path $metadataPath -Label "Screenshot metadata '$baseName'"
    Assert-NoReparsePoint -Path $reviewPath -Label "Screenshot review '$baseName'"

    $expectedWidth = ConvertTo-PositiveInt32 -Value $row.width -Label "Queue width for '$baseName'"
    $expectedHeight = ConvertTo-PositiveInt32 -Value $row.height -Label "Queue height for '$baseName'"
    $queueScale = ConvertTo-FiniteDouble -Value $row.uiScale -Label "Queue UI scale for '$baseName'"

    $matchingViewports = @($manifest.viewports | Where-Object {
        ([string]$_.key).Equals(([string]$row.viewportKey), [System.StringComparison]::Ordinal)
    })
    if ($matchingViewports.Count -ne 1) {
        throw "Viewport '$($row.viewportKey)' must resolve exactly once in the run manifest for '$baseName'."
    }
    $manifestScale = ConvertTo-FiniteDouble -Value $matchingViewports[0].uiScale -Label "Manifest UI scale for '$baseName'"
    $manifestWidth = ConvertTo-PositiveInt32 -Value $matchingViewports[0].width -Label "Manifest width for '$baseName'"
    $manifestHeight = ConvertTo-PositiveInt32 -Value $matchingViewports[0].height -Label "Manifest height for '$baseName'"
    if ([Math]::Abs($queueScale - $manifestScale) -gt 0.0001 -or
        $expectedWidth -ne $manifestWidth -or
        $expectedHeight -ne $manifestHeight) {
        throw "Queue dimensions or DPI scale do not match the run-manifest viewport for '$baseName'."
    }

    $imageWidth = 0
    $imageHeight = 0
    $image = [System.Drawing.Image]::FromFile($imagePath)
    try {
        if ($image.RawFormat.Guid -ne [System.Drawing.Imaging.ImageFormat]::Png.Guid) {
            throw "Screenshot '$baseName' is not a PNG image."
        }
        $imageWidth = $image.Width
        $imageHeight = $image.Height
    }
    finally {
        $image.Dispose()
    }
    if ($imageWidth -ne $expectedWidth -or $imageHeight -ne $expectedHeight) {
        throw "Screenshot '$baseName' is ${imageWidth}x${imageHeight}; expected ${expectedWidth}x${expectedHeight}."
    }

    $imageSha256 = Get-FileSha256 -Path $imagePath
    $metadataSha256 = Get-FileSha256 -Path $metadataPath
    $reviewSha256 = Get-FileSha256 -Path $reviewPath
    $metadata = Read-JsonObject -Path $metadataPath -Label "Screenshot metadata '$baseName'"

    foreach ($identity in @(
        @{ Name = "captureId"; Expected = $baseName },
        @{ Name = "scenarioId"; Expected = [string]$row.scenarioId },
        @{ Name = "viewportKey"; Expected = [string]$row.viewportKey },
        @{ Name = "locale"; Expected = [string]$row.locale }
    )) {
        $actual = [string](Get-RequiredPropertyValue -Object $metadata -Name $identity.Name -Label "Screenshot metadata '$baseName'")
        if (-not $actual.Equals([string]$identity.Expected, [System.StringComparison]::Ordinal)) {
            throw "Screenshot metadata '$($identity.Name)' does not match the queue for '$baseName'."
        }
    }

    $metadataWidth = ConvertTo-PositiveInt32 `
        -Value (Get-RequiredPropertyValue -Object $metadata -Name "clientWidth" -Label "Screenshot metadata '$baseName'") `
        -Label "Metadata width for '$baseName'"
    $metadataHeight = ConvertTo-PositiveInt32 `
        -Value (Get-RequiredPropertyValue -Object $metadata -Name "clientHeight" -Label "Screenshot metadata '$baseName'") `
        -Label "Metadata height for '$baseName'"
    if ($metadataWidth -ne $expectedWidth -or $metadataHeight -ne $expectedHeight) {
        throw "Screenshot metadata dimensions do not match the queue for '$baseName'."
    }

    $actualDpiScale = ConvertTo-FiniteDouble `
        -Value (Get-RequiredPropertyValue -Object $metadata -Name "actualDpiScale" -Label "Screenshot metadata '$baseName'") `
        -Label "Actual DPI scale for '$baseName'"
    if ([Math]::Abs($actualDpiScale - $manifestScale) -gt $dpiScaleTolerance) {
        throw "Screenshot '$baseName' used DPI scale $actualDpiScale; expected $manifestScale (+/- $dpiScaleTolerance)."
    }

    $metadataEditorHash = ConvertTo-ValidatedSha256 `
        -Value (Get-RequiredPropertyValue -Object $metadata -Name "editorSha256" -Label "Screenshot metadata '$baseName'") `
        -Label "Metadata editorSha256 for '$baseName'"
    if ($metadataEditorHash -ne $editorSha256) {
        throw "Screenshot '$baseName' came from a different editor executable."
    }
    $metadataImageHash = ConvertTo-ValidatedSha256 `
        -Value (Get-RequiredPropertyValue -Object $metadata -Name "imageSha256" -Label "Screenshot metadata '$baseName'") `
        -Label "Metadata imageSha256 for '$baseName'"
    if ($metadataImageHash -ne $imageSha256) {
        throw "Screenshot hash does not match metadata for '$baseName'."
    }

    foreach ($confirmationName in @("stateConfirmedByReviewer", "foregroundConfirmed")) {
        $confirmed = ConvertTo-StrictBoolean `
            -Value (Get-RequiredPropertyValue -Object $metadata -Name $confirmationName -Label "Screenshot metadata '$baseName'") `
            -Label "Metadata $confirmationName for '$baseName'"
        if (-not $confirmed) {
            throw "Screenshot '$baseName' lacks required $confirmationName confirmation."
        }
    }
    $captureMethod = [string](Get-RequiredPropertyValue -Object $metadata -Name "captureMethod" -Label "Screenshot metadata '$baseName'")
    if (-not $captureMethod.Equals($expectedCaptureMethod, [System.StringComparison]::Ordinal)) {
        throw "Screenshot '$baseName' was not produced by the release visual-smoke capture method."
    }
    $playerSafe = ConvertTo-StrictBoolean -Value $row.playerSafe -Label "Queue playerSafe for '$baseName'"
    if ($playerSafe) {
        $playerSafetyConfirmed = ConvertTo-StrictBoolean `
            -Value (Get-RequiredPropertyValue -Object $metadata -Name "playerSafetyConfirmed" -Label "Screenshot metadata '$baseName'") `
            -Label "Metadata playerSafetyConfirmed for '$baseName'"
        if (-not $playerSafetyConfirmed) {
            throw "Player-facing screenshot '$baseName' lacks player-safety confirmation."
        }
    }

    $metadataReviewer = ([string](Get-RequiredPropertyValue -Object $metadata -Name "reviewer" -Label "Screenshot metadata '$baseName'")).Trim()
    if ([string]::IsNullOrWhiteSpace($metadataReviewer)) {
        throw "Screenshot metadata has no reviewer for '$baseName'."
    }

    $review = Get-Content -LiteralPath $reviewPath -Raw
    $headingMatches = @([regex]::Matches(
        $review,
        "(?im)^#[ \t]+Visual review:[ \t]*$([regex]::Escape($baseName))[ \t]*\r?$"))
    if ($headingMatches.Count -ne 1) {
        throw "Review heading does not uniquely identify '$baseName'."
    }
    $verdictMatches = @([regex]::Matches(
        $review,
        '(?im)^[ \t]*Verdict[ \t]*:[ \t]*([a-z_-]+)[ \t]*\r?$'))
    if ($verdictMatches.Count -ne 1) {
        throw "Review for '$baseName' must contain exactly one Verdict field."
    }
    $verdict = $verdictMatches[0].Groups[1].Value.ToLowerInvariant()
    if ($verdict -ne "pass") {
        throw "Review for '$baseName' is '$verdict'; only an explicit Verdict: pass can be selected."
    }

    $reviewChecks = @([regex]::Matches(
        $review,
        '(?im)^[ \t]*[-*][ \t]+\[([ xX])\][ \t]+([^\r\n]+?)[ \t]*\r?$'))
    foreach ($requiredCheck in $requiredReviewChecks) {
        $matchingChecks = @($reviewChecks | Where-Object {
            $text = $_.Groups[2].Value.Trim()
            $text.Equals($requiredCheck, [System.StringComparison]::OrdinalIgnoreCase) -or
                $text.StartsWith($requiredCheck + " (", [System.StringComparison]::OrdinalIgnoreCase)
        })
        if ($matchingChecks.Count -ne 1) {
            throw "Pass review for '$baseName' must contain exactly one '$requiredCheck' checkbox."
        }
        if ($matchingChecks[0].Groups[1].Value -notmatch '^[xX]$') {
            throw "Pass review for '$baseName' has not checked '$requiredCheck'."
        }
    }

    $reviewerMatches = @([regex]::Matches(
        $review,
        '(?im)^[ \t]*(?:Reviewer|Reviewed by)[ \t]*:[ \t]*([^\r\n]*?)[ \t]*\r?$'))
    if ($reviewerMatches.Count -ne 1) {
        throw "Pass review for '$baseName' must contain exactly one Reviewer field."
    }
    $reviewReviewer = $reviewerMatches[0].Groups[1].Value.Trim()
    if ([string]::IsNullOrWhiteSpace($reviewReviewer)) {
        throw "Pass review for '$baseName' has an empty Reviewer field."
    }
    if (-not $reviewReviewer.Equals($metadataReviewer, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Review Reviewer does not match capture metadata for '$baseName'."
    }

    $validatedSelections.Add([PSCustomObject]@{
        SourceImagePath = $imagePath
        Entry = [ordered]@{
            captureId = $baseName
            file = "$baseName.png"
            sha256 = $imageSha256
            width = $imageWidth
            height = $imageHeight
            locale = [string]$row.locale
            scenarioId = [string]$row.scenarioId
            viewportKey = [string]$row.viewportKey
            actualDpiScale = $actualDpiScale
            metadataSha256 = $metadataSha256
            reviewSha256 = $reviewSha256
            reviewer = $reviewReviewer
        }
    })
}

$finalManifest = [ordered]@{
    schemaVersion = 1
    assetClass = "product_screenshot"
    pixelTransform = "none"
    generatedAtUtc = [DateTime]::UtcNow.ToString("o")
    runId = $runId
    runManifestSha256 = Get-FileSha256 -Path $manifestPath
    captureQueueSha256 = Get-FileSha256 -Path $queuePath
    editorSha256 = $editorSha256
    screenshotCount = $validatedSelections.Count
    screenshots = @($validatedSelections | ForEach-Object { $_.Entry })
}
if ($null -ne $sha256SumsBinding) {
    $finalManifest.sha256SumsSha256 = $sha256SumsBinding.Sha256
}

$stagingPath = Join-Path $run (".screenshot-selection-staging-" + [System.Guid]::NewGuid().ToString("N"))
if (-not (Test-IsStrictChildPath -Child $stagingPath -Parent $run)) {
    throw "Refusing unsafe staging path: $stagingPath"
}

try {
    [System.IO.Directory]::CreateDirectory($stagingPath) | Out-Null
    foreach ($selection in $validatedSelections) {
        $destinationImage = Join-Path $stagingPath ([string]$selection.Entry.file)
        [System.IO.File]::Copy($selection.SourceImagePath, $destinationImage, $false)
        if ((Get-FileSha256 -Path $destinationImage) -ne [string]$selection.Entry.sha256) {
            throw "Copied screenshot hash changed for '$($selection.Entry.captureId)'."
        }
    }

    $finalManifestPath = Join-Path $stagingPath "FINAL_SCREENSHOT_MANIFEST.json"
    $finalManifestJson = ($finalManifest | ConvertTo-Json -Depth 8) + "`n"
    [System.IO.File]::WriteAllText($finalManifestPath, $finalManifestJson, $utf8NoBom)
    $finalManifestSha256 = Get-FileSha256 -Path $finalManifestPath
    $sidecarPath = $finalManifestPath + ".sha256"
    [System.IO.File]::WriteAllText(
        $sidecarPath,
        "$finalManifestSha256  FINAL_SCREENSHOT_MANIFEST.json`n",
        $utf8NoBom)

    if (Test-Path -LiteralPath $outputPath) {
        throw "OutputDirectory appeared during validation; refusing to overwrite it: $outputPath"
    }
    $outputParent = Split-Path -Parent $outputPath
    [System.IO.Directory]::CreateDirectory($outputParent) | Out-Null
    Assert-NoReparsePoint -Path $outputParent -Label "Output parent directory"
    [System.IO.Directory]::Move($stagingPath, $outputPath)
}
catch {
    if (Test-Path -LiteralPath $stagingPath -PathType Container) {
        $verifiedStagingPath = [System.IO.Path]::GetFullPath($stagingPath)
        if ((Test-IsStrictChildPath -Child $verifiedStagingPath -Parent $run) -and
            (Split-Path -Leaf $verifiedStagingPath).StartsWith(".screenshot-selection-staging-", [System.StringComparison]::Ordinal)) {
            [System.IO.Directory]::Delete($verifiedStagingPath, $true)
        }
    }
    throw
}

Write-Host "Selected $($validatedSelections.Count) reviewed product screenshot(s)."
Write-Host "Output: $outputPath"
Write-Host "Manifest: $(Join-Path $outputPath 'FINAL_SCREENSHOT_MANIFEST.json')"
Write-Host "Manifest SHA-256: $finalManifestSha256"
