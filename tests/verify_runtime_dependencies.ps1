param(
    [string]$OutputDir = (Join-Path (Split-Path -Parent $PSScriptRoot) 'bin'),
    [switch]$RequireH264
)
$ErrorActionPreference = 'Stop'
$requiredFiles = @('day20_smart_monitor.exe','libopencv_world4140.dll','opencv_videoio_ffmpeg4140_64.dll')
if ($RequireH264) { $requiredFiles += 'openh264-2.5.0-win64.dll' }
$missing = @($requiredFiles | Where-Object { -not (Test-Path -LiteralPath (Join-Path $OutputDir $_) -PathType Leaf) })
if ($missing.Count -gt 0) { throw "Missing runtime dependencies in $OutputDir`: $($missing -join ', ')" }
if (-not (Test-Path -LiteralPath (Join-Path $OutputDir 'openh264-2.5.0-win64.dll'))) {
    Write-Warning 'OpenH264 missing: install the Cisco binary separately to enable H.264; mp4v fallback remains available.'
}
Write-Output "PASS: runtime dependency files are present in $OutputDir (this does not replace an encoding test)"
