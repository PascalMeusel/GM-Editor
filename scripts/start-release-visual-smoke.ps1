#requires -Version 5.1

[CmdletBinding(PositionalBinding = $false)]
param(
    [Parameter(Mandatory = $true)][string]$RunRoot,
    [Parameter(Mandatory = $true)][string]$Fixture,
    [ValidateSet("1280x720", "1920x1080", "ultrawide", "1920x1080-150pct")]
    [string]$ViewportKey = "1280x720",
    [int]$StartupTimeoutSeconds = 30,
    [switch]$AllowHostMismatch
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.Windows.Forms

if (-not ("VisualSmoke.NativeWindow" -as [type])) {
    Add-Type @"
using System;
using System.Runtime.InteropServices;
namespace VisualSmoke {
    public static class NativeWindow {
        [StructLayout(LayoutKind.Sequential)]
        public struct RECT { public int Left, Top, Right, Bottom; }
        [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
        [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hWnd, out RECT rect);
        [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hWnd, IntPtr after, int x, int y, int width, int height, uint flags);
        [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int command);
        [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
        [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr hWnd);
        [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
        [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);
        [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint first, uint second, bool attach);
        [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
        [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr hWnd);
        [DllImport("user32.dll")] public static extern void keybd_event(byte virtualKey, byte scanCode, uint flags, UIntPtr extraInfo);
    }
}
"@
}

$run = [System.IO.Path]::GetFullPath($RunRoot)
$manifestPath = Join-Path $run "visual-smoke-run.json"
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "Visual-smoke manifest not found: $manifestPath"
}
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$editorPath = [System.IO.Path]::GetFullPath([string]$manifest.editorExecutable)
$expectedEditorHash = ([string]$manifest.editorSha256).Trim().ToLowerInvariant()
if ($expectedEditorHash -notmatch '^[0-9a-f]{64}$') {
    throw "Run manifest has an invalid editor SHA-256: '$($manifest.editorSha256)'."
}
if (-not (Test-Path -LiteralPath $editorPath -PathType Leaf)) {
    throw "Editor executable from run manifest not found: $editorPath"
}
$actualEditorHash = (Get-FileHash -LiteralPath $editorPath -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualEditorHash -ne $expectedEditorHash) {
    throw "Editor executable hash does not match the run manifest. Prepare a new visual-smoke run before starting GM Editor."
}
$fixtureRecord = @($manifest.fixtures | Where-Object { $_.id -eq $Fixture })
if ($fixtureRecord.Count -ne 1) {
    throw "Unknown fixture '$Fixture'. Available: $(@($manifest.fixtures.id) -join ', ')"
}
$viewport = @($manifest.viewports | Where-Object { $_.key -eq $ViewportKey })
if ($viewport.Count -ne 1) {
    throw "Viewport '$ViewportKey' is not present in the run manifest."
}

$screen = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
if (($screen.Width -lt [int]$viewport.width -or $screen.Height -lt [int]$viewport.height) -and -not $AllowHostMismatch) {
    throw "Primary display is $($screen.Width)x$($screen.Height); $ViewportKey requires $($viewport.width)x$($viewport.height). Use a matching display."
}

$startInfo = New-Object System.Diagnostics.ProcessStartInfo
$startInfo.FileName = $editorPath
$startInfo.WorkingDirectory = Split-Path -Parent $editorPath
$startInfo.UseShellExecute = $false
$startInfo.CreateNoWindow = $true
$startInfo.EnvironmentVariables["TTRPG_DATA_ROOT"] = [string]$fixtureRecord[0].dataRoot
foreach ($name in @("TTRPG_DB_PATH", "TTRPG_SETTINGS_PATH")) {
    [void]$startInfo.EnvironmentVariables.Remove($name)
}

$process = New-Object System.Diagnostics.Process
$process.StartInfo = $startInfo
if (-not $process.Start()) {
    throw "Could not start GM Editor."
}

$deadline = [DateTime]::UtcNow.AddSeconds($StartupTimeoutSeconds)
do {
    Start-Sleep -Milliseconds 100
    $process.Refresh()
} while ($process.MainWindowHandle -eq [IntPtr]::Zero -and -not $process.HasExited -and [DateTime]::UtcNow -lt $deadline)

if ($process.HasExited) {
    throw "GM Editor exited during startup with code $($process.ExitCode)."
}
if ($process.MainWindowHandle -eq [IntPtr]::Zero) {
    try { $process.Kill() } catch { }
    throw "GM Editor did not create its main window within $StartupTimeoutSeconds seconds."
}

$window = $process.MainWindowHandle
[VisualSmoke.NativeWindow]::ShowWindow($window, 9) | Out-Null
$windowRect = New-Object VisualSmoke.NativeWindow+RECT
$clientRect = New-Object VisualSmoke.NativeWindow+RECT
if (-not [VisualSmoke.NativeWindow]::GetWindowRect($window, [ref]$windowRect) -or
    -not [VisualSmoke.NativeWindow]::GetClientRect($window, [ref]$clientRect)) {
    throw "Could not inspect the GM Editor window."
}
$frameWidth = ($windowRect.Right - $windowRect.Left) - ($clientRect.Right - $clientRect.Left)
$frameHeight = ($windowRect.Bottom - $windowRect.Top) - ($clientRect.Bottom - $clientRect.Top)
$outerWidth = [int]$viewport.width + $frameWidth
$outerHeight = [int]$viewport.height + $frameHeight
if (-not [VisualSmoke.NativeWindow]::SetWindowPos($window, [IntPtr]::Zero, 0, 0, $outerWidth, $outerHeight, 0x0004)) {
    throw "Could not resize the GM Editor window."
}
$previousForeground = [VisualSmoke.NativeWindow]::GetForegroundWindow()
$ignoredProcessId = [uint32]0
$foregroundThread = [VisualSmoke.NativeWindow]::GetWindowThreadProcessId($previousForeground, [ref]$ignoredProcessId)
$targetThread = [VisualSmoke.NativeWindow]::GetWindowThreadProcessId($window, [ref]$ignoredProcessId)
$currentThread = [VisualSmoke.NativeWindow]::GetCurrentThreadId()
[VisualSmoke.NativeWindow]::keybd_event(0x12, 0, 0, [UIntPtr]::Zero)
$attachedForeground = [VisualSmoke.NativeWindow]::AttachThreadInput($currentThread, $foregroundThread, $true)
$attachedTarget = [VisualSmoke.NativeWindow]::AttachThreadInput($currentThread, $targetThread, $true)
try {
    [VisualSmoke.NativeWindow]::BringWindowToTop($window) | Out-Null
    [VisualSmoke.NativeWindow]::SetForegroundWindow($window) | Out-Null
}
finally {
    if ($attachedTarget) { [VisualSmoke.NativeWindow]::AttachThreadInput($currentThread, $targetThread, $false) | Out-Null }
    if ($attachedForeground) { [VisualSmoke.NativeWindow]::AttachThreadInput($currentThread, $foregroundThread, $false) | Out-Null }
    [VisualSmoke.NativeWindow]::keybd_event(0x12, 0, 2, [UIntPtr]::Zero)
}

Start-Sleep -Milliseconds 750
$process.Refresh()
[VisualSmoke.NativeWindow]::GetClientRect($window, [ref]$clientRect) | Out-Null
$actualWidth = $clientRect.Right - $clientRect.Left
$actualHeight = $clientRect.Bottom - $clientRect.Top
if (($actualWidth -ne [int]$viewport.width -or $actualHeight -ne [int]$viewport.height) -and -not $AllowHostMismatch) {
    throw "Client area is ${actualWidth}x${actualHeight}, expected $($viewport.width)x$($viewport.height)."
}

$dpi = [VisualSmoke.NativeWindow]::GetDpiForWindow($window)
$actualScale = [Math]::Round($dpi / 96.0, 2)
if ([double]$viewport.uiScale -gt 1.0 -and [Math]::Abs($actualScale - [double]$viewport.uiScale) -gt 0.08 -and -not $AllowHostMismatch) {
    try { $process.CloseMainWindow() | Out-Null } catch { }
    throw "Window DPI scale is $actualScale; $ViewportKey requires $($viewport.uiScale). Change Windows display scaling before capture."
}

$session = [ordered]@{
    schemaVersion = 1
    processId = $process.Id
    processStartTimeUtc = $process.StartTime.ToUniversalTime().ToString("o")
    fixture = $Fixture
    locale = [string]$fixtureRecord[0].locale
    dataRoot = [string]$fixtureRecord[0].dataRoot
    viewportKey = [string]$viewport.key
    expectedWidth = [int]$viewport.width
    expectedHeight = [int]$viewport.height
    expectedUiScale = [double]$viewport.uiScale
    actualDpiScale = $actualScale
    editorExecutable = $editorPath
    editorSha256 = $actualEditorHash
}
$sessionPath = Join-Path $run "active-session.json"
[System.IO.File]::WriteAllText($sessionPath, (($session | ConvertTo-Json -Depth 5) + "`n"), (New-Object System.Text.UTF8Encoding($false)))

Write-Host "GM Editor started for isolated visual smoke."
Write-Host "PID: $($process.Id) | Fixture: $Fixture | Locale: $($fixtureRecord[0].locale)"
Write-Host "Client: ${actualWidth}x${actualHeight} | DPI scale: $actualScale"
Write-Host "Navigate using capture-queue.csv, then call capture-release-visual-smoke.ps1 with -ConfirmState."
