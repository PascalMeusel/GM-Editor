#requires -Version 5.1
param([Parameter(Mandatory)][string]$CaptureDirectory)
$ErrorActionPreference = 'Stop'
if (-not ('GmPlayerPixelControl' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.IO;
public static class GmPlayerPixelControl {
    public static double[] Measure(string path) {
        var bytes = File.ReadAllBytes(path);
        if (bytes.Length < 54 || bytes[0] != 66 || bytes[1] != 77 ||
            BitConverter.ToInt16(bytes,28) != 24 || BitConverter.ToInt32(bytes,30) != 0)
            throw new InvalidDataException("Expected an uncompressed 24-bit framebuffer");
        int width=BitConverter.ToInt32(bytes,18), height=BitConverter.ToInt32(bytes,22);
        int offset=BitConverter.ToInt32(bytes,10);
        if (width < 100 || height < 100 || width > 16384 || height > 16384)
            throw new InvalidDataException("Player framebuffer has invalid dimensions");
        int stride=(width*3+3)&~3;
        if (offset < 54 || (long)offset+(long)stride*height != bytes.LongLength)
            throw new InvalidDataException("Player framebuffer has invalid bounds");
        long count=0, bright=0; int maximum=0;
        // Exclude window chrome; these coordinates are physical capture pixels.
        for (int y=60; y<height-20; ++y) for (int x=20; x<width-20; ++x) {
            int index=offset+(height-y-1)*stride+x*3;
            int peak=Math.Max(bytes[index],Math.Max(bytes[index+1],bytes[index+2]));
            maximum=Math.Max(maximum,peak); ++count; if (peak>8) ++bright;
        }
        return new double[]{maximum,(double)bright/count};
    }
}
'@
}
$measurements = @(foreach ($name in @('player-blackout','player-published','player-revoked')) {
    $values = [GmPlayerPixelControl]::Measure((Join-Path $CaptureDirectory "$name/capture.bmp"))
    $passed = if ($name -eq 'player-published') { $values[1] -gt 0.10 } else { $values[0] -le 3 }
    [ordered]@{capture=$name; maximum_channel=$values[0]; bright_fraction=$values[1]; passed=$passed}
})
$measurements | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $CaptureDirectory 'player-pixel-controls.json') -Encoding UTF8
if (@($measurements | Where-Object { -not $_.passed }).Count) { throw 'Player framebuffer positive/negative control failed.' }
