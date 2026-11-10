#requires -Version 5.1

[CmdletBinding(PositionalBinding = $false)]
param(
    [Parameter(Mandatory = $true)][string]$RunRoot,
    [Parameter(Mandatory = $true)][string]$ScenarioId,
    [Parameter(Mandatory = $true)][switch]$ConfirmState,
    [switch]$PlayerSafetyConfirmed,
    [string]$Reviewer = $env:USERNAME,
    [string]$Notes = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.Drawing

if (-not ("VisualSmoke.CaptureWindow" -as [type])) {
    Add-Type @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
namespace VisualSmoke {
    public static class CaptureWindow {
        public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr parameter);
        [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
        [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
        [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr parameter);
        [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);
        [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
        [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr hWnd, StringBuilder text, int count);
        [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hWnd, out RECT rect);
        [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hWnd, ref POINT point);
        [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int command);
        [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
        [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr hWnd);
        [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hWnd, IntPtr after, int x, int y, int width, int height, uint flags);
        [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
        [DllImport("user32.dll")] public static extern void SwitchToThisWindow(IntPtr hWnd, bool altTab);
        [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint first, uint second, bool attach);
        [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
        [DllImport("user32.dll")] public static extern void keybd_event(byte virtualKey, byte scanCode, uint flags, UIntPtr extraInfo);
        public static IntPtr[] WindowsForProcess(uint wanted) {
            var result = new List<IntPtr>();
            EnumWindows(delegate(IntPtr window, IntPtr unused) {
                uint process;
                GetWindowThreadProcessId(window, out process);
                if (process == wanted && IsWindowVisible(window)) result.Add(window);
                return true;
            }, IntPtr.Zero);
            return result.ToArray();
        }
        public static string Title(IntPtr window) {
            var text = new StringBuilder(512);
            GetWindowText(window, text, text.Capacity);
            return text.ToString();
        }
    }
}
"@
}

if (-not $ConfirmState) {
    throw "Captures require -ConfirmState after the reviewer has checked the exact visible scenario."
}

$run = [System.IO.Path]::GetFullPath($RunRoot)
$manifestPath = Join-Path $run "visual-smoke-run.json"
$sessionPath = Join-Path $run "active-session.json"
$queuePath = Join-Path $run "capture-queue.csv"
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "Visual-smoke manifest not found: $manifestPath"
}
if (-not (Test-Path -LiteralPath $sessionPath -PathType Leaf)) {
    throw "No active visual-smoke session found: $sessionPath"
}
if (-not (Test-Path -LiteralPath $queuePath -PathType Leaf)) {
    throw "Capture queue not found: $queuePath"
}
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$session = Get-Content -LiteralPath $sessionPath -Raw | ConvertFrom-Json
$expectedEditorHash = ([string]$manifest.editorSha256).Trim().ToLowerInvariant()
if ($expectedEditorHash -notmatch '^[0-9a-f]{64}$') {
    throw "Run manifest has an invalid editor SHA-256: '$($manifest.editorSha256)'."
}
$manifestEditorPath = [System.IO.Path]::GetFullPath([string]$manifest.editorExecutable)
if (-not (Test-Path -LiteralPath $manifestEditorPath -PathType Leaf)) {
    throw "Editor executable from run manifest not found: $manifestEditorPath"
}
$manifestEditorHash = (Get-FileHash -LiteralPath $manifestEditorPath -Algorithm SHA256).Hash.ToLowerInvariant()
if ($manifestEditorHash -ne $expectedEditorHash) {
    throw "Editor executable hash does not match the run manifest; no screenshot was written. Prepare a new visual-smoke run."
}
if (([string]$session.editorSha256).Trim().ToLowerInvariant() -ne $expectedEditorHash) {
    throw "Active session targets a different editor binary than the run manifest. Start a new visual-smoke session."
}
$matching = @(Import-Csv -LiteralPath $queuePath | Where-Object {
    $_.scenarioId -eq $ScenarioId -and
    $_.viewportKey -eq [string]$session.viewportKey -and
    $_.locale -eq [string]$session.locale
})
if ($matching.Count -ne 1) {
    throw "Scenario '$ScenarioId' does not have exactly one queue entry for $($session.viewportKey) / $($session.locale)."
}
$scenario = $matching[0]
if ($scenario.playerSafe -eq "True" -and -not $PlayerSafetyConfirmed) {
    throw "Player Display requires -PlayerSafetyConfirmed after checking GM notes, secrets, hidden tokens, and unrevealed links."
}
$expectedFixturePrefix = if ($scenario.seedProfile -eq "empty-library") { "empty-" } else { "rich-" }
if (-not ([string]$session.fixture).StartsWith($expectedFixturePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Scenario '$ScenarioId' expects a $expectedFixturePrefix fixture, active fixture is '$($session.fixture)'."
}

$process = Get-Process -Id ([int]$session.processId) -ErrorAction Stop
$savedStart = ([DateTime]$session.processStartTimeUtc).ToUniversalTime()
$actualStart = $process.StartTime.ToUniversalTime()
if ([Math]::Abs(($savedStart - $actualStart).TotalSeconds) -gt 0.01) {
    throw "PID $($session.processId) was reused; start a new visual-smoke session."
}
$actualEditorPath = try { [System.IO.Path]::GetFullPath([string]$process.MainModule.FileName) } catch { "" }
if ([string]::IsNullOrWhiteSpace($actualEditorPath) -or -not (Test-Path -LiteralPath $actualEditorPath -PathType Leaf)) {
    throw "Could not resolve the executable for PID $($process.Id); no screenshot was written."
}
$actualEditorHash = (Get-FileHash -LiteralPath $actualEditorPath -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualEditorHash -ne $expectedEditorHash) {
    throw "Running GM Editor hash does not match the run manifest; no screenshot was written. Start a new visual-smoke session."
}

$windows = [VisualSmoke.CaptureWindow]::WindowsForProcess([uint32]$process.Id)
if ($windows.Count -eq 0) {
    throw "No visible GM Editor window found for PID $($process.Id)."
}
$wantPlayerDisplay = $scenario.playerSafe -eq "True"
$candidates = foreach ($window in $windows) {
    $title = [VisualSmoke.CaptureWindow]::Title($window)
    $rect = New-Object VisualSmoke.CaptureWindow+RECT
    if ([VisualSmoke.CaptureWindow]::GetClientRect($window, [ref]$rect)) {
        [PSCustomObject]@{
            Handle = $window
            Title = $title
            Width = $rect.Right - $rect.Left
            Height = $rect.Bottom - $rect.Top
            Area = ($rect.Right - $rect.Left) * ($rect.Bottom - $rect.Top)
        }
    }
}
if ($wantPlayerDisplay) {
    $candidates = @($candidates | Where-Object { $_.Title -match "Player Display|Spieleranzeige" } | Sort-Object Area -Descending)
} else {
    $candidates = @($candidates | Where-Object { $_.Title -match "^GM Editor" } | Sort-Object Area -Descending)
}
if ($candidates.Count -eq 0) {
    $titles = @($windows | ForEach-Object { [VisualSmoke.CaptureWindow]::Title($_) }) -join " | "
    throw "Expected capture window is not visible. Windows for process: $titles"
}
$target = $candidates[0]
if (-not $wantPlayerDisplay -and
    ($target.Width -ne [int]$session.expectedWidth -or $target.Height -ne [int]$session.expectedHeight)) {
    throw "Main client area is $($target.Width)x$($target.Height); expected $($session.expectedWidth)x$($session.expectedHeight)."
}

[VisualSmoke.CaptureWindow]::ShowWindow($target.Handle, 9) | Out-Null
$previousForeground = [VisualSmoke.CaptureWindow]::GetForegroundWindow()
$ignoredProcessId = [uint32]0
$foregroundThread = [VisualSmoke.CaptureWindow]::GetWindowThreadProcessId($previousForeground, [ref]$ignoredProcessId)
$targetThread = [VisualSmoke.CaptureWindow]::GetWindowThreadProcessId($target.Handle, [ref]$ignoredProcessId)
$currentThread = [VisualSmoke.CaptureWindow]::GetCurrentThreadId()
[VisualSmoke.CaptureWindow]::keybd_event(0x12, 0, 0, [UIntPtr]::Zero)
$attachedForeground = [VisualSmoke.CaptureWindow]::AttachThreadInput($currentThread, $foregroundThread, $true)
$attachedTarget = [VisualSmoke.CaptureWindow]::AttachThreadInput($currentThread, $targetThread, $true)
try {
    [VisualSmoke.CaptureWindow]::BringWindowToTop($target.Handle) | Out-Null
    [VisualSmoke.CaptureWindow]::SetForegroundWindow($target.Handle) | Out-Null
    [VisualSmoke.CaptureWindow]::SwitchToThisWindow($target.Handle, $true)
}
finally {
    if ($attachedTarget) { [VisualSmoke.CaptureWindow]::AttachThreadInput($currentThread, $targetThread, $false) | Out-Null }
    if ($attachedForeground) { [VisualSmoke.CaptureWindow]::AttachThreadInput($currentThread, $foregroundThread, $false) | Out-Null }
    [VisualSmoke.CaptureWindow]::keybd_event(0x12, 0, 2, [UIntPtr]::Zero)
}
if (-not [VisualSmoke.CaptureWindow]::SetWindowPos($target.Handle, [IntPtr](-1), 0, 0, 0, 0, 0x0043)) {
    throw "Could not temporarily place GM Editor above other windows for an unobscured capture."
}
Start-Sleep -Milliseconds 800
$foreground = [VisualSmoke.CaptureWindow]::GetForegroundWindow()
$foregroundProcessId = [uint32]0
[void][VisualSmoke.CaptureWindow]::GetWindowThreadProcessId($foreground, [ref]$foregroundProcessId)
$foregroundConfirmed = $foregroundProcessId -eq [uint32]$process.Id
if (-not $foregroundConfirmed) {
    [VisualSmoke.CaptureWindow]::SetWindowPos($target.Handle, [IntPtr](-2), 0, 0, 0, 0, 0x0043) | Out-Null
    throw "GM Editor could not be made the foreground window; no screenshot was written. Click it and retry."
}
$origin = New-Object VisualSmoke.CaptureWindow+POINT
$origin.X = 0
$origin.Y = 0
if (-not [VisualSmoke.CaptureWindow]::ClientToScreen($target.Handle, [ref]$origin)) {
    throw "Could not resolve the capture window client position."
}

$screenshots = Join-Path $run "screenshots"
[System.IO.Directory]::CreateDirectory($screenshots) | Out-Null
$safeLocale = ([string]$session.locale).Replace("-", "_")
$captureId = "$ScenarioId`__$($session.viewportKey)__$safeLocale"
$imagePath = Join-Path $screenshots "$captureId.png"
$bitmap = New-Object System.Drawing.Bitmap($target.Width, $target.Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
try {
    $graphics.CopyFromScreen($origin.X, $origin.Y, 0, 0, (New-Object System.Drawing.Size($target.Width, $target.Height)), [System.Drawing.CopyPixelOperation]::SourceCopy)
    $bitmap.Save($imagePath, [System.Drawing.Imaging.ImageFormat]::Png)
}
finally {
    $graphics.Dispose()
    $bitmap.Dispose()
    [VisualSmoke.CaptureWindow]::SetWindowPos($target.Handle, [IntPtr](-2), 0, 0, 0, 0, 0x0043) | Out-Null
}

$imageHash = (Get-FileHash -LiteralPath $imagePath -Algorithm SHA256).Hash.ToLowerInvariant()
$metadata = [ordered]@{
    schemaVersion = 1
    captureId = $captureId
    capturedAtUtc = [DateTime]::UtcNow.ToString("o")
    reviewer = $Reviewer
    scenarioId = $ScenarioId
    surface = $scenario.surface
    expectedState = $scenario.expectedState
    navigation = $scenario.navigation
    stateConfirmedByReviewer = $true
    playerSafetyConfirmed = [bool]$PlayerSafetyConfirmed
    locale = [string]$session.locale
    fixture = [string]$session.fixture
    viewportKey = [string]$session.viewportKey
    clientWidth = $target.Width
    clientHeight = $target.Height
    actualDpiScale = [double]$session.actualDpiScale
    windowTitle = $target.Title
    editorExecutable = $actualEditorPath
    editorSha256 = $actualEditorHash
    runManifestSha256 = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash
    imageSha256 = $imageHash
    captureMethod = "Windows client-area CopyFromScreen after explicit manual state confirmation"
    foregroundConfirmed = $foregroundConfirmed
    notes = $Notes
}
$metadataPath = Join-Path $screenshots "$captureId.json"
[System.IO.File]::WriteAllText($metadataPath, (($metadata | ConvertTo-Json -Depth 5) + "`n"), (New-Object System.Text.UTF8Encoding($false)))

$reviewPath = Join-Path $screenshots "$captureId.review.md"
$review = @"
# Visual review: $captureId

- [ ] Correct workspace and expected state are visible
- [ ] No important text overlaps or clips
- [ ] Primary action and current selection are clear
- [ ] Disabled actions explain why
- [ ] German/English copy is consistent for the selected locale
- [ ] Keyboard focus remains visible where applicable
- [ ] Player-facing output contains no GM-only information (if applicable)

Reviewer: $Reviewer

Verdict: pending

Notes:

"@
[System.IO.File]::WriteAllText($reviewPath, $review, (New-Object System.Text.UTF8Encoding($false)))

Write-Host "Captured: $imagePath"
Write-Host "Metadata: $metadataPath"
Write-Host "Review checklist: $reviewPath"
