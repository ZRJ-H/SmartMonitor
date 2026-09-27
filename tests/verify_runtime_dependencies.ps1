param(
    [string]$OutputDir = (Join-Path (Split-Path -Parent $PSScriptRoot) 'bin'),
    [switch]$RequireH264,
    [switch]$CleanStart
)
$ErrorActionPreference = 'Stop'
$requiredFiles = @('day20_smart_monitor.exe','libopencv_world4140.dll','opencv_videoio_ffmpeg4140_64.dll')
if ($RequireH264) { $requiredFiles += 'openh264-2.5.0-win64.dll' }
$missing = @($requiredFiles | Where-Object { -not (Test-Path -LiteralPath (Join-Path $OutputDir $_) -PathType Leaf) })
if ($missing.Count -gt 0) { throw "Missing runtime dependencies in $OutputDir`: $($missing -join ', ')" }
if (-not (Test-Path -LiteralPath (Join-Path $OutputDir 'openh264-2.5.0-win64.dll'))) {
    Write-Warning 'OpenH264 missing: install the Cisco binary separately to enable H.264; mp4v fallback remains available.'
}
if($CleanStart){
    $names=@('PATH','QT_PLUGIN_PATH','QT_QPA_PLATFORM','SMART_MONITOR_DB_DRIVER','SMART_MONITOR_DB_NAME','SMART_MONITOR_AUTOSTART')
    $saved=@{}
    foreach($name in $names){$saved[$name]=[Environment]::GetEnvironmentVariable($name,'Process')}
    try{
        $env:PATH="$env:SystemRoot/System32;$env:SystemRoot"
        $env:QT_PLUGIN_PATH=''
        $env:QT_QPA_PLATFORM='offscreen'
        $env:SMART_MONITOR_DB_DRIVER='QSQLITE'
        $env:SMART_MONITOR_DB_NAME=':memory:'
        $env:SMART_MONITOR_AUTOSTART='0'
        $output=Join-Path $OutputDir 'runtime-check.stdout.log'
        $errors=Join-Path $OutputDir 'runtime-check.stderr.log'
        $process=Start-Process -FilePath (Join-Path $OutputDir 'day20_smart_monitor.exe') -ArgumentList '--runtime-check' -WorkingDirectory $OutputDir -WindowStyle Hidden -PassThru -RedirectStandardOutput $output -RedirectStandardError $errors
        if(!$process.WaitForExit(20000)){$process.Kill();throw 'Runtime probe timeout'}
        if($process.ExitCode -ne 0){throw "Runtime probe failed: $($process.ExitCode); see $errors"}
        $text=(Get-Content -LiteralPath $output,$errors -Raw) -join "`n"
        if($text -notmatch 'RUNTIME_DRIVERS_OK'){throw 'Probe did not confirm deployed SQL plugins'}
        Write-Output "PASS: clean environment process started, loaded SQL drivers, and exited without starting monitoring"
    } finally {foreach($name in $names){[Environment]::SetEnvironmentVariable($name,$saved[$name],'Process')}}
}
Write-Output "PASS: runtime dependency files are present in $OutputDir (this does not replace an encoding test)"
