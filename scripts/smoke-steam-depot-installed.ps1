#requires -Version 5.1

[CmdletBinding(PositionalBinding = $false)]
param(
    [string]$DepotRoot = "dist/steam/windows-x64/GMEditor-Depot",
    [string]$ReportPath = "build/release-evidence/steam-depot-installed-smoke.json",
    [int]$TimeoutSeconds = 20,
    [switch]$KeepArtifacts,
    [ValidateSet('release_candidate','engineering_candidate')][string]$ExpectedPackageKind = 'release_candidate'
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
    return [System.IO.Path]::GetFullPath($candidate).TrimEnd('\', '/')
}

function Test-IsStrictChildPath {
    param(
        [Parameter(Mandatory = $true)][string]$Child,
        [Parameter(Mandatory = $true)][string]$Parent
    )

    $normalizedChild = [System.IO.Path]::GetFullPath($Child).TrimEnd('\', '/')
    $normalizedParent = [System.IO.Path]::GetFullPath($Parent).TrimEnd('\', '/')
    $prefix = $normalizedParent + [System.IO.Path]::DirectorySeparatorChar
    return $normalizedChild.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)
}

function Assert-Condition {
    param(
        [Parameter(Mandatory = $true)][bool]$Condition,
        [Parameter(Mandatory = $true)][string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Write-Utf8File {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Content
    )

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        [System.IO.Directory]::CreateDirectory($parent) | Out-Null
    }
    [System.IO.File]::WriteAllText($Path, $Content, (New-Object System.Text.UTF8Encoding($false)))
}

function Get-RelativePathFromRoot {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Root
    )

    $normalizedPath = [System.IO.Path]::GetFullPath($Path)
    $normalizedRoot = [System.IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
    Assert-Condition -Condition (Test-IsStrictChildPath -Child $normalizedPath -Parent $normalizedRoot) `
        -Message "Path is outside the expected root: $normalizedPath"
    return $normalizedPath.Substring($normalizedRoot.Length).TrimStart('\', '/').Replace('\', '/')
}

function Get-TreeFingerprint {
    param([Parameter(Mandatory = $true)][string]$Root)

    $lines = New-Object System.Collections.Generic.List[string]
    foreach ($directory in @(Get-ChildItem -LiteralPath $Root -Recurse -Directory -Force | Sort-Object FullName)) {
        $relative = Get-RelativePathFromRoot -Path $directory.FullName -Root $Root
        $lines.Add("D  $relative")
    }
    foreach ($file in @(Get-ChildItem -LiteralPath $Root -Recurse -File -Force | Sort-Object FullName)) {
        $relative = Get-RelativePathFromRoot -Path $file.FullName -Root $Root
        $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        $lines.Add("F  $relative  $($file.Length)  $hash")
    }

    $bytes = [System.Text.Encoding]::UTF8.GetBytes([string]::Join("`n", $lines))
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $digest = [System.BitConverter]::ToString($sha.ComputeHash($bytes)).Replace("-", "").ToLowerInvariant()
    }
    finally {
        $sha.Dispose()
    }
    return [PSCustomObject]@{
        EntryCount = $lines.Count
        Sha256 = $digest
    }
}

function Invoke-EditorCli {
    param(
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string]$Arguments,
        [Parameter(Mandatory = $true)][string]$WorkingDirectory,
        [Parameter(Mandatory = $true)][string]$LocalAppData,
        [Parameter(Mandatory = $true)][int]$TimeoutMilliseconds
    )

    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $Executable
    $startInfo.Arguments = $Arguments
    $startInfo.WorkingDirectory = $WorkingDirectory
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $utf8 = New-Object System.Text.UTF8Encoding($false, $true)
    $startInfo.StandardOutputEncoding = $utf8
    $startInfo.StandardErrorEncoding = $utf8

    # Isolate this child without mutating the shell running the smoke test.
    $appDataRoot = Split-Path -Parent $LocalAppData
    $userProfileRoot = Split-Path -Parent $appDataRoot
    $startInfo.EnvironmentVariables["LOCALAPPDATA"] = $LocalAppData
    $startInfo.EnvironmentVariables["APPDATA"] = (Join-Path $appDataRoot "Roaming")
    $startInfo.EnvironmentVariables["USERPROFILE"] = $userProfileRoot
    foreach ($name in @("TTRPG_DATA_ROOT", "TTRPG_DB_PATH", "TTRPG_SETTINGS_PATH")) {
        [void]$startInfo.EnvironmentVariables.Remove($name)
    }

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $startInfo
    try {
        Assert-Condition -Condition $process.Start() -Message "Could not start GMEditor.exe."
        $standardOutputTask = $process.StandardOutput.ReadToEndAsync()
        $standardErrorTask = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit($TimeoutMilliseconds)) {
            try { $process.Kill() } catch { }
            throw "GMEditor.exe timed out after $TimeoutMilliseconds ms for '$Arguments'."
        }
        $standardOutput = $standardOutputTask.Result
        $standardError = $standardErrorTask.Result
        if ($process.ExitCode -ne 0) {
            throw "GMEditor.exe returned exit code $($process.ExitCode) for '$Arguments'. stderr: $standardError"
        }
        return [PSCustomObject]@{
            ExitCode = $process.ExitCode
            StdOut = $standardOutput
            StdErr = $standardError
        }
    }
    finally {
        $process.Dispose()
    }
}

function ConvertFrom-KeyValueOutput {
    param([Parameter(Mandatory = $true)][string]$Text)

    $values = @{}
    foreach ($line in @($Text -split "`r?`n")) {
        if ([string]::IsNullOrWhiteSpace($line)) {
            continue
        }
        if ($line -notmatch '^([^=]+)=(.*)$') {
            throw "Unexpected --print-paths output line: $line"
        }
        $key = $Matches[1]
        if ($values.ContainsKey($key)) {
            throw "Duplicate --print-paths key: $key"
        }
        $values[$key] = $Matches[2]
    }
    return $values
}

function Assert-SamePath {
    param(
        [Parameter(Mandatory = $true)][string]$Actual,
        [Parameter(Mandatory = $true)][string]$Expected,
        [Parameter(Mandatory = $true)][string]$Label
    )

    $actualPath = [System.IO.Path]::GetFullPath($Actual).TrimEnd('\', '/')
    $expectedPath = [System.IO.Path]::GetFullPath($Expected).TrimEnd('\', '/')
    Assert-Condition -Condition $actualPath.Equals($expectedPath, [System.StringComparison]::OrdinalIgnoreCase) `
        -Message "$Label path mismatch. Expected '$expectedPath', got '$actualPath'."
}

function Enable-ReadOnlyInstallAcl {
    param([Parameter(Mandatory = $true)][string]$Root)

    Assert-Condition -Condition ([System.Environment]::OSVersion.Platform -eq [System.PlatformID]::Win32NT) `
        -Message "The installed Steam depot smoke test currently requires Windows."

    $acl = Get-Acl -LiteralPath $Root
    $originalSddl = $acl.Sddl
    $identity = [System.Security.Principal.WindowsIdentity]::GetCurrent().Name
    $rights = [System.Security.AccessControl.FileSystemRights]::WriteData -bor
        [System.Security.AccessControl.FileSystemRights]::AppendData -bor
        [System.Security.AccessControl.FileSystemRights]::WriteExtendedAttributes -bor
        [System.Security.AccessControl.FileSystemRights]::WriteAttributes -bor
        [System.Security.AccessControl.FileSystemRights]::Delete -bor
        [System.Security.AccessControl.FileSystemRights]::DeleteSubdirectoriesAndFiles
    $rule = New-Object System.Security.AccessControl.FileSystemAccessRule(
        $identity,
        $rights,
        ([System.Security.AccessControl.InheritanceFlags]::ContainerInherit -bor [System.Security.AccessControl.InheritanceFlags]::ObjectInherit),
        [System.Security.AccessControl.PropagationFlags]::None,
        [System.Security.AccessControl.AccessControlType]::Deny)
    $aclApplied = $false
    try {
        [void]$acl.AddAccessRule($rule)
        Set-Acl -LiteralPath $Root -AclObject $acl
        $aclApplied = $true

        # Verify the protection without changing a byte.
        $markerPath = Join-Path $Root "gm_editor.install.json"
        $writeWasDenied = $false
        try {
            $stream = New-Object System.IO.FileStream(
                $markerPath,
                [System.IO.FileMode]::Open,
                [System.IO.FileAccess]::Write,
                [System.IO.FileShare]::Read)
            $stream.Dispose()
        }
        catch [System.UnauthorizedAccessException] {
            $writeWasDenied = $true
        }
        Assert-Condition -Condition $writeWasDenied `
            -Message "The temporary install directory could still be opened for writing after applying its read-only ACL."

        return [PSCustomObject]@{
            Applied = $true
            OriginalSddl = $originalSddl
            Identity = $identity
        }
    }
    catch {
        # Do not strand an undeletable temp tree if ACL verification itself
        # fails before the caller receives a restoration token.
        if ($aclApplied) {
            $restoreAcl = Get-Acl -LiteralPath $Root
            $restoreAcl.SetSecurityDescriptorSddlForm($originalSddl)
            Set-Acl -LiteralPath $Root -AclObject $restoreAcl
        }
        throw
    }
}

function Restore-InstallAcl {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)]$State
    )

    if ($null -eq $State -or -not $State.Applied -or -not (Test-Path -LiteralPath $Root -PathType Container)) {
        return
    }
    $acl = Get-Acl -LiteralPath $Root
    $acl.SetSecurityDescriptorSddlForm([string]$State.OriginalSddl)
    Set-Acl -LiteralPath $Root -AclObject $acl
}

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$sourceDepot = Get-NormalizedFullPath -Path $DepotRoot -BasePath $projectRoot
$reportFile = Get-NormalizedFullPath -Path $ReportPath -BasePath $projectRoot
$assertDepotScript = Join-Path $PSScriptRoot "assert-steam-depot.ps1"

Assert-Condition -Condition (Test-Path -LiteralPath $sourceDepot -PathType Container) `
    -Message "Steam depot does not exist: $sourceDepot"
Assert-Condition -Condition (Test-Path -LiteralPath $assertDepotScript -PathType Leaf) `
    -Message "Depot validator is missing: $assertDepotScript"
Assert-Condition -Condition ($TimeoutSeconds -ge 1 -and $TimeoutSeconds -le 300) `
    -Message "TimeoutSeconds must be between 1 and 300."

# Reject a malformed or contaminated source package before copying it into the fixture.
& $assertDepotScript -DepotRoot $sourceDepot -ExpectedPackageKind $ExpectedPackageKind
$sourceExecutablePath = Join-Path $sourceDepot "GMEditor.exe"
$sourceChecksumPath = Join-Path $sourceDepot "SHA256SUMS.txt"
$sourceExecutableSha256 = (Get-FileHash -LiteralPath $sourceExecutablePath -Algorithm SHA256).Hash.ToLowerInvariant()
$sourceChecksumManifestSha256 = (Get-FileHash -LiteralPath $sourceChecksumPath -Algorithm SHA256).Hash.ToLowerInvariant()

$systemTempRoot = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd('\', '/')
$runId = [System.Guid]::NewGuid().ToString("N")
$capitalUWithDiaeresis = [string][char]0x00DC
$capitalAWithDiaeresis = [string][char]0x00C4
$testLeaf = "GMEditor-DepotSmoke-$runId-$capitalUWithDiaeresis" + "nicode"
$testRoot = [System.IO.Path]::GetFullPath((Join-Path $systemTempRoot $testLeaf)).TrimEnd('\', '/')
Assert-Condition -Condition (Test-IsStrictChildPath -Child $testRoot -Parent $systemTempRoot) `
    -Message "Refusing unsafe temporary smoke-test root: $testRoot"
Assert-Condition -Condition ((Split-Path -Leaf $testRoot) -eq $testLeaf) `
    -Message "Temporary smoke-test root has an unexpected leaf name: $testRoot"
Assert-Condition -Condition (-not (Test-Path -LiteralPath $testRoot)) `
    -Message "Temporary smoke-test root already exists: $testRoot"

$installRoot = Join-Path $testRoot ("Steam Bibliothek $capitalUWithDiaeresis\steamapps\common\GM Editor")
$localAppDataRoot = Join-Path $testRoot ("Benutzer $capitalAWithDiaeresis" + "nne\AppData\Local")
$expectedDataRoot = Join-Path $localAppDataRoot "GM Editor"
$workingRoot = Join-Path $testRoot "Arbeitsordner ausserhalb des Depots"
$readOnlyState = $null
$passed = $false

try {
    [System.IO.Directory]::CreateDirectory($installRoot) | Out-Null
    [System.IO.Directory]::CreateDirectory($localAppDataRoot) | Out-Null
    [System.IO.Directory]::CreateDirectory($workingRoot) | Out-Null
    foreach ($entry in @(Get-ChildItem -LiteralPath $sourceDepot -Force)) {
        Copy-Item -LiteralPath $entry.FullName -Destination $installRoot -Recurse -Force
    }

    $executable = Join-Path $installRoot "GMEditor.exe"
    $installMarkerPath = Join-Path $installRoot "gm_editor.install.json"
    Assert-Condition -Condition (Test-Path -LiteralPath $executable -PathType Leaf) `
        -Message "Temporary depot has no GMEditor.exe."
    Assert-Condition -Condition (Test-Path -LiteralPath $installMarkerPath -PathType Leaf) `
        -Message "Temporary depot has no package marker."
    Assert-Condition -Condition ((Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash.ToLowerInvariant() -eq $sourceExecutableSha256) `
        -Message "Temporary install GMEditor.exe does not match the validated source depot binary."
    $installMarker = Get-Content -Raw -LiteralPath $installMarkerPath | ConvertFrom-Json
    Assert-Condition -Condition ([string]$installMarker.product -eq "GM Editor") `
        -Message "Package marker has an unexpected product."
    Assert-Condition -Condition ([string]$installMarker.data_policy -eq "per-user") `
        -Message "Package marker does not request per-user storage."

    # Simulate data left beside an older portable build.  Sources must remain
    # untouched while missing targets are copied into the per-user directory.
    $legacyDatabase = "legacy-db-$runId"
    $legacySettings = "language=de-DE`nlegacy=$runId"
    $legacyCampaign = "legacy-campaign-$runId"
    $legacyConflict = "legacy-must-not-overwrite-$runId"
    $existingConflict = "existing-user-copy-wins-$runId"
    Write-Utf8File -Path (Join-Path $installRoot "TTRPG.db") -Content $legacyDatabase
    Write-Utf8File -Path (Join-Path $installRoot "GMEditor.settings") -Content $legacySettings
    Write-Utf8File -Path (Join-Path $installRoot ("Resources\Campaigns\Alte Welt $capitalUWithDiaeresis\campaign.fixture")) -Content $legacyCampaign
    Write-Utf8File -Path (Join-Path $installRoot "Resources\Campaigns\Konflikt\campaign.fixture") -Content $legacyConflict
    Write-Utf8File -Path (Join-Path $installRoot "Resources\Backups\legacy.backup-fixture") -Content "legacy-backup-$runId"
    Write-Utf8File -Path (Join-Path $installRoot "Resources\Recovery\legacy.recovery-fixture") -Content "legacy-recovery-$runId"
    Write-Utf8File -Path (Join-Path $installRoot "dist\CampaignPackages\legacy.package-fixture") -Content "legacy-package-$runId"
    Write-Utf8File -Path (Join-Path $expectedDataRoot "Resources\Campaigns\Konflikt\campaign.fixture") -Content $existingConflict

    $beforeFingerprint = Get-TreeFingerprint -Root $installRoot
    $readOnlyState = Enable-ReadOnlyInstallAcl -Root $installRoot

    $versionMetadata = @{}
    foreach ($line in @(Get-Content -LiteralPath (Join-Path $installRoot "VERSION.txt"))) {
        if ($line -match '^([^=]+)=(.*)$') {
            $versionMetadata[$Matches[1]] = $Matches[2]
        }
    }
    Assert-Condition -Condition $versionMetadata.ContainsKey("version") `
        -Message "VERSION.txt has no version entry."
    Assert-Condition -Condition $versionMetadata.ContainsKey("binary_sha256") `
        -Message "VERSION.txt has no binary_sha256 entry."
    Assert-Condition -Condition (([string]$versionMetadata["binary_sha256"]).ToLowerInvariant() -eq $sourceExecutableSha256) `
        -Message "VERSION.txt does not identify the exact source GMEditor.exe used by the smoke test."
    $expectedVersionOutput = "GM Editor $($versionMetadata['version']) Early Access"

    $versionResult = Invoke-EditorCli -Executable $executable -Arguments "--version" `
        -WorkingDirectory $workingRoot -LocalAppData $localAppDataRoot `
        -TimeoutMilliseconds ($TimeoutSeconds * 1000)
    Assert-Condition -Condition ($versionResult.StdErr.Length -eq 0) `
        -Message "--version wrote unexpected stderr: $($versionResult.StdErr)"
    Assert-Condition -Condition ($versionResult.StdOut.Trim() -eq $expectedVersionOutput) `
        -Message "Unexpected --version output. Expected '$expectedVersionOutput', got '$($versionResult.StdOut.Trim())'."

    $pathsResult = Invoke-EditorCli -Executable $executable -Arguments "--print-paths" `
        -WorkingDirectory $workingRoot -LocalAppData $localAppDataRoot `
        -TimeoutMilliseconds ($TimeoutSeconds * 1000)
    Assert-Condition -Condition ($pathsResult.StdErr.Length -eq 0) `
        -Message "--print-paths wrote unexpected stderr: $($pathsResult.StdErr)"
    $paths = ConvertFrom-KeyValueOutput -Text $pathsResult.StdOut
    foreach ($requiredKey in @("executable_root", "data_root", "database", "settings", "crash_reports")) {
        Assert-Condition -Condition $paths.ContainsKey($requiredKey) `
            -Message "--print-paths is missing '$requiredKey'."
    }
    Assert-SamePath -Actual $paths["executable_root"] -Expected $installRoot -Label "Executable root"
    Assert-SamePath -Actual $paths["data_root"] -Expected $expectedDataRoot -Label "User data root"
    Assert-SamePath -Actual $paths["database"] -Expected (Join-Path $expectedDataRoot "TTRPG.db") -Label "Database"
    Assert-SamePath -Actual $paths["settings"] -Expected (Join-Path $expectedDataRoot "GMEditor.settings") -Label "Settings"
    Assert-SamePath -Actual $paths["crash_reports"] -Expected (Join-Path $expectedDataRoot "Logs") -Label "Crash-report directory"

    $migratedDatabasePath = Join-Path $expectedDataRoot "TTRPG.db"
    $migratedSettingsPath = Join-Path $expectedDataRoot "GMEditor.settings"
    $migratedCampaignPath = Join-Path $expectedDataRoot ("Resources\Campaigns\Alte Welt $capitalUWithDiaeresis\campaign.fixture")
    $conflictPath = Join-Path $expectedDataRoot "Resources\Campaigns\Konflikt\campaign.fixture"
    Assert-Condition -Condition ((Get-Content -Raw -LiteralPath $migratedDatabasePath) -eq $legacyDatabase) `
        -Message "Legacy database was not copied byte-for-byte into per-user storage."
    Assert-Condition -Condition ((Get-Content -Raw -LiteralPath $migratedSettingsPath) -eq $legacySettings) `
        -Message "Legacy settings were not copied into per-user storage."
    Assert-Condition -Condition ((Get-Content -Raw -LiteralPath $migratedCampaignPath) -eq $legacyCampaign) `
        -Message "Legacy campaign content was not copied into per-user storage."
    Assert-Condition -Condition ((Get-Content -Raw -LiteralPath $conflictPath) -eq $existingConflict) `
        -Message "Legacy campaign migration overwrote an existing user file."
    Assert-Condition -Condition ((Get-Content -Raw -LiteralPath (Join-Path $expectedDataRoot "Resources\Backups\legacy.backup-fixture")) -eq "legacy-backup-$runId") `
        -Message "Legacy backup fixture was not copied."
    Assert-Condition -Condition ((Get-Content -Raw -LiteralPath (Join-Path $expectedDataRoot "Resources\Recovery\legacy.recovery-fixture")) -eq "legacy-recovery-$runId") `
        -Message "Legacy recovery fixture was not copied."
    Assert-Condition -Condition ((Get-Content -Raw -LiteralPath (Join-Path $expectedDataRoot "dist\CampaignPackages\legacy.package-fixture")) -eq "legacy-package-$runId") `
        -Message "Legacy campaign package fixture was not copied."

    # A second resolution must preserve user-owned targets (skip-existing).
    $userDatabase = "newer-user-db-$runId"
    $userSettings = "language=en-US`nnewer=$runId"
    Write-Utf8File -Path $migratedDatabasePath -Content $userDatabase
    Write-Utf8File -Path $migratedSettingsPath -Content $userSettings
    $secondPathsResult = Invoke-EditorCli -Executable $executable -Arguments "--print-paths" `
        -WorkingDirectory $workingRoot -LocalAppData $localAppDataRoot `
        -TimeoutMilliseconds ($TimeoutSeconds * 1000)
    Assert-Condition -Condition ($secondPathsResult.ExitCode -eq 0) `
        -Message "Second --print-paths invocation failed."
    Assert-Condition -Condition ((Get-Content -Raw -LiteralPath $migratedDatabasePath) -eq $userDatabase) `
        -Message "A second run overwrote the existing per-user database."
    Assert-Condition -Condition ((Get-Content -Raw -LiteralPath $migratedSettingsPath) -eq $userSettings) `
        -Message "A second run overwrote the existing per-user settings."
    Assert-Condition -Condition ((Get-Content -Raw -LiteralPath $conflictPath) -eq $existingConflict) `
        -Message "A second run overwrote an existing per-user campaign file."

    $afterFingerprint = Get-TreeFingerprint -Root $installRoot
    Assert-Condition -Condition ($beforeFingerprint.EntryCount -eq $afterFingerprint.EntryCount -and
        $beforeFingerprint.Sha256 -eq $afterFingerprint.Sha256) `
        -Message "GM Editor changed its read-only install tree while resolving or migrating user data."

    $report = [ordered]@{
        schema_version = 2
        status = "passed"
        checked_at_utc = [System.DateTime]::UtcNow.ToString("o")
        source_depot = $sourceDepot
        source_executable_sha256 = $sourceExecutableSha256
        source_checksum_manifest_sha256 = $sourceChecksumManifestSha256
        executable_version = $versionMetadata["version"]
        install_marker = "passed"
        cli_version = "passed"
        cli_paths = "passed"
        unicode_paths = "passed"
        per_user_storage = "passed"
        legacy_database_migration = "passed"
        legacy_settings_migration = "passed"
        legacy_campaign_migration = "passed"
        skip_existing = "passed"
        read_only_install_acl = "passed"
        install_tree_unchanged = "passed"
        crash_report_directory = $paths["crash_reports"]
        install_tree_entries = $afterFingerprint.EntryCount
        install_tree_sha256 = $afterFingerprint.Sha256
    }
    $reportParent = Split-Path -Parent $reportFile
    [System.IO.Directory]::CreateDirectory($reportParent) | Out-Null
    [System.IO.File]::WriteAllText(
        $reportFile,
        ($report | ConvertTo-Json -Depth 5),
        (New-Object System.Text.UTF8Encoding($false)))

    $passed = $true
    Write-Host "Installed Steam depot smoke test passed."
    Write-Host "Version: $($versionMetadata['version'])"
    Write-Host "Depot: $sourceDepot"
    Write-Host "Read-only install: enforced by Windows ACL"
    Write-Host "Unicode per-user storage and non-destructive legacy migration: passed"
    Write-Host "Crash-report directory: $($paths['crash_reports'])"
    Write-Host "Report: $reportFile"
}
finally {
    if ($null -ne $readOnlyState) {
        Restore-InstallAcl -Root $installRoot -State $readOnlyState
    }

    if ($KeepArtifacts) {
        Write-Host "Temporary smoke-test files kept at: $testRoot"
    }
    elseif (Test-Path -LiteralPath $testRoot) {
        # Re-resolve and revalidate the exact GUID-scoped target immediately
        # before the only recursive delete in this script.
        $resolvedTestRoot = (Resolve-Path -LiteralPath $testRoot).Path.TrimEnd('\', '/')
        $safeToDelete = (Test-IsStrictChildPath -Child $resolvedTestRoot -Parent $systemTempRoot) -and
            ((Split-Path -Leaf $resolvedTestRoot) -eq $testLeaf) -and
            $testLeaf.StartsWith("GMEditor-DepotSmoke-$runId-", [System.StringComparison]::Ordinal)
        if (-not $safeToDelete) {
            throw "Refusing to clean an unexpected smoke-test path: $resolvedTestRoot"
        }
        Remove-Item -LiteralPath $resolvedTestRoot -Recurse -Force
    }
}

if (-not $passed) {
    throw "Installed Steam depot smoke test did not reach its success state."
}
