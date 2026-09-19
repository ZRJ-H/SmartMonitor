$repoRoot = Split-Path -Parent $PSScriptRoot
$outputDir = Join-Path $repoRoot 'bin'
$requiredFiles = @(
    'day20_smart_monitor.exe',
    'libopencv_world4140.dll'
)

$missing = @(
    $requiredFiles | Where-Object {
        -not (Test-Path -LiteralPath (Join-Path $outputDir $_))
    }
)

if ($missing.Count -gt 0) {
    throw "Missing runtime dependencies in $outputDir`: $($missing -join ', ')"
}

Write-Output "PASS: runtime dependencies are present in $outputDir"
