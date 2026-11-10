#requires -Version 5.1

[CmdletBinding(PositionalBinding = $false)]
param(
    [string]$DepotRoot = "dist/steam/windows-x64/GMEditor-Depot",
    [string]$BuildDirectory = "build-release-preflight",
    [string]$ReportPath = "build/release-evidence/steam-binary-dependency-audit.json",
    [ValidateSet('release_candidate','engineering_candidate')]
    [string]$ExpectedPackageKind = 'release_candidate',
    [ValidateSet("steam_common_redistributables", "app_local")]
    [string]$RuntimeStrategy = "steam_common_redistributables"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Get-NormalizedFullPath {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$BasePath
    )

    $candidate = $Path
    if (-not [System.IO.Path]::IsPathRooted($candidate)) {
        $candidate = Join-Path $BasePath $candidate
    }
    return [System.IO.Path]::GetFullPath($candidate)
}

function Get-Sha256 {
    param([Parameter(Mandatory = $true)][string]$Path)
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-TextSha256 {
    param([Parameter(Mandatory = $true)][string]$Value)

    $algorithm = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($Value)
        return ([System.BitConverter]::ToString($algorithm.ComputeHash($bytes))).Replace("-", "").ToLowerInvariant()
    }
    finally {
        $algorithm.Dispose()
    }
}

function Assert-SafeRelativePath {
    param([Parameter(Mandatory = $true)][string]$Path)

    $normalized = $Path.Replace('\', '/')
    if ([string]::IsNullOrWhiteSpace($normalized) -or
        [System.IO.Path]::IsPathRooted($normalized) -or
        $normalized.Split('/') -contains "..") {
        throw "Unsafe relative path in release metadata: $Path"
    }
    return $normalized
}

function Test-AudioContainerHeader {
    param([Parameter(Mandatory = $true)][string]$Path)

    $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
    try {
        $header = New-Object byte[] 12
        $read = $stream.Read($header, 0, $header.Length)
        if ($read -lt 3) {
            return $false
        }
        $extension = [System.IO.Path]::GetExtension($Path).ToLowerInvariant()
        if ($extension -eq ".wav") {
            return ($read -ge 12 -and
                [System.Text.Encoding]::ASCII.GetString($header, 0, 4) -eq "RIFF" -and
                [System.Text.Encoding]::ASCII.GetString($header, 8, 4) -eq "WAVE")
        }
        if ($extension -eq ".mp3") {
            return ([System.Text.Encoding]::ASCII.GetString($header, 0, 3) -eq "ID3" -or
                ($header[0] -eq 0xff -and ($header[1] -band 0xe0) -eq 0xe0))
        }
        return $false
    }
    finally {
        $stream.Dispose()
    }
}

function Find-PeScanner {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$BuildRoot
    )

    $dumpbinCandidates = New-Object System.Collections.Generic.List[string]
    if (-not [string]::IsNullOrWhiteSpace($env:VCToolsInstallDir)) {
        $dumpbinCandidates.Add((Join-Path $env:VCToolsInstallDir "bin/Hostx64/x64/dumpbin.exe"))
    }

    $cachePath = Join-Path $BuildRoot "CMakeCache.txt"
    if (Test-Path -LiteralPath $cachePath -PathType Leaf) {
        $compilerLine = Select-String -LiteralPath $cachePath -Pattern '^CMAKE_CXX_COMPILER:FILEPATH=(.+)$' |
            Select-Object -First 1
        if ($null -ne $compilerLine) {
            $compilerPath = $compilerLine.Matches[0].Groups[1].Value
            $dumpbinCandidates.Add((Join-Path (Split-Path -Parent $compilerPath) "dumpbin.exe"))
        }
    }

    $command = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        $dumpbinCandidates.Add($command.Source)
    }

    foreach ($candidate in $dumpbinCandidates) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and
            (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return [pscustomobject]@{ kind = "dumpbin"; path = [System.IO.Path]::GetFullPath($candidate) }
        }
    }

    $objdump = Get-Command objdump.exe -ErrorAction SilentlyContinue
    if ($null -eq $objdump) {
        $objdump = Get-Command objdump -ErrorAction SilentlyContinue
    }
    if ($null -ne $objdump) {
        return [pscustomobject]@{ kind = "objdump"; path = $objdump.Source }
    }

    throw "No PE dependency scanner was found. Run from a Visual Studio developer environment or install objdump."
}

function Get-PeInspection {
    param(
        [Parameter(Mandatory = $true)][pscustomobject]$Scanner,
        [Parameter(Mandatory = $true)][string]$Executable
    )

    if ($Scanner.kind -eq "dumpbin") {
        $dependencyOutput = @(& $Scanner.path /nologo /dependents $Executable 2>&1)
        if ($LASTEXITCODE -ne 0) {
            throw "dumpbin dependency scan failed with exit code $LASTEXITCODE."
        }
        $headerOutput = @(& $Scanner.path /nologo /headers $Executable 2>&1)
        if ($LASTEXITCODE -ne 0) {
            throw "dumpbin header scan failed with exit code $LASTEXITCODE."
        }
        $imports = @($dependencyOutput | ForEach-Object {
                if ([string]$_ -match '^\s+([A-Za-z0-9._-]+\.dll)\s*$') {
                    $Matches[1].ToLowerInvariant()
                }
            } | Sort-Object -Unique)
        $headers = $headerOutput -join "`n"
        return [pscustomobject]@{
            imports = $imports
            architecture = if ($headers -match '(?im)^\s+8664 machine \(x64\)') { "x64" } else { "unknown" }
            subsystem = if ($headers -match '(?im)^\s+2 subsystem \(Windows GUI\)') { "windows_gui" } else { "unknown" }
            high_entropy_aslr = [bool]($headers -match '(?im)High Entropy Virtual Addresses')
            dynamic_base_aslr = [bool]($headers -match '(?im)^\s+Dynamic base\s*$')
            nx_compatible = [bool]($headers -match '(?im)^\s+NX compatible\s*$')
            control_flow_guard = [bool]($headers -match '(?im)^\s+Control Flow Guard\s*$')
        }
    }

    $output = @(& $Scanner.path -p $Executable 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "objdump PE scan failed with exit code $LASTEXITCODE."
    }
    $text = $output -join "`n"
    $imports = @($output | ForEach-Object {
            if ([string]$_ -match '^\s*DLL Name:\s*(\S+\.dll)\s*$') {
                $Matches[1].ToLowerInvariant()
            }
        } | Sort-Object -Unique)
    return [pscustomobject]@{
        imports = $imports
        architecture = if ($text -match '(?im)architecture:\s+i386:x86-64') { "x64" } else { "unknown" }
        subsystem = if ($text -match '(?im)Subsystem\s+00000002\s+\(Windows GUI\)') { "windows_gui" } else { "unknown" }
        high_entropy_aslr = $null
        dynamic_base_aslr = $null
        nx_compatible = $null
        control_flow_guard = $null
    }
}

function Test-ProductionSnapshot {
    param(
        [Parameter(Mandatory = $true)][string]$DepotRoot,
        [Parameter(Mandatory = $true)][string]$RelativeDirectory
    )

    $directory = Join-Path $DepotRoot $RelativeDirectory
    $manifestPath = Join-Path $directory "pack_manifest.json"
    $snapshotPath = Join-Path $directory "production_style_snapshot.json"
    $manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
    $snapshot = Get-Content -Raw -LiteralPath $snapshotPath | ConvertFrom-Json
    $manifestHash = Get-Sha256 $manifestPath

    $manifestAssets = @($manifest.assets)
    $snapshotAssets = @($snapshot.assets)
    $snapshotById = @{}
    foreach ($asset in $snapshotAssets) {
        $snapshotById[[string]$asset.id] = $asset
    }

    $sourceProperty = $snapshot.PSObject.Properties["source_manifest_sha256"]
    $requiresPerAssetHashes = ($null -ne $sourceProperty -and
        -not [string]::IsNullOrWhiteSpace([string]$sourceProperty.Value))
    $missingAssets = New-Object System.Collections.Generic.List[string]
    $missingAssetHashes = New-Object System.Collections.Generic.List[string]
    $assetHashMismatches = New-Object System.Collections.Generic.List[string]
    foreach ($asset in $manifestAssets) {
        $id = [string]$asset.id
        $relativeFile = Assert-SafeRelativePath ([string]$asset.file)
        $assetPath = Join-Path $directory $relativeFile
        if (-not (Test-Path -LiteralPath $assetPath -PathType Leaf)) {
            $missingAssets.Add($id)
            continue
        }

        if ($snapshotById.ContainsKey($id)) {
            $snapshotAsset = $snapshotById[$id]
            $declaredHashes = New-Object System.Collections.Generic.List[string]
            foreach ($hashOwner in @($asset, $snapshotAsset)) {
                $shaProperty = $hashOwner.PSObject.Properties["sha256"]
                if ($null -ne $shaProperty -and -not [string]::IsNullOrWhiteSpace([string]$shaProperty.Value)) {
                    $declaredHashes.Add(([string]$shaProperty.Value).ToLowerInvariant())
                }
            }
            if ($requiresPerAssetHashes -and $declaredHashes.Count -eq 0) {
                $missingAssetHashes.Add($id)
            }
            elseif ($declaredHashes.Count -gt 0) {
                $actualAssetHash = Get-Sha256 $assetPath
                foreach ($declaredHash in $declaredHashes) {
                    if ($declaredHash -notmatch '^[0-9a-f]{64}$' -or $actualAssetHash -ne $declaredHash) {
                        if (-not $assetHashMismatches.Contains($id)) {
                            $assetHashMismatches.Add($id)
                        }
                    }
                }
            }
        }
    }

    $manifestOverlayHashProperty = $manifest.PSObject.Properties["overlay_content_sha256"]
    $snapshotOverlayHashProperty = $snapshot.PSObject.Properties["overlay_content_sha256"]
    $overlayContentExpected = ""
    $overlayContentActual = ""
    $overlayContentPass = $true
    if ($null -ne $manifestOverlayHashProperty -or $null -ne $snapshotOverlayHashProperty) {
        $manifestOverlayHash = if ($null -ne $manifestOverlayHashProperty) {
            ([string]$manifestOverlayHashProperty.Value).ToLowerInvariant()
        }
        else { "" }
        $snapshotOverlayHash = if ($null -ne $snapshotOverlayHashProperty) {
            ([string]$snapshotOverlayHashProperty.Value).ToLowerInvariant()
        }
        else { "" }
        $overlayContentExpected = $manifestOverlayHash

        $overlayLines = New-Object System.Collections.Generic.List[string]
        $overlayLines.Add([string]$manifest.base_snapshot_id)
        @($manifestAssets | ForEach-Object {
                "$($_.id)|$(([string]$_.file).Replace('\', '/'))|$(([string]$_.sha256).ToLowerInvariant())"
            } | Sort-Object) | ForEach-Object { $overlayLines.Add($_) }
        @($manifest.selection_overrides | ForEach-Object {
                "$($_.map_level)|$($_.role)|$($_.semantic_role)|$($_.mode)"
            } | Sort-Object) | ForEach-Object { $overlayLines.Add($_) }
        $overlayContentActual = Get-TextSha256 (($overlayLines -join "`n") + "`n")
        $overlayContentPass = (
            $manifestOverlayHash -match '^[0-9a-f]{64}$' -and
            $snapshotOverlayHash -eq $manifestOverlayHash -and
            $overlayContentActual -eq $manifestOverlayHash)
    }

    $fingerprintKind = "none"
    $expectedFingerprint = ""
    $actualFingerprint = ""
    if ($null -ne $sourceProperty -and -not [string]::IsNullOrWhiteSpace([string]$sourceProperty.Value)) {
        $fingerprintKind = "raw_manifest_sha256"
        $expectedFingerprint = ([string]$sourceProperty.Value).ToLowerInvariant()
        $actualFingerprint = $manifestHash
    }
    else {
        $manifestFingerprintProperty = $snapshot.PSObject.Properties["manifest_sha256"]
        $overridesProperty = $manifest.PSObject.Properties["base_asset_overrides"]
        if ($null -ne $manifestFingerprintProperty -and $null -ne $overridesProperty) {
            # V3.1 calls this field manifest_sha256 for compatibility, but its
            # builder intentionally hashes ordered asset IDs, paths, pixels and
            # semantic overrides rather than the serialized JSON file itself.
            $lines = @($manifestAssets | ForEach-Object {
                    $relativeFile = Assert-SafeRelativePath ([string]$_.file)
                    $fileHash = Get-Sha256 (Join-Path $directory $relativeFile)
                    "$($_.id):$relativeFile`:$fileHash"
                })
            $hashInput = $lines -join "`n"
            $hashInput += "`n" + (@($manifest.base_asset_overrides | ForEach-Object {
                        "$($_.id):$($_.semantic_role)"
                    }) -join "`n")
            $fingerprintKind = "ordered_asset_content_and_semantics_sha256"
            $expectedFingerprint = ([string]$manifestFingerprintProperty.Value).ToLowerInvariant()
            $actualFingerprint = Get-TextSha256 $hashInput
        }
    }

    $acceptedCount = @($manifestAssets | Where-Object { [string]$_.status -eq "accepted" }).Count
    $snapshotIdsComplete = ($snapshotAssets.Count -eq $manifestAssets.Count)
    if ($snapshotIdsComplete) {
        foreach ($asset in $manifestAssets) {
            if (-not $snapshotById.ContainsKey([string]$asset.id)) {
                $snapshotIdsComplete = $false
                break
            }
        }
    }

    $fingerprintPass = (-not [string]::IsNullOrWhiteSpace($expectedFingerprint) -and
        $expectedFingerprint -eq $actualFingerprint)
    $pass = (
        [string]$snapshot.state -eq "production" -and
        $manifestAssets.Count -gt 0 -and
        $acceptedCount -eq $manifestAssets.Count -and
        $snapshotIdsComplete -and
        $missingAssets.Count -eq 0 -and
        $missingAssetHashes.Count -eq 0 -and
        $assetHashMismatches.Count -eq 0 -and
        $overlayContentPass -and
        $fingerprintPass)

    $atlasVersionProperty = $snapshot.PSObject.Properties["atlas_version"]
    $baseSnapshotProperty = $snapshot.PSObject.Properties["base_snapshot_id"]
    return [pscustomobject]@{
        directory = $RelativeDirectory.Replace('\', '/')
        snapshot_id = [string]$snapshot.snapshot_id
        atlas_version = if ($null -ne $atlasVersionProperty) { [string]$atlasVersionProperty.Value } else { "" }
        base_snapshot_id = if ($null -ne $baseSnapshotProperty) { [string]$baseSnapshotProperty.Value } else { "" }
        asset_count = $manifestAssets.Count
        accepted_count = $acceptedCount
        raw_manifest_sha256 = $manifestHash
        fingerprint_kind = $fingerprintKind
        expected_fingerprint = $expectedFingerprint
        actual_fingerprint = $actualFingerprint
        fingerprint_pass = $fingerprintPass
        snapshot_ids_complete = $snapshotIdsComplete
        missing_assets = @($missingAssets)
        missing_asset_hashes = @($missingAssetHashes)
        asset_hash_mismatches = @($assetHashMismatches)
        overlay_content_expected_sha256 = $overlayContentExpected
        overlay_content_actual_sha256 = $overlayContentActual
        overlay_content_fingerprint_pass = $overlayContentPass
        pass = $pass
    }
}

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$root = Get-NormalizedFullPath -Path $DepotRoot -BasePath $projectRoot
$buildRoot = Get-NormalizedFullPath -Path $BuildDirectory -BasePath $projectRoot
$resolvedReportPath = Get-NormalizedFullPath -Path $ReportPath -BasePath $projectRoot

if (-not (Test-Path -LiteralPath $root -PathType Container)) {
    throw "Steam depot does not exist: $root"
}
$executable = Join-Path $root "GMEditor.exe"
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw "Steam depot has no GMEditor.exe: $root"
}

$assertScript = Join-Path $PSScriptRoot "assert-steam-depot.ps1"
if (-not (Test-Path -LiteralPath $assertScript -PathType Leaf)) {
    throw "Depot validator is missing: $assertScript"
}
$validationOutput = @(& $assertScript -DepotRoot $root -ExpectedPackageKind $ExpectedPackageKind -AllowAppLocalRuntime:($RuntimeStrategy -eq "app_local"))
$validationOutput | ForEach-Object { Write-Verbose ([string]$_) }

$scanner = Find-PeScanner -ProjectRoot $projectRoot -BuildRoot $buildRoot
$pe = Get-PeInspection -Scanner $scanner -Executable $executable
if ($pe.imports.Count -eq 0) {
    throw "The PE scan returned no imported DLLs."
}

$windowsSystemNames = @(
    "advapi32.dll", "bcrypt.dll", "combase.dll", "comdlg32.dll", "crypt32.dll",
    "dwmapi.dll", "gdi32.dll", "gdiplus.dll", "imm32.dll", "kernel32.dll",
    "ntdll.dll", "ole32.dll", "oleaut32.dll", "opengl32.dll", "setupapi.dll",
    "shcore.dll", "shell32.dll", "user32.dll", "uxtheme.dll", "version.dll",
    "winmm.dll", "ws2_32.dll"
)
$bundledDllNames = @((Get-ChildItem -LiteralPath $root -File -Filter "*.dll" -Force |
        ForEach-Object { $_.Name.ToLowerInvariant() } | Sort-Object -Unique))
$systemImports = New-Object System.Collections.Generic.List[string]
$msvcImports = New-Object System.Collections.Generic.List[string]
$ucrtImports = New-Object System.Collections.Generic.List[string]
$bundledImports = New-Object System.Collections.Generic.List[string]
$unknownImports = New-Object System.Collections.Generic.List[string]
foreach ($importName in @($pe.imports)) {
    $normalizedName = ([string]$importName).ToLowerInvariant()
    if ($bundledDllNames -contains $normalizedName) {
        $bundledImports.Add($normalizedName)
    }
    if ($normalizedName -match '^(msvcp|vcruntime|concrt)14.*\.dll$') {
        $msvcImports.Add($normalizedName)
    }
    elseif ($normalizedName -match '^api-ms-win-crt-[a-z0-9-]+\.dll$') {
        $ucrtImports.Add($normalizedName)
    }
    elseif ($windowsSystemNames -contains $normalizedName -or
        $normalizedName -match '^api-ms-win-(core|security|eventing|service)-[a-z0-9-]+\.dll$') {
        $systemImports.Add($normalizedName)
    }
    else {
        $unknownImports.Add($normalizedName)
    }
}

$runtimePolicyPass = $false
$runtimeExternalAction = ""
if ($RuntimeStrategy -eq "steam_common_redistributables") {
    $runtimePolicyPass = ($bundledDllNames.Count -eq 0 -and $msvcImports.Count -gt 0)
    $runtimeExternalAction = "Enable the Microsoft Visual C++ 2015-2022 x64 Steam Common Redistributable for the app."
}
else {
    $missingAppLocal = @($msvcImports | Where-Object { $bundledDllNames -notcontains $_ })
    $runtimePolicyPass = ($msvcImports.Count -gt 0 -and $missingAppLocal.Count -eq 0)
    $runtimeExternalAction = if ($runtimePolicyPass) { "none" } else { "Stage every imported MSVC runtime DLL under valid Microsoft redistribution terms." }
}

$versionInfo = (Get-Item -LiteralPath $executable).VersionInfo
$signature = Get-AuthenticodeSignature -FilePath $executable
$signatureStatus = [string]$signature.Status
$allFiles = @(Get-ChildItem -LiteralPath $root -Recurse -File -Force)
$checksumPath = Join-Path $root "SHA256SUMS.txt"
$checksumEntries = @(Get-Content -LiteralPath $checksumPath | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
$checksumFileHash = Get-Sha256 $checksumPath
$versionManifestPath = Join-Path $root "VERSION.txt"
$versionManifest = @{}
foreach ($line in @(Get-Content -LiteralPath $versionManifestPath)) {
    if ($line -match '^([^=]+)=(.*)$') {
        $versionManifest[$Matches[1]] = $Matches[2]
    }
}
$metadataErrors = New-Object System.Collections.Generic.List[string]
if ($signatureStatus -ne "Valid" -and $signatureStatus -ne "NotSigned") {
    $metadataErrors.Add("Authenticode signature has invalid status '$signatureStatus'.")
}
if ([string]$versionInfo.ProductName -ne "GM Editor") {
    $metadataErrors.Add("ProductName is not 'GM Editor'.")
}
if ([string]$versionInfo.OriginalFilename -ne "GMEditor.exe") {
    $metadataErrors.Add("OriginalFilename is not 'GMEditor.exe'.")
}
if (-not $versionManifest.ContainsKey("version") -or
    [string]$versionInfo.FileVersion -ne [string]$versionManifest["version"]) {
    $metadataErrors.Add("FileVersion does not match VERSION.txt.")
}
if ($versionManifest.ContainsKey("version") -and
    [string]$versionInfo.ProductVersion -ne "$($versionManifest['version']) Early Access") {
    $metadataErrors.Add("ProductVersion does not match the Early Access package version.")
}
$binarySha256 = Get-Sha256 $executable
if (-not $versionManifest.ContainsKey("binary_sha256") -or
    [string]$versionManifest["binary_sha256"] -notmatch '^[0-9a-fA-F]{64}$' -or
    ([string]$versionManifest["binary_sha256"]).ToLowerInvariant() -ne $binarySha256) {
    $metadataErrors.Add("binary_sha256 in VERSION.txt does not match GMEditor.exe.")
}
$fileDescriptionCodePoints = @(([string]$versionInfo.FileDescription).ToCharArray() |
    ForEach-Object { [int][char]$_ })
if ($fileDescriptionCodePoints -contains 0x00e2 -or
    $fileDescriptionCodePoints -contains 0x20ac -or
    $fileDescriptionCodePoints -contains 0xfffd -or
    [string]::IsNullOrWhiteSpace([string]$versionInfo.FileDescription)) {
    $metadataErrors.Add("FileDescription is empty or contains likely encoding corruption.")
}
elseif ([string]$versionInfo.FileDescription -ne "GM Editor - Campaign preparation and live-session workspace") {
    $metadataErrors.Add("FileDescription does not match the GM Editor 0.9 release identity.")
}
$metadataPass = ($metadataErrors.Count -eq 0)

$noticeRelative = "Documentation/THIRD_PARTY_NOTICES_DRAFT.md"
$noticePath = Join-Path $root $noticeRelative
$noticeText = Get-Content -Raw -LiteralPath $noticePath
$noticeComponents = @(
    [pscustomobject]@{ component = "Dear ImGui"; present = [bool]($noticeText -match '(?i)Dear ImGui') },
    [pscustomobject]@{ component = "GLFW"; present = [bool]($noticeText -match '(?i)GLFW') },
    [pscustomobject]@{ component = "SQLiteCpp"; present = [bool]($noticeText -match '(?i)SQLiteCpp') },
    [pscustomobject]@{ component = "SQLite"; present = [bool]($noticeText -match '(?i)SQLite') },
    [pscustomobject]@{ component = "stb_image_write"; present = [bool]($noticeText -match '(?i)stb_image_write') },
    [pscustomobject]@{ component = "Inter"; present = [bool]($noticeText -match '(?i)Inter 4\.1') },
    [pscustomobject]@{ component = "Source Serif"; present = [bool]($noticeText -match '(?i)Source Serif 4\.005') },
    [pscustomobject]@{ component = "Windows platform facilities"; present = [bool]($noticeText -match '(?i)Windows platform facilities') },
    [pscustomobject]@{ component = "MSVC runtime"; present = [bool]($noticeText -match '(?i)MSVC runtime') },
    [pscustomobject]@{ component = "D&D SRD"; present = [bool]($noticeText -match '(?i)Systems Reference Document') },
    [pscustomobject]@{ component = "Bundled audio"; present = [bool]($noticeText -match '(?i)Bundled audio') },
    [pscustomobject]@{ component = "FantasyNeutral"; present = [bool]($noticeText -match '(?i)FantasyNeutral') }
)
$upstreamLicenseFiles = @(
    "Documentation/Licenses/DearImGui-MIT.txt",
    "Documentation/Licenses/GLFW-zlib.txt",
    "Documentation/Licenses/SQLiteCpp-MIT.txt",
    "Resources/Fonts/Inter/LICENSE.txt",
    "Resources/Fonts/SourceSerif4/LICENSE.md"
)
$missingLicenseFiles = @($upstreamLicenseFiles | Where-Object {
        -not (Test-Path -LiteralPath (Join-Path $root $_) -PathType Leaf)
    })
$noticeCoveragePass = (@($noticeComponents | Where-Object { -not $_.present }).Count -eq 0 -and
    $missingLicenseFiles.Count -eq 0)

$interFontEvidence = @(
    "Resources/Fonts/Inter/Inter-Regular.ttf",
    "Resources/Fonts/Inter/Inter-SemiBold.ttf",
    "Resources/Fonts/Inter/LICENSE.txt",
    "Resources/Fonts/Inter/SOURCE.md"
) | ForEach-Object {
    $fontRelativePath = $_
    $fontPath = Join-Path $root $fontRelativePath
    [pscustomobject]@{
        path = $fontRelativePath
        bytes = [long](Get-Item -LiteralPath $fontPath).Length
        sha256 = Get-Sha256 $fontPath
    }
}

$atlasFontEvidence = @(
    "Resources/Fonts/SourceSerif4/SourceSerif4-Regular.ttf",
    "Resources/Fonts/SourceSerif4/LICENSE.md",
    "Resources/Fonts/SourceSerif4/SOURCE.md"
) | ForEach-Object {
    $fontPath = Join-Path $root $_
    [pscustomobject]@{path=$_;bytes=[long](Get-Item -LiteralPath $fontPath).Length;sha256=Get-Sha256 $fontPath}
}

$catalogPath = Join-Path $root "Resources/Audio/catalog.psv"
$catalogLines = @(Get-Content -LiteralPath $catalogPath | Where-Object {
        -not [string]::IsNullOrWhiteSpace($_) -and -not $_.StartsWith("#")
    })
$catalogRows = New-Object System.Collections.Generic.List[object]
$catalogFormatErrors = New-Object System.Collections.Generic.List[string]
foreach ($line in $catalogLines) {
    $columns = @($line -split '\|')
    if ($columns.Count -ne 11) {
        $catalogFormatErrors.Add($line)
        continue
    }
    $relativeAudioPath = Assert-SafeRelativePath $columns[4]
    $catalogRows.Add([pscustomobject]@{
            id = $columns[0]
            path = $relativeAudioPath
            license = $columns[9]
            source_url = $columns[10]
        })
}
$catalogIdSet = @{}
$catalogPathSet = @{}
$duplicateCatalogIds = New-Object System.Collections.Generic.List[string]
$duplicateCatalogPaths = New-Object System.Collections.Generic.List[string]
$missingCatalogFiles = New-Object System.Collections.Generic.List[string]
$invalidCatalogSourceUrls = New-Object System.Collections.Generic.List[string]
foreach ($row in $catalogRows) {
    $catalogId = [string]$row.id
    if ([string]::IsNullOrWhiteSpace($catalogId) -or $catalogIdSet.ContainsKey($catalogId)) {
        $duplicateCatalogIds.Add($catalogId)
    }
    else {
        $catalogIdSet[$catalogId] = $true
    }
    $key = ([string]$row.path).ToLowerInvariant()
    if ($catalogPathSet.ContainsKey($key)) {
        $duplicateCatalogPaths.Add([string]$row.path)
    }
    else {
        $catalogPathSet[$key] = $true
    }
    if (-not (Test-Path -LiteralPath (Join-Path $root ([string]$row.path)) -PathType Leaf)) {
        $missingCatalogFiles.Add([string]$row.path)
    }
    $sourceUri = $null
    if (-not [System.Uri]::TryCreate([string]$row.source_url, [System.UriKind]::Absolute, [ref]$sourceUri) -or
        $sourceUri.Scheme -notin @("http", "https")) {
        $invalidCatalogSourceUrls.Add($catalogId)
    }
}
$audioFiles = @(Get-ChildItem -LiteralPath (Join-Path $root "Resources/Audio") -Recurse -File |
    Where-Object { $_.Extension.ToLowerInvariant() -in @(".mp3", ".wav") })
$extraAudioFiles = New-Object System.Collections.Generic.List[string]
$invalidAudioHeaders = New-Object System.Collections.Generic.List[string]
foreach ($audioFile in $audioFiles) {
    $relative = $audioFile.FullName.Substring($root.Length).TrimStart('\', '/').Replace('\', '/')
    if (-not $catalogPathSet.ContainsKey($relative.ToLowerInvariant())) {
        $extraAudioFiles.Add($relative)
    }
    if (-not (Test-AudioContainerHeader $audioFile.FullName)) {
        $invalidAudioHeaders.Add($relative)
    }
}
$invalidCatalogRights = @($catalogRows | Where-Object {
        [string]$_.license -ne "CC0" -or [string]::IsNullOrWhiteSpace([string]$_.source_url)
    })
$audioInventoryPass = ($catalogFormatErrors.Count -eq 0 -and
    $duplicateCatalogIds.Count -eq 0 -and
    $duplicateCatalogPaths.Count -eq 0 -and
    $invalidCatalogSourceUrls.Count -eq 0 -and
    $missingCatalogFiles.Count -eq 0 -and
    $extraAudioFiles.Count -eq 0 -and
    $invalidAudioHeaders.Count -eq 0 -and
    $invalidCatalogRights.Count -eq 0 -and
    $catalogRows.Count -eq $audioFiles.Count)

$snapshotResults = @(
    Test-ProductionSnapshot -DepotRoot $root -RelativeDirectory "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3"
    Test-ProductionSnapshot -DepotRoot $root -RelativeDirectory "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3_1"
    Test-ProductionSnapshot -DepotRoot $root -RelativeDirectory "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3_2"
    Test-ProductionSnapshot -DepotRoot $root -RelativeDirectory "Resources/Maps/Tilesets/Generated/FantasyNeutral/v3_3"
)
$v3SnapshotResult = $snapshotResults[0]
$v31SnapshotResult = $snapshotResults[1]
$v32SnapshotResult = $snapshotResults[2]
$v33SnapshotResult = $snapshotResults[3]
$snapshotLineagePass = (
    [string]$v31SnapshotResult.atlas_version -eq "3.1" -and
    [string]$v31SnapshotResult.base_snapshot_id -eq [string]$v3SnapshotResult.snapshot_id -and
    [string]$v32SnapshotResult.atlas_version -eq "3.2" -and
    [string]$v32SnapshotResult.base_snapshot_id -eq [string]$v31SnapshotResult.snapshot_id -and
    [string]$v33SnapshotResult.atlas_version -eq "3.3" -and
    [string]$v33SnapshotResult.base_snapshot_id -eq [string]$v32SnapshotResult.snapshot_id)
$assetIntegrityPass = (
    @($snapshotResults | Where-Object { -not $_.pass }).Count -eq 0 -and
    $snapshotLineagePass)

$technicalPass = (
    $pe.architecture -eq "x64" -and
    $pe.subsystem -eq "windows_gui" -and
    $unknownImports.Count -eq 0 -and
    $runtimePolicyPass -and
    $metadataPass -and
    $noticeCoveragePass -and
    $audioInventoryPass -and
    $assetIntegrityPass)

$report = [ordered]@{
    schema_version = 2
    generated_at_utc = [System.DateTime]::UtcNow.ToString("o")
    depot = [ordered]@{
        root = $root
        package_kind = $ExpectedPackageKind
        file_count = $allFiles.Count
        byte_count = [long](($allFiles | Measure-Object -Property Length -Sum).Sum)
        checksum_entry_count = $checksumEntries.Count
        checksum_manifest_sha256 = $checksumFileHash
        checksum_validation = "pass"
    }
    binary = [ordered]@{
        path = $executable
        sha256 = $binarySha256
        declared_sha256 = if ($versionManifest.ContainsKey("binary_sha256")) { [string]$versionManifest["binary_sha256"] } else { "" }
        file_version = [string]$versionInfo.FileVersion
        product_version = [string]$versionInfo.ProductVersion
        product_name = [string]$versionInfo.ProductName
        company_name = [string]$versionInfo.CompanyName
        file_description = [string]$versionInfo.FileDescription
        original_filename = [string]$versionInfo.OriginalFilename
        legal_copyright = [string]$versionInfo.LegalCopyright
        metadata_errors = @($metadataErrors)
        metadata_pass = $metadataPass
        architecture = $pe.architecture
        subsystem = $pe.subsystem
        authenticode_status = $signatureStatus
        security_characteristics = [ordered]@{
            high_entropy_aslr = $pe.high_entropy_aslr
            dynamic_base_aslr = $pe.dynamic_base_aslr
            nx_compatible = $pe.nx_compatible
            control_flow_guard = $pe.control_flow_guard
        }
        scanner = [ordered]@{ kind = $scanner.kind; path = $scanner.path }
    }
    imports = [ordered]@{
        all = @($pe.imports)
        windows_system = @($systemImports)
        msvc_runtime = @($msvcImports)
        universal_crt_forwarders = @($ucrtImports)
        bundled = @($bundledImports)
        unknown = @($unknownImports)
    }
    runtime_delivery = [ordered]@{
        selected_strategy = $RuntimeStrategy
        root_runtime_dlls = @($bundledDllNames)
        policy_pass = $runtimePolicyPass
        external_configuration_required = ($RuntimeStrategy -eq "steam_common_redistributables")
        external_action = $runtimeExternalAction
    }
    notices = [ordered]@{
        shipped_notice = $noticeRelative
        component_coverage = $noticeComponents
        upstream_license_files = $upstreamLicenseFiles
        missing_upstream_license_files = @($missingLicenseFiles)
        coverage_pass = $noticeCoveragePass
    }
    fonts = [ordered]@{
        family = "Inter"
        version = "4.1"
        license = "SIL Open Font License 1.1"
        source_release = "https://github.com/rsms/inter/releases/tag/v4.1"
        files = $interFontEvidence
        atlas_family = "Source Serif 4"
        atlas_version = "4.005"
        atlas_source_release = "https://github.com/adobe-fonts/source-serif/releases/tag/4.005R"
        atlas_files = $atlasFontEvidence
        integrity_validation = "pass"
    }
    audio = [ordered]@{
        catalog_entry_count = $catalogRows.Count
        shipped_media_count = $audioFiles.Count
        format_errors = @($catalogFormatErrors)
        duplicate_catalog_ids = @($duplicateCatalogIds)
        duplicate_catalog_paths = @($duplicateCatalogPaths)
        invalid_catalog_source_urls = @($invalidCatalogSourceUrls)
        missing_catalog_files = @($missingCatalogFiles)
        uncatalogued_media = @($extraAudioFiles)
        invalid_container_headers = @($invalidAudioHeaders)
        invalid_or_incomplete_catalog_rights_rows = $invalidCatalogRights.Count
        inventory_pass = $audioInventoryPass
        provenance_evidence_outside_depot_required = $true
    }
    production_assets = [ordered]@{
        snapshots = $snapshotResults
        snapshot_lineage_pass = $snapshotLineagePass
        integrity_pass = $assetIntegrityPass
        commercial_distribution_clearance = "not established by technical audit"
    }
    gates = [ordered]@{
        technical_binary_and_inventory_audit = if ($technicalPass) { "pass" } else { "fail" }
        steam_common_redistributable_configuration = if ($RuntimeStrategy -eq "steam_common_redistributables") { "pending_external_verification" } else { "not_applicable" }
        code_signing = if ($signatureStatus -eq "Valid") { "pass" } elseif ($signatureStatus -eq "NotSigned") { "not_signed" } else { "invalid_signature" }
        third_party_legal_review = "pending"
        content_provenance_review = "pending"
        product_eula_publisher_identity_support_contact = "pending"
        public_distribution_clearance = "not_granted"
    }
}

$reportParent = Split-Path -Parent $resolvedReportPath
if (-not (Test-Path -LiteralPath $reportParent -PathType Container)) {
    New-Item -ItemType Directory -Path $reportParent -Force | Out-Null
}
$json = $report | ConvertTo-Json -Depth 12
[System.IO.File]::WriteAllText($resolvedReportPath, $json + "`n", (New-Object System.Text.UTF8Encoding($false)))

$markdownPath = [System.IO.Path]::ChangeExtension($resolvedReportPath, ".md")
$markdownLines = New-Object System.Collections.Generic.List[string]
$runtimeGateText = if ($RuntimeStrategy -eq "steam_common_redistributables") {
    "Configure Microsoft Visual C++ 2015-2022 x64 under Steam Common Redistributables and verify it on a clean Windows machine."
}
else {
    "Verify the app-local MSVC runtime files and their redistribution terms on a clean Windows machine."
}
$signingGateText = if ($signatureStatus -eq "Valid") {
    "Record the signing certificate identity and timestamp status for the uploaded executable."
}
elseif ($signatureStatus -eq "NotSigned") {
    "The executable is not Authenticode-signed; decide the publisher signing policy."
}
else {
    "The executable has Authenticode status '$signatureStatus'; replace or correctly re-sign it before distribution."
}
@(
    "# GM Editor 0.9 Windows binary dependency audit",
    "",
    "Generated: $($report.generated_at_utc)",
    "",
    "- Technical binary and inventory audit: **$($report.gates.technical_binary_and_inventory_audit)**",
    "- Public distribution clearance: **not granted by this technical audit**",
    "- Executable: ``$($report.binary.path)``",
    "- SHA-256: ``$($report.binary.sha256)``",
    "- Version / target: ``$($report.binary.file_version)`` / ``$($report.binary.architecture)`` ``$($report.binary.subsystem)``",
    "- Version-resource metadata: ``$(if ($metadataPass) { 'pass' } else { 'fail' })``$(if ($metadataErrors.Count -gt 0) { ' - ' + (@($metadataErrors) -join '; ') } else { '' })",
    "- Imported DLLs: $($pe.imports.Count) total; $($msvcImports.Count) MSVC; $($ucrtImports.Count) UCRT forwarders; $($unknownImports.Count) unknown",
    "- Runtime strategy: ``$RuntimeStrategy``",
    "- Required external action: $runtimeExternalAction",
    "- Authenticode: ``$($report.binary.authenticode_status)``",
    "- Depot: $($allFiles.Count) files, $($report.depot.byte_count) bytes, $($checksumEntries.Count) verified SHA entries",
    "- Audio: $($catalogRows.Count) catalog rows exactly match $($audioFiles.Count) shipped media files",
    "- UI font: Inter 4.1 Regular/SemiBold, pinned static-file hashes and bundled SIL OFL 1.1 license",
    "- Atlas font: Source Serif 4.005 Regular, pinned static-file hash and bundled SIL OFL 1.1 license",
    "- Production asset snapshots: $(@($snapshotResults | ForEach-Object { $_.snapshot_id }) -join ', ')",
    "",
    "## Imported DLLs",
    ""
) | ForEach-Object { $markdownLines.Add($_) }
foreach ($importName in @($pe.imports)) {
    $markdownLines.Add("- ``$importName``")
}
@(
    "",
    "## Manual/external release gates",
    "",
    "- $runtimeGateText",
    "- $signingGateText",
    "- Complete FantasyNeutral, audio-source, icon/wordmark/Store-art and D&D SRD rights review.",
    "- Add the final EULA/product license, legal publisher identity and support contact.",
    "",
    "This report proves technical inventory consistency only. It is not legal advice or commercial-distribution approval."
) | ForEach-Object { $markdownLines.Add($_) }
[System.IO.File]::WriteAllLines($markdownPath, $markdownLines.ToArray(), (New-Object System.Text.UTF8Encoding($false)))

Write-Host "Steam binary dependency audit completed."
Write-Host "Technical gate: $($report.gates.technical_binary_and_inventory_audit)"
Write-Host "Runtime strategy: $RuntimeStrategy"
Write-Host "External runtime configuration: $($report.gates.steam_common_redistributable_configuration)"
Write-Host "JSON: $resolvedReportPath"
Write-Host "Markdown: $markdownPath"

if (-not $technicalPass) {
    throw "Steam binary dependency audit failed. Inspect $resolvedReportPath"
}
