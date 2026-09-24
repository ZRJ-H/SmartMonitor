param(
    [string]$OutputDir = (Join-Path (Split-Path -Parent $PSScriptRoot) 'bin'),
    [string]$QtRoot = 'D:/Develop/QT6/6.11.2/mingw_64',
    [string]$MinGWRoot = 'D:/Develop/QT6/Tools/mingw1310_64',
    [string]$OpenCVBin = 'E:/opencv/install/x64/mingw/bin',
    [string]$MySQLRuntime = '',
    [switch]$CleanStart
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
if (!$MySQLRuntime) { $MySQLRuntime = Join-Path $root 'bin' }
$exe = Join-Path $OutputDir 'day20_smart_monitor.exe'
if (!(Test-Path -LiteralPath $exe)) { throw "Application missing: $exe" }
$savedPath = $env:PATH
try {
    $env:PATH = "$MinGWRoot/bin;$QtRoot/bin;$env:SystemRoot/System32;$env:SystemRoot"
    & "$QtRoot/bin/windeployqt.exe" --release --compiler-runtime --no-translations $exe *> "$OutputDir/deployment.log"
    if ($LASTEXITCODE -ne 0) { throw "windeployqt failed; see $OutputDir/deployment.log" }
    foreach ($dll in @('libopencv_world4140.dll','opencv_videoio_ffmpeg4140_64.dll')) {
        Copy-Item -LiteralPath "$OpenCVBin/$dll" -Destination $OutputDir -Force
    }
    # windeployqt does not discover the custom MySQL plugin or offscreen test platform.
    New-Item -ItemType Directory -Force "$OutputDir/sqldrivers","$OutputDir/platforms" | Out-Null
    foreach ($relative in @('sqldrivers/qsqlmysql.dll','libmysql.dll')) {
        $from = [IO.Path]::GetFullPath((Join-Path $MySQLRuntime $relative))
        $to = [IO.Path]::GetFullPath((Join-Path $OutputDir $relative))
        if (!(Test-Path -LiteralPath $from)) { throw "MySQL runtime missing: $from" }
        if ($from -ne $to) { Copy-Item -LiteralPath $from -Destination $to -Force }
    }
    Copy-Item -LiteralPath "$QtRoot/plugins/platforms/qoffscreen.dll" -Destination "$OutputDir/platforms" -Force
    @('[Paths]','Prefix=.','Plugins=.') | Set-Content -LiteralPath "$OutputDir/qt.conf" -Encoding ascii
    & "$root/tests/verify_runtime_dependencies.ps1" -OutputDir $OutputDir -CleanStart:$CleanStart
} finally { $env:PATH = $savedPath }
