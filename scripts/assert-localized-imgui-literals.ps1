[CmdletBinding(PositionalBinding = $false)]
param([string]$RepositoryRoot = '')

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($RepositoryRoot)) { $RepositoryRoot = Split-Path -Parent $PSScriptRoot }
$projectRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$sourceRoots = @(
    (Get-ChildItem -LiteralPath $projectRoot -File -Filter "Application*.cpp" | ForEach-Object { $_.FullName }),
    (Join-Path $projectRoot "Views"),
    (Join-Path $projectRoot "Controllers")
)

$visibleCalls = @(
    "Text",
    "TextWrapped",
    "TextDisabled",
    "TextUnformatted",
    "TextColored",
    "Button",
    "SmallButton",
    "MenuItem",
    "Selectable",
    "Checkbox",
    "RadioButton",
    "BeginTabItem",
    "CollapsingHeader"
) -join "|"

# Stable ImGui IDs (BeginChild, PushID, hidden ### suffixes) are deliberately
# excluded: only calls whose first literal is rendered to users are guarded.
$literalPattern = "ImGui::(?:$visibleCalls)\s*\(\s*`"[A-Za-z]"
$messagePattern = "(?:_statusMessage|_errorMessage|_feedbackMessage|_sceneLaunchMessage|_audioStatusMessage|_audioErrorMessage|_clockStatusMessage)\s*=\s*`"[A-Za-z]"
$findings = New-Object System.Collections.Generic.List[string]

foreach ($sourceRoot in $sourceRoots) {
    $files = if (Test-Path -LiteralPath $sourceRoot -PathType Container) {
        Get-ChildItem -LiteralPath $sourceRoot -Recurse -File -Include *.cpp,*.hpp,*.h
    } else {
        Get-Item -LiteralPath $sourceRoot
    }

    foreach ($file in $files) {
        $lineNumber = 0
        foreach ($line in Get-Content -LiteralPath $file.FullName -Encoding UTF8) {
            $lineNumber++
            if ($line -match $literalPattern) {
                $relative = $file.FullName.Substring($projectRoot.Length).TrimStart('\', '/')
                $findings.Add("${relative}:${lineNumber}: $($line.Trim())")
            }
            elseif ($line -match $messagePattern) {
                $relative = $file.FullName.Substring($projectRoot.Length).TrimStart('\', '/')
                $findings.Add("${relative}:${lineNumber}: $($line.Trim())")
            }
        }
    }
}

# Passing a literal through localizedUiLabel()/localizedUiText()/trUi() is only
# safe when the catalog actually contains that visible key. trUi deliberately
# falls back to English for dynamic campaign content, so a missing catalog
# entry would otherwise pass the source gate and leak into the German UI.
$catalogFiles = Get-ChildItem -LiteralPath (Join-Path $projectRoot "Models/Localization") -File |
    Where-Object { $_.Name -like "TranslationCatalog*.cpp" -or $_.Name -like "TranslationCatalog*.hpp" }
$catalogSource = ($catalogFiles | ForEach-Object {
    Get-Content -LiteralPath $_.FullName -Raw -Encoding UTF8
}) -join [Environment]::NewLine
$localizedCallPattern = '(?:localizedUiLabel|localizedUiText|(?:gm::localization::)?trUi)\s*\(\s*"([^"]+)"\s*\)'
$internalLocalizedIds = @(
    "MainMenuContainer",
    "NewCompendiumEntry",
    "RandomRulesLeft",
    "RandomRulesRight",
    "TableCoreLeft",
    "TableCoreRight"
)
$localizedRoots = @(
    (Get-ChildItem -LiteralPath $projectRoot -File -Filter "Application*.cpp" | ForEach-Object { $_.FullName }),
    (Join-Path $projectRoot "Views"),
    (Join-Path $projectRoot "Controllers"),
    (Join-Path $projectRoot "Models")
)

foreach ($localizedRoot in $localizedRoots) {
    $files = if (Test-Path -LiteralPath $localizedRoot -PathType Container) {
        Get-ChildItem -LiteralPath $localizedRoot -Recurse -File -Include *.cpp,*.hpp,*.h
    } else {
        Get-Item -LiteralPath $localizedRoot
    }

    foreach ($file in $files) {
        $source = Get-Content -LiteralPath $file.FullName -Raw -Encoding UTF8
        if ($null -eq $source) { continue }
        foreach ($match in [regex]::Matches($source, $localizedCallPattern)) {
            $visibleKey = ([string]$match.Groups[1].Value -split '##', 2)[0]
            if ([string]::IsNullOrWhiteSpace($visibleKey) -or
                $visibleKey -notmatch '[A-Za-z]' -or
                $visibleKey.StartsWith('%') -or
                $visibleKey.StartsWith('[%') -or
                $visibleKey.Contains(',') -or
                $internalLocalizedIds -contains $visibleKey) {
                continue
            }

            $catalogNeedle = 'UiLiteralEntry{"' + $visibleKey + '"'
            if ($catalogSource.IndexOf($catalogNeedle, [StringComparison]::Ordinal) -lt 0) {
                $relative = $file.FullName.Substring($projectRoot.Length).TrimStart('\', '/')
                $findings.Add("${relative}: localized key missing from catalog: $visibleKey")
            }
        }
    }
}

if ($findings.Count -gt 0) {
    $details = $findings -join [Environment]::NewLine
    throw "Visible ImGui labels and runtime messages must pass through localization:`n$details"
}

Write-Host "Localization source gate passed. No direct English ImGui labels or runtime status/error assignments were found."
