#requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$CandidateRoot,
    [string]$ReferenceRoot,
    [Parameter(Mandatory)][string]$ReportPath,
    [ValidateRange(0,255)][int]$ChannelTolerance = 0,
    [ValidateRange(0,1)][double]$ChangedFractionLimit = 0
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (-not ('GmVisualPixels' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.IO;
public sealed class GmVisualImage {
    public int Width, Height;
    public byte[] Pixels;
    public string Sha256;
}
public sealed class GmVisualDifference {
    public double ChangedPixelFraction, MeanChannelDelta;
    public int MaximumChannelDelta;
}
public static class GmVisualPixels {
    public static GmVisualImage Read(string path) {
        byte[] bytes = File.ReadAllBytes(path);
        if (bytes.Length < 54 || bytes[0] != 66 || bytes[1] != 77) throw new InvalidDataException("Not a BMP framebuffer");
        int offset = BitConverter.ToInt32(bytes, 10);
        int width = BitConverter.ToInt32(bytes, 18), height = BitConverter.ToInt32(bytes, 22);
        if (BitConverter.ToInt32(bytes, 14) != 40 || width < 1 || height < 1 || width > 16384 || height > 16384 ||
            BitConverter.ToInt16(bytes, 26) != 1 || BitConverter.ToInt16(bytes, 28) != 24 || BitConverter.ToInt32(bytes, 30) != 0)
            throw new InvalidDataException("Expected bottom-up uncompressed 24-bit BMP");
        long stride = ((long)width * 3 + 3) & ~3L;
        if (offset < 54 || offset + stride * height != bytes.LongLength) throw new InvalidDataException("Invalid BMP pixel bounds");
        byte[] pixels = new byte[checked(width * height * 3)];
        for (int row = 0; row < height; ++row)
            Buffer.BlockCopy(bytes, checked(offset + (int)(stride * (height - row - 1))), pixels, row * width * 3, width * 3);
        string hash;
        using (var sha = System.Security.Cryptography.SHA256.Create())
            hash = BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-", "");
        return new GmVisualImage { Width = width, Height = height, Pixels = pixels, Sha256 = hash };
    }
    public static GmVisualDifference Compare(GmVisualImage candidate, GmVisualImage reference, int tolerance) {
        if (candidate.Width != reference.Width || candidate.Height != reference.Height) throw new InvalidDataException("Different framebuffer dimensions");
        long changed = 0, total = 0;
        int maximum = 0;
        for (int i = 0; i < candidate.Pixels.Length; i += 3) {
            int peak = 0;
            for (int channel = 0; channel < 3; ++channel) {
                int delta = Math.Abs((int)candidate.Pixels[i + channel] - reference.Pixels[i + channel]);
                total += delta; peak = Math.Max(peak, delta);
            }
            if (peak > tolerance) ++changed;
            maximum = Math.Max(maximum, peak);
        }
        return new GmVisualDifference { ChangedPixelFraction = (double)changed / (candidate.Pixels.Length / 3),
            MeanChannelDelta = (double)total / candidate.Pixels.Length, MaximumChannelDelta = maximum };
    }
}
'@
}
function Get-Field($Object, [string]$Name) {
    if ($null -eq $Object) { return $null }
    $property = $Object.PSObject.Properties[$Name]
    if ($property) { return $property.Value }
    return $null
}
function Test-Child([string]$Path, [string]$Root) {
    return $Path.StartsWith($Root.TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)
}
function Read-VerifiedImage([string]$Root, $Capture) {
    foreach ($field in @('commit','executable_sha256','assets_sha256','viewport','locale','dpi','renderer')) {
        if ($null -eq (Get-Field $Capture $field) -or [string](Get-Field $Capture $field) -eq '') { throw "Missing capture binding: $field" }
    }
    $path = [IO.Path]::GetFullPath((Join-Path $Root ([string]$Capture.image)))
    if (-not (Test-Child $path $Root) -or -not (Test-Path -LiteralPath $path -PathType Leaf)) { throw 'Capture path is outside the bundle or missing' }
    # Bundles contain regular files. Reject junction/symlink traversal rather
    # than trusting a textual path prefix for an image outside the bundle.
    $component = Get-Item -LiteralPath $path -Force
    while ($component -and $component.FullName -ne $Root) {
        if ($component.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Capture bundle contains a reparse point' }
        $component = if ($component -is [IO.DirectoryInfo]) { $component.Parent } else { $component.Directory }
    }
    $imagePath = [IO.Path]::GetFullPath((Join-Path $Root ([string]$Capture.image)))
    $image = [GmVisualPixels]::Read($imagePath)
    if ($image.Sha256 -ne [string]$Capture.image_sha256) { throw 'Framebuffer hash mismatch' }
    if (@($Capture.viewport).Count -ne 2 -or $image.Width -ne [int]$Capture.viewport[0] -or $image.Height -ne [int]$Capture.viewport[1]) { throw 'Framebuffer dimensions disagree with capture manifest' }
    return $image
}
$candidateDirectory = [IO.Path]::GetFullPath($CandidateRoot).TrimEnd('\','/')
$referenceDirectory = if ($ReferenceRoot) { [IO.Path]::GetFullPath($ReferenceRoot).TrimEnd('\','/') } else { '' }
$report = [IO.Path]::GetFullPath($ReportPath)
if (($referenceDirectory -and (($report -eq $referenceDirectory) -or (Test-Child $report $referenceDirectory))) -or
    $report -eq (Join-Path $candidateDirectory 'captures.json')) { throw 'Comparison report must not overwrite reference data or the candidate manifest' }
$candidate = Get-Content -LiteralPath (Join-Path $candidateDirectory 'captures.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$references = @{}
if ($referenceDirectory) {
    $reference = Get-Content -LiteralPath (Join-Path $referenceDirectory 'captures.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    foreach ($case in $reference.cases) {
        if ($references.ContainsKey([string]$case.id)) { throw 'Duplicate reference case ID' }
        $references[[string]$case.id] = $case
    }
}
$seen = @{}
$results = @(foreach ($case in $candidate.cases) {
    if ($seen.ContainsKey([string]$case.id)) { throw 'Duplicate candidate case ID' }
    $seen[[string]$case.id] = $true
    $result = [ordered]@{ id=[string]$case.id; status='uncaptured' }
    if ((Get-Field $case 'status') -ne 'captured') {
        $result.reason = Get-Field $case 'reason'; [pscustomobject]$result; continue
    }
    $pixels = Read-VerifiedImage $candidateDirectory $case
    $prior = $references[[string]$case.id]
    $approval = Get-Field $prior 'approval'
    if ($null -eq $prior -or (Get-Field $prior 'status') -ne 'captured' -or
        -not (Get-Field $approval 'reviewer') -or -not (Get-Field $approval 'approved_at_utc')) {
        $result.status='needs-reference'; [pscustomobject]$result; continue
    }
    $priorPixels = Read-VerifiedImage $referenceDirectory $prior
    if ((Get-Field $approval 'image_sha256') -ne $prior.image_sha256) {
        $result.status='needs-reference'; $result.reason='Approval is not bound to the current reference image'; [pscustomobject]$result; continue
    }
    $mismatches = @(foreach ($field in @('assets_sha256','viewport','locale','dpi','renderer','text_scale')) {
        if ((ConvertTo-Json -InputObject (Get-Field $case $field) -Compress) -cne (ConvertTo-Json -InputObject (Get-Field $prior $field) -Compress)) { $field }
    })
    if ($mismatches.Count) {
        $result.status='incompatible-reference'; $result.reason=$mismatches -join ', '; [pscustomobject]$result; continue
    }
    $difference = [GmVisualPixels]::Compare($pixels,$priorPixels,$ChannelTolerance)
    $result.status = if ($difference.ChangedPixelFraction -le $ChangedFractionLimit) { 'match' } else { 'different' }
    $result.changed_pixel_fraction=$difference.ChangedPixelFraction
    $result.maximum_channel_delta=$difference.MaximumChannelDelta
    $result.mean_channel_delta=$difference.MeanChannelDelta
    [pscustomobject]$result
})
$allMatched = $results.Count -gt 0 -and @($results | Where-Object status -ne 'match').Count -eq 0
$resultManifest = [ordered]@{ reference_approval_modified=$false; channel_tolerance=$ChannelTolerance;
    changed_fraction_limit=$ChangedFractionLimit; cases=$results; all_cases_matched=$allMatched }
[IO.Directory]::CreateDirectory((Split-Path -Parent $report)) | Out-Null
$resultManifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $report -Encoding UTF8
if (-not $allMatched) { exit 2 }
