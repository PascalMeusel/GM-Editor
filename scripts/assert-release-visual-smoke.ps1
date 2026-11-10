#requires -Version 5.1

[CmdletBinding(PositionalBinding = $false)]
param(
    [Parameter(Mandatory = $true)][string]$RunRoot,
    [ValidateSet("All", "P0", "P1", "P2")][string]$Priority = "All",
    [switch]$RequireComplete,
    [switch]$RequirePass
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.Drawing

function Add-Problem {
    param([System.Collections.Generic.List[string]]$Problems, [string]$Message)
    $Problems.Add($Message)
}

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

$run = [System.IO.Path]::GetFullPath($RunRoot)
$manifestPath = Join-Path $run "visual-smoke-run.json"
$queuePath = Join-Path $run "capture-queue.csv"
$screenshots = Join-Path $run "screenshots"

if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "Visual-smoke manifest not found: $manifestPath"
}
if (-not (Test-Path -LiteralPath $queuePath -PathType Leaf)) {
    throw "Capture queue not found: $queuePath"
}

$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$queue = @(Import-Csv -LiteralPath $queuePath)
$problems = New-Object 'System.Collections.Generic.List[string]'

if ($queue.Count -ne [int]$manifest.expectedCaptureCount) {
    Add-Problem $problems "Queue has $($queue.Count) rows; manifest expects $($manifest.expectedCaptureCount)."
}
$scenarioCount = @($queue.scenarioId | Sort-Object -Unique).Count
if ($scenarioCount -ne [int]$manifest.expectedScenarioCount) {
    Add-Problem $problems "Queue has $scenarioCount scenarios; manifest expects $($manifest.expectedScenarioCount)."
}
foreach ($duplicate in @($queue | Group-Object captureId | Where-Object Count -ne 1)) {
    Add-Problem $problems "Capture ID '$($duplicate.Name)' occurs $($duplicate.Count) times."
}

$editorPath = [string]$manifest.editorExecutable
if (-not (Test-Path -LiteralPath $editorPath -PathType Leaf)) {
    Add-Problem $problems "Editor executable is missing: $editorPath"
}
else {
    $actualEditorHash = (Get-FileHash -LiteralPath $editorPath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actualEditorHash -ne ([string]$manifest.editorSha256).ToLowerInvariant()) {
        Add-Problem $problems "Editor hash changed after fixture preparation; create a new run manifest."
    }
}

$targets = if ($Priority -eq "All") { $queue } else { @($queue | Where-Object priority -eq $Priority) }
$states = New-Object 'System.Collections.Generic.List[object]'

foreach ($row in $targets) {
    $safeLocale = ([string]$row.locale).Replace("-", "_")
    $baseName = "$($row.scenarioId)`__$($row.viewportKey)`__$safeLocale"
    $imagePath = Join-Path $screenshots "$baseName.png"
    $metadataPath = Join-Path $screenshots "$baseName.json"
    $reviewPath = Join-Path $screenshots "$baseName.review.md"
    $existing = @($imagePath, $metadataPath, $reviewPath | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })
    if ($existing.Count -eq 0) {
        $states.Add([PSCustomObject]@{ CaptureId = $row.captureId; State = "missing"; Verdict = "" })
        continue
    }
    if ($existing.Count -ne 3) {
        Add-Problem $problems "Incomplete evidence trio for '$($row.captureId)'."
        $states.Add([PSCustomObject]@{ CaptureId = $row.captureId; State = "invalid"; Verdict = "" })
        continue
    }

    try {
        $image = [System.Drawing.Image]::FromFile($imagePath)
        try {
            if ($image.Width -ne [int]$row.width -or $image.Height -ne [int]$row.height) {
                Add-Problem $problems "Image '$($row.captureId)' is $($image.Width)x$($image.Height); expected $($row.width)x$($row.height)."
            }
        }
        finally {
            $image.Dispose()
        }

        $metadata = Get-Content -LiteralPath $metadataPath -Raw | ConvertFrom-Json
        if ([string]$metadata.scenarioId -ne [string]$row.scenarioId -or
            [string]$metadata.viewportKey -ne [string]$row.viewportKey -or
            [string]$metadata.locale -ne [string]$row.locale) {
            Add-Problem $problems "Metadata identity does not match queue row '$($row.captureId)'."
        }
        if ([int]$metadata.clientWidth -ne [int]$row.width -or [int]$metadata.clientHeight -ne [int]$row.height) {
            Add-Problem $problems "Metadata dimensions do not match '$($row.captureId)'."
        }
        $manifestViewport = @($manifest.viewports | Where-Object { [string]$_.key -eq [string]$row.viewportKey })
        if ($manifestViewport.Count -ne 1) {
            Add-Problem $problems "Viewport '$($row.viewportKey)' does not resolve exactly once in the run manifest for '$($row.captureId)'."
        }
        else {
            $queueScale = [System.Convert]::ToDouble([string]$row.uiScale, [System.Globalization.CultureInfo]::InvariantCulture)
            $expectedScale = [double]$manifestViewport[0].uiScale
            if ([Math]::Abs($queueScale - $expectedScale) -gt 0.0001) {
                Add-Problem $problems "Queue UI scale $queueScale does not match manifest scale $expectedScale for '$($row.captureId)'."
            }
            $actualScale = [double]$metadata.actualDpiScale
            if ($actualScale -le 0.0 -or [double]::IsNaN($actualScale) -or [double]::IsInfinity($actualScale) -or
                [Math]::Abs($actualScale - $expectedScale) -gt $dpiScaleTolerance) {
                Add-Problem $problems "Capture '$($row.captureId)' used DPI scale $actualScale; expected $expectedScale (+/- $dpiScaleTolerance)."
            }
        }
        if (([string]$metadata.editorSha256).ToLowerInvariant() -ne ([string]$manifest.editorSha256).ToLowerInvariant()) {
            Add-Problem $problems "Capture '$($row.captureId)' came from a different editor binary."
        }
        $imageHash = (Get-FileHash -LiteralPath $imagePath -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($imageHash -ne ([string]$metadata.imageSha256).ToLowerInvariant()) {
            Add-Problem $problems "Image hash does not match metadata for '$($row.captureId)'."
        }
        if (-not [bool]$metadata.stateConfirmedByReviewer -or -not [bool]$metadata.foregroundConfirmed) {
            Add-Problem $problems "Capture '$($row.captureId)' lacks state or foreground confirmation."
        }
        if ([string]$row.playerSafe -eq "True" -and -not [bool]$metadata.playerSafetyConfirmed) {
            Add-Problem $problems "Player-safe capture '$($row.captureId)' lacks safety confirmation."
        }

        $review = Get-Content -LiteralPath $reviewPath -Raw
        $match = [regex]::Match($review, '(?im)^Verdict:\s*([a-z_-]+)\s*$')
        $verdict = if ($match.Success) { $match.Groups[1].Value.ToLowerInvariant() } else { "missing" }
        if ($verdict -notin @("pass", "fail", "pending")) {
            Add-Problem $problems "Review verdict is invalid for '$($row.captureId)'."
        }
        if ($RequirePass -and $verdict -eq "pass") {
            $metadataReviewerProperty = $metadata.PSObject.Properties["reviewer"]
            $metadataReviewer = if ($null -ne $metadataReviewerProperty) { ([string]$metadataReviewerProperty.Value).Trim() } else { "" }
            if ([string]::IsNullOrWhiteSpace($metadataReviewer)) {
                Add-Problem $problems "Pass review for '$($row.captureId)' does not name a reviewer in capture metadata."
            }

            $reviewChecks = @([regex]::Matches(
                $review,
                '(?im)^[ \t]*[-*][ \t]+\[([ xX])\][ \t]+([^\r\n]+?)[ \t]*\r?$'))
            foreach ($requiredCheck in $requiredReviewChecks) {
                $matchingChecks = @($reviewChecks | Where-Object {
                    $_.Groups[2].Value.Trim().StartsWith($requiredCheck, [System.StringComparison]::OrdinalIgnoreCase)
                })
                if ($matchingChecks.Count -ne 1) {
                    Add-Problem $problems "Pass review for '$($row.captureId)' must contain exactly one '$requiredCheck' checkbox."
                }
                elseif ($matchingChecks[0].Groups[1].Value -notmatch '^[xX]$') {
                    Add-Problem $problems "Pass review for '$($row.captureId)' has not checked '$requiredCheck'."
                }
            }

            # New review files carry this human-readable field. Older evidence
            # keeps the reviewer only in JSON, so absence remains compatible.
            $reviewerMatches = @([regex]::Matches(
                $review,
                '(?im)^[ \t]*(?:Reviewer|Reviewed by)[ \t]*:[ \t]*([^\r\n]*?)[ \t]*\r?$'))
            if ($reviewerMatches.Count -gt 1) {
                Add-Problem $problems "Pass review for '$($row.captureId)' contains multiple reviewer fields."
            }
            elseif ($reviewerMatches.Count -eq 1) {
                $reviewReviewer = $reviewerMatches[0].Groups[1].Value.Trim()
                if ([string]::IsNullOrWhiteSpace($reviewReviewer)) {
                    Add-Problem $problems "Pass review for '$($row.captureId)' has an empty reviewer field."
                }
                elseif (-not [string]::IsNullOrWhiteSpace($metadataReviewer) -and
                    -not $reviewReviewer.Equals($metadataReviewer, [System.StringComparison]::OrdinalIgnoreCase)) {
                    Add-Problem $problems "Reviewer field does not match capture metadata for '$($row.captureId)'."
                }
            }
        }
        $states.Add([PSCustomObject]@{ CaptureId = $row.captureId; State = "captured"; Verdict = $verdict })
    }
    catch {
        Add-Problem $problems "Could not validate '$($row.captureId)': $($_.Exception.Message)"
        $states.Add([PSCustomObject]@{ CaptureId = $row.captureId; State = "invalid"; Verdict = "" })
    }
}

$missingCount = @($states | Where-Object State -eq "missing").Count
$invalidCount = @($states | Where-Object State -eq "invalid").Count
$passCount = @($states | Where-Object Verdict -eq "pass").Count
$failCount = @($states | Where-Object Verdict -eq "fail").Count
$pendingCount = @($states | Where-Object Verdict -eq "pending").Count

Write-Host "Visual smoke integrity: $($targets.Count) target(s), $passCount pass, $failCount fail, $pendingCount pending, $missingCount missing, $invalidCount invalid."

if ($RequireComplete -and $missingCount -gt 0) {
    Add-Problem $problems "$missingCount required capture(s) are missing for priority $Priority."
}
if ($RequirePass -and ($passCount -ne $targets.Count)) {
    Add-Problem $problems "Only $passCount of $($targets.Count) required capture(s) have Verdict: pass for priority $Priority."
}

if ($problems.Count -gt 0) {
    foreach ($problem in $problems) { Write-Error $problem }
    throw "Visual-smoke validation failed with $($problems.Count) problem(s)."
}

Write-Host "Visual-smoke evidence is internally consistent."
