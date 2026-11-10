[CmdletBinding(PositionalBinding = $false)]
param()

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "tileset-validation-lib.ps1")

$sourceQa = [pscustomobject]@{
    case_count = 3
    case_filter = "balanced"
    available_case_count = 8
    case_offset = 2
    case_limit = 3
    context_fixtures = $true
    art_readiness = $true
    contextual_asset_coverage = [pscustomobject]@{
        accepted_asset_count = 0
        target_asset_count = 542
        candidate_asset_count = 542
    }
    timing = [pscustomobject]@{
        case_loop_elapsed_ms = 1057
        preflight_elapsed_ms = 770
        art_readiness_warmup_elapsed_ms = 770
    }
}

$line = Format-TilesetSourceQaLine $sourceQa
if (-not $line.Contains('timing case loop `1.1 s`')) {
    throw "Expected invariant seconds formatting in source QA line, got: $line"
}
if ($line -like '*1,1 s*') {
    throw "Source QA line used locale-specific decimal comma: $line"
}
if (-not $line.Contains('preflight `770 ms` (style warm-up `770 ms`)')) {
    throw "Expected preflight and style warm-up timing in source QA line, got: $line"
}
if (-not $line.Contains('slice offset `2`, limit `3` from `8` available')) {
    throw "Expected case-slice provenance in source QA line, got: $line"
}

Write-Host $line
