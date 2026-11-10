#requires -Version 5.1
[CmdletBinding()]
param([switch]$CheckOnly)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repository = 'PascalMeusel/TTRPG_Editor_cpp'
$runnerName = 'gm-editor-windows'
$root = 'C:\GMEditorCI'
$runnerRoot = Join-Path $root 'runner'
$accountName = 'GmEditorCI'
$version = '2.337.0'
$sha256 = '1150692afa94e71f872017e254ea55b6eece1eece3fe7e3a6d4c93d0a1b85cfc'
$gh = 'C:\Program Files\GitHub CLI\gh.exe'
$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
foreach ($required in @($gh, $vswhere, 'C:\Program Files\Git\cmd\git.exe', 'C:\Program Files\CMake\bin\cmake.exe')) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Missing prerequisite: $required" }
}
$vs = & $vswhere -latest -prerelease -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Install the Visual Studio C++ desktop workload first.' }
$ninja = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
if (-not (Test-Path -LiteralPath $ninja)) { throw 'Visual Studio CMake/Ninja tools are missing.' }
$metadataText = & $gh api "repos/$repository"
if ($LASTEXITCODE) { throw 'GitHub authentication failed. Sign in as the repository owner with gh auth login.' }
$metadata = $metadataText | ConvertFrom-Json
if (-not $metadata.private -or -not $metadata.permissions.admin) { throw 'A private repository and administrator access are required.' }
$runnerListing = & $gh api "repos/$repository/actions/runners"
if ($LASTEXITCODE) { throw 'Could not inspect existing GitHub runners.' }
$existing = @(($runnerListing | ConvertFrom-Json).runners | Where-Object { $_.name -eq $runnerName })
if ($CheckOnly) {
    Write-Host "Prerequisites passed. Will install $runnerName in $runnerRoot as local non-admin account $accountName."
    return
}
$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Open Windows PowerShell as Administrator under your signed-in Windows account, then run this script.'
}
if (Test-Path -LiteralPath (Join-Path $runnerRoot '.runner')) {
    $config = Get-Content -LiteralPath (Join-Path $runnerRoot '.runner') -Raw | ConvertFrom-Json
    if ($config.agentName -ne $runnerName -or $config.gitHubUrl.TrimEnd('/') -ne "https://github.com/$repository") { throw 'Existing runner configuration belongs to another runner/repository.' }
    $serviceName = (Get-Content -LiteralPath (Join-Path $runnerRoot '.service') -Raw).Trim()
    $service = Get-CimInstance Win32_Service | Where-Object Name -eq $serviceName
    if (-not $service -or $service.StartName -notin @(".\$accountName", "$env:COMPUTERNAME\$accountName")) { throw 'Existing service identity does not match the dedicated account.' }
    Start-Service -Name $serviceName
    Write-Host "Runner service is installed and running: $serviceName"
    return
}
if (Get-LocalUser -Name $accountName -ErrorAction SilentlyContinue) { throw "Local account $accountName already exists; refusing to change its password or reuse an unverified identity." }
if ($existing) { throw 'A runner with this name already exists on GitHub; inspect it before retrying.' }
[IO.Directory]::CreateDirectory($root) | Out-Null
# Restrict the dedicated root before downloading binaries or storing runner credentials.
$acl = New-Object Security.AccessControl.DirectorySecurity
$acl.SetAccessRuleProtection($true, $false)
foreach ($sidText in @('S-1-5-18', 'S-1-5-32-544')) {
    $sid = New-Object Security.Principal.SecurityIdentifier($sidText)
    $rule = New-Object Security.AccessControl.FileSystemAccessRule($sid, 'FullControl', 'ContainerInherit,ObjectInherit', 'None', 'Allow')
    $acl.AddAccessRule($rule)
}
Set-Acl -LiteralPath $root -AclObject $acl
$archive = Join-Path $root "actions-runner-win-x64-$version.zip"
if (-not (Test-Path -LiteralPath $archive)) {
    Invoke-WebRequest -UseBasicParsing -Uri "https://github.com/actions/runner/releases/download/v$version/actions-runner-win-x64-$version.zip" -OutFile $archive
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $sha256) { throw 'Runner archive SHA-256 mismatch. Nothing will be executed.' }
if (-not (Test-Path -LiteralPath (Join-Path $runnerRoot 'config.cmd'))) { Expand-Archive -LiteralPath $archive -DestinationPath $runnerRoot }
$randomBytes = New-Object byte[] 48
$rng = [Security.Cryptography.RandomNumberGenerator]::Create()
$rng.GetBytes($randomBytes)
$rng.Dispose()
$password = 'Gm9!' + [Convert]::ToBase64String($randomBytes)
$securePassword = ConvertTo-SecureString -String $password -AsPlainText -Force
$account = New-LocalUser -Name $accountName -Password $securePassword -AccountNeverExpires -PasswordNeverExpires -UserMayNotChangePassword -Description 'GM Editor dedicated CI service'
$acl = Get-Acl -LiteralPath $root
$acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule($account.SID, 'Modify', 'ContainerInherit,ObjectInherit', 'None', 'Allow')))
Set-Acl -LiteralPath $root -AclObject $acl
[IO.Directory]::CreateDirectory((Join-Path $root 'evidence')) | Out-Null
$registration = & $gh api --method POST "repos/$repository/actions/runners/registration-token" | ConvertFrom-Json
if ($LASTEXITCODE -or -not $registration.token) { throw 'Runner registration-token request failed.' }
try {
    # Supported runner input variables keep passwords and registration tokens off command lines.
    $env:ACTIONS_RUNNER_INPUT_TOKEN = $registration.token
    $env:ACTIONS_RUNNER_INPUT_WINDOWSLOGONPASSWORD = $password
    Push-Location $runnerRoot
    try {
        & .\config.cmd --unattended --url "https://github.com/$repository" --name $runnerName --labels gm-editor --work _work --runasservice --windowslogonaccount "$env:COMPUTERNAME\$accountName"
        if ($LASTEXITCODE) { throw 'Runner configuration failed. Retain this installation for diagnosis; do not rerun with --replace.' }
    } finally { Pop-Location }
} finally {
    Remove-Item Env:ACTIONS_RUNNER_INPUT_TOKEN -ErrorAction SilentlyContinue
    Remove-Item Env:ACTIONS_RUNNER_INPUT_WINDOWSLOGONPASSWORD -ErrorAction SilentlyContinue
    $password = $null
    $registration = $null
    $securePassword.Dispose()
}
$serviceName = (Get-Content -LiteralPath (Join-Path $runnerRoot '.service') -Raw).Trim()
$service = Get-CimInstance Win32_Service | Where-Object Name -eq $serviceName
if (-not $service -or $service.StartName -notin @(".\$accountName", "$env:COMPUTERNAME\$accountName")) { throw 'Runner service identity verification failed.' }
Write-Host "Installed $serviceName as $($service.StartName). GitHub runner: $runnerName."
Write-Host 'Keep this PC awake and connected while CI runs. Desktop/GPU acceptance runs separately in your signed-in session.'
