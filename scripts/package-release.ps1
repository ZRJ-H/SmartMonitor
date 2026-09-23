param(
    [string]$QtRoot='D:/Develop/QT6/6.11.2/mingw_64',
    [string]$MinGWRoot='D:/Develop/QT6/Tools/mingw1310_64',
    [string]$OpenCVBin='E:/opencv/install/x64/mingw/bin',
    [string]$MySQLRuntime='', [switch]$SkipBuild
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(!$MySQLRuntime){$MySQLRuntime=Join-Path $root 'bin'}
if(!$SkipBuild){& "$PSScriptRoot/build-and-test.ps1" -QtRoot $QtRoot -MinGWRoot $MinGWRoot -OpenCVBin $OpenCVBin}
$report=Get-Content "$root/artifacts/verification/latest.json" -Raw | ConvertFrom-Json
$expected=@(Get-ChildItem "$root/tests/*_tests.pro").Count+1
if($report.status -ne 'passed' -or $report.results.Count -ne $expected){throw 'Full build/test verification required before packaging'}
if(!$report.sourceFingerprint -or (& "$PSScriptRoot/source-fingerprint.ps1" -Root $root) -ne $report.sourceFingerprint){throw 'Source differs from verified build; rerun full verification'}
$app=@($report.results | Where-Object {$_.project -eq 'day15_pro.pro'})
if($app.Count -ne 1){throw 'Verified application missing'}
$source=$app[0].executable
if((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $app[0].sha256){throw 'Executable differs from verified binary'}
$target=Join-Path $root ('artifacts/releases/SmartMonitor-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $target | Out-Null
Copy-Item -LiteralPath $source -Destination $target
$commit = & git -C $root rev-parse HEAD
$savedPath=$env:PATH
try{
    $env:PATH="$MinGWRoot/bin;$QtRoot/bin;$env:SystemRoot/System32"
    & "$QtRoot/bin/windeployqt.exe" --release --compiler-runtime --no-translations "$target/day20_smart_monitor.exe" *> "$target/deployment.log"
    if($LASTEXITCODE -ne 0){throw 'windeployqt failed'}
    foreach($dll in @('libopencv_world4140.dll','opencv_videoio_ffmpeg4140_64.dll')){
        Copy-Item -LiteralPath "$OpenCVBin/$dll" -Destination $target
    }
    New-Item -ItemType Directory -Force "$target/sqldrivers" | Out-Null
    Copy-Item -LiteralPath "$MySQLRuntime/sqldrivers/qsqlmysql.dll" -Destination "$target/sqldrivers"
    Copy-Item -LiteralPath "$MySQLRuntime/libmysql.dll" -Destination $target
    Copy-Item -LiteralPath "$root/database","$root/scripts" -Destination $target -Recurse
    if(Test-Path "$root/docs/DEVELOPMENT.md"){Copy-Item "$root/docs/DEVELOPMENT.md" "$target/README.md"}
    @('[Paths]','Plugins=.') | Set-Content -Encoding ascii "$target/qt.conf"
    $files=Get-ChildItem -LiteralPath $target -Recurse -File | ForEach-Object {
        @{ path=$_.FullName.Substring($target.Length+1); sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
    }
    @{commit=$report.gitCommit; dirty=$report.dirty; sourceFingerprint=$report.sourceFingerprint; verificationRun=$report.runId; files=$files} |
        ConvertTo-Json -Depth 5 | Set-Content -Encoding utf8 "$target/manifest.json"
    Set-Content -Encoding utf8 "$root/artifacts/releases/latest.txt" $target
    Write-Output $target
}finally{$env:PATH=$savedPath}
