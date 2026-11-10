#requires -Version 5.1
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$gate = Join-Path $PSScriptRoot 'assert-localized-imgui-literals.ps1'
$root = Join-Path ([IO.Path]::GetTempPath()) ('gm-localization-gate-' + [Guid]::NewGuid().ToString('N'))
$utf8 = New-Object System.Text.UTF8Encoding($false)
$dot = [string][char]0x00B7
$umlaut = [string][char]0x00FC
$key = "Ready $dot %d review notes"
$cases = @(
    @{Name='utf8'; Source=('localizedUiLabel("' + $key + '");'); Pass=$true; Error=''},
    @{Name='corrupted-utf8'; Source=('localizedUiLabel("Ready ' + [char]0x00C2 + $dot + ' %d review notes");'); Pass=$false; Error='localized key missing from catalog'},
    @{Name='missing-key'; Source='localizedUiLabel("Untranslated action");'; Pass=$false; Error='localized key missing from catalog'},
    @{Name='direct-label'; Source='ImGui::Button("Untranslated action");'; Pass=$false; Error='ImGui::Button'}
)
foreach ($case in $cases) {
    $fixture = Join-Path $root $case.Name
    foreach ($directory in @('Views','Controllers','Models/Localization')) {
        [IO.Directory]::CreateDirectory((Join-Path $fixture $directory)) | Out-Null
    }
    [IO.File]::WriteAllText((Join-Path $fixture 'Application.cpp'), $case.Source, $utf8)
    $catalog = 'UiLiteralEntry{"' + $key + '", "Bereit ' + $dot + ' %d Pr' + $umlaut + 'fhinweise"},'
    [IO.File]::WriteAllText((Join-Path $fixture 'Models/Localization/TranslationCatalog.cpp'), $catalog, $utf8)
    $passed = $false
    $failure = ''
    try {
        & $gate -RepositoryRoot $fixture *> (Join-Path $fixture 'gate.log')
        $passed = $true
    } catch { $failure = $_.Exception.Message }
    if ($passed -ne $case.Pass -or (-not $passed -and -not $failure.Contains($case.Error))) {
        throw "Localization gate control failed: $($case.Name): $failure"
    }
}
Write-Output 'Localization gate controls passed: UTF-8 without BOM, corrupted UTF-8 key, missing key, and direct label.'
